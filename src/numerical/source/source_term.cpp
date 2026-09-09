#include "euler1d/numerical/source/source_term.hpp"

namespace euler1d {
namespace {

/// Potencia entera por exponenciacion binaria.
///
/// Se evita `std::pow(double, int)`, que en glibc pasa por la version en
/// coma flotante y puede diferir en el ultimo bit. gfortran compila
/// `r**alpha` con exponente entero mediante `__builtin_powi`, que es
/// exactamente esta secuencia de multiplicaciones.
inline double ipow(double base, int exponent) {
    double result = 1.0;
    double factor = base;
    int e = exponent;
    while (e > 0) {
        if (e & 1) result *= factor;
        e >>= 1;
        if (e > 0) factor *= factor;
    }
    return result;
}

}  // namespace

void CartesianDivergence::apply(const ConservedField& u_in, const PrimitiveField&,
                                const std::vector<FluxVector>& fluxes, double dt,
                                ConservedField& u_out) const {
    const int n = u_in.cellCount();
    // El Fortran escribe `dt/dx*(...)`: el cociente se forma primero. Se
    // conserva el orden para no introducir una diferencia de redondeo.
    const double lambda = dt / dx_;
    // Celdas independientes: candidato a `#pragma omp parallel for`.
    for (int k = 1; k <= n; ++k) {
        const FluxVector& f_right = fluxes[static_cast<std::size_t>(k)];
        const FluxVector& f_left = fluxes[static_cast<std::size_t>(k - 1)];
        u_out[k].rho = u_in[k].rho - lambda * (f_right.mass - f_left.mass);
        u_out[k].momentum = u_in[k].momentum - lambda * (f_right.momentum - f_left.momentum);
        u_out[k].energy = u_in[k].energy - lambda * (f_right.energy - f_left.energy);
    }
}

void RadialDivergence::apply(const ConservedField& u_in, const PrimitiveField& w,
                             const std::vector<FluxVector>& fluxes, double dt,
                             ConservedField& u_out) const {
    const int n = u_in.cellCount();
    const double dx = mesh_->dx();
    for (int k = 1; k <= n; ++k) {
        const double r_plus = mesh_->center(k) + 0.5 * dx;
        const double r_minus = mesh_->center(k) - 0.5 * dx;
        const double area_plus = ipow(r_plus, alpha_);
        const double area_minus = ipow(r_minus, alpha_);
        const double volume =
            (ipow(r_plus, alpha_ + 1) - ipow(r_minus, alpha_ + 1)) / static_cast<double>(alpha_ + 1);

        const FluxVector& f_right = fluxes[static_cast<std::size_t>(k)];
        const FluxVector& f_left = fluxes[static_cast<std::size_t>(k - 1)];
        u_out[k].rho = u_in[k].rho - dt * (area_plus * f_right.mass - area_minus * f_left.mass) / volume;
        u_out[k].momentum =
            u_in[k].momentum -
            dt * (area_plus * f_right.momentum - area_minus * f_left.momentum) / volume;
        u_out[k].energy =
            u_in[k].energy - dt * (area_plus * f_right.energy - area_minus * f_left.energy) / volume;

        // Termino geometrico de presion, BIEN BALANCEADO: cancela exactamente
        // la divergencia de una presion uniforme, de modo que un ambiente en
        // reposo no se acelera solo por la curvatura de la malla.
        u_out[k].momentum += dt * w[k].p * (area_plus - area_minus) / volume;
    }
}

void GravitySource::accumulate(const PrimitiveField& w, double dt, ConservedField& u_out) const {
    const int n = w.cellCount();
    // El trabajo de la gravedad usa la velocidad del INICIO del subpaso, no la
    // recien actualizada. Es lo que hace el Fortran y se conserva: cambiarlo
    // alteraria el resultado en el ultimo digito y rompe la equivalencia.
    for (int k = 1; k <= n; ++k) {
        u_out[k].momentum -= dt * w[k].rho * gravity_;
        u_out[k].energy -= dt * w[k].rho * gravity_ * w[k].u;
    }
}

}  // namespace euler1d
