#include "euler1d/numerical/reconstruction/reconstruction.hpp"

#include "euler1d/numerical/reconstruction/slope_limiter.hpp"

namespace euler1d {
namespace {

/// Barrido MUSCL con el limitador fijado en tiempo de compilacion.
///
/// Reproduce el bucle `do k = 0, N / do m = 1, 3` de `rhs_fv`. La diferencia
/// respecto al Fortran es puramente de rendimiento: alli el limitador se
/// escogia con `select case (trim(limiter))` DENTRO del bucle, o sea una
/// comparacion de cadenas por celda y componente. Aqui la eleccion ocurre una
/// vez por barrido y el cuerpo queda libre de ramas.
///
/// El bucle sobre caras es independiente entre iteraciones: candidato directo
/// a `#pragma omp parallel for`.
template <typename Limiter>
void sweep(const PrimitiveField& w,
           const HydrostaticContext& hydrostatic,
           std::vector<PrimitiveState>& left,
           std::vector<PrimitiveState>& right) {
    const int n = w.cellCount();
    const bool balanced = hydrostatic.enabled();
    const double g = hydrostatic.gravity;
    const double dx = hydrostatic.dx;

    for (int k = 0; k <= n; ++k) {
        // Cara k+1/2: el estado izquierdo se extrapola desde la celda k y el
        // derecho desde la celda k+1. Las plantillas alcanzan k-1 .. k+2, que
        // es justo el motivo de tener dos fantasmas por lado.
        double slope = Limiter::slope(w[k].rho - w[k - 1].rho, w[k + 1].rho - w[k].rho);
        const double rho_left = w[k].rho + 0.5 * slope;
        slope = Limiter::slope(w[k + 1].rho - w[k].rho, w[k + 2].rho - w[k + 1].rho);
        const double rho_right = w[k + 1].rho - 0.5 * slope;

        slope = Limiter::slope(w[k].u - w[k - 1].u, w[k + 1].u - w[k].u);
        const double u_left = w[k].u + 0.5 * slope;
        slope = Limiter::slope(w[k + 1].u - w[k].u, w[k + 2].u - w[k + 1].u);
        const double u_right = w[k + 1].u - 0.5 * slope;

        double p_left;
        double p_right;
        if (balanced) {
            // RECONSTRUCCION HIDROSTATICA (Kaeppeli-Mishra). Se reconstruye la
            // DESVIACION de p respecto a la extrapolacion hidrostatica local,
            //   delta = (p_{k+-1} - p_k) +- g dx (rho_k + rho_{k+-1})/2,
            // y la cara lleva p_hidro + desviacion limitada. En el equilibrio
            // discreto trapezoidal las delta son cero, ambas caras coinciden y
            // el flujo cancela la fuente -rho g a cero maquina.
            double backward = (w[k].p - w[k - 1].p) + g * dx * 0.5 * (w[k].rho + w[k - 1].rho);
            double forward = (w[k + 1].p - w[k].p) + g * dx * 0.5 * (w[k + 1].rho + w[k].rho);
            slope = Limiter::slope(backward, forward);
            p_left = w[k].p - 0.5 * g * dx * w[k].rho + 0.5 * slope;

            backward = forward;
            forward = (w[k + 2].p - w[k + 1].p) + g * dx * 0.5 * (w[k + 2].rho + w[k + 1].rho);
            slope = Limiter::slope(backward, forward);
            p_right = w[k + 1].p + 0.5 * g * dx * w[k + 1].rho - 0.5 * slope;
        } else {
            slope = Limiter::slope(w[k].p - w[k - 1].p, w[k + 1].p - w[k].p);
            p_left = w[k].p + 0.5 * slope;
            slope = Limiter::slope(w[k + 1].p - w[k].p, w[k + 2].p - w[k + 1].p);
            p_right = w[k + 1].p - 0.5 * slope;
        }

        left[static_cast<std::size_t>(k)] = {rho_left, u_left, p_left};
        right[static_cast<std::size_t>(k)] = {rho_right, u_right, p_right};
    }
}

}  // namespace

double limitedSlope(LimiterKind kind, double a, double b) {
    switch (kind) {
        case LimiterKind::FirstOrder: return limiters::FirstOrder::slope(a, b);
        case LimiterKind::MinMod: return limiters::MinMod::slope(a, b);
        case LimiterKind::VanLeer: return limiters::VanLeer::slope(a, b);
        case LimiterKind::MonotonizedCentral: return limiters::MonotonizedCentral::slope(a, b);
        case LimiterKind::Superbee: return limiters::Superbee::slope(a, b);
    }
    return 0.0;
}

void MusclReconstruction::computeInterfaceStates(const PrimitiveField& w,
                                                 const HydrostaticContext& hydrostatic,
                                                 std::vector<PrimitiveState>& left,
                                                 std::vector<PrimitiveState>& right) const {
    switch (limiter_) {
        case LimiterKind::FirstOrder: sweep<limiters::FirstOrder>(w, hydrostatic, left, right); break;
        case LimiterKind::MinMod: sweep<limiters::MinMod>(w, hydrostatic, left, right); break;
        case LimiterKind::VanLeer: sweep<limiters::VanLeer>(w, hydrostatic, left, right); break;
        case LimiterKind::MonotonizedCentral:
            sweep<limiters::MonotonizedCentral>(w, hydrostatic, left, right);
            break;
        case LimiterKind::Superbee: sweep<limiters::Superbee>(w, hydrostatic, left, right); break;
    }
}

std::string MusclReconstruction::name() const {
    return limiter_ == LimiterKind::FirstOrder ? "godunov-1o" : "muscl-" + toString(limiter_);
}

}  // namespace euler1d
