#pragma once

#include "euler1d/numerical/riemann/riemann_solver.hpp"

namespace euler1d {

/// Flujo HLLC (Toro sec. 10.4): anade la onda de contacto que HLL promedia.
///
/// La velocidad del contacto S* y los estados estrella siguen literalmente
/// las ecuaciones (10.37)-(10.39) tal como estan escritas en `riemann_flux`
/// del Fortran, incluida la forma exacta de la energia estrella
///   U*_E = coef * ( E_K/rho_K + (S* - u_K) (S* + p_K/(rho_K (S_K - u_K))) ).
inline FluxVector hllcFlux(const IdealGas& gas,
                           const PrimitiveState& q_left,
                           const PrimitiveState& q_right) {
    const WaveSpeeds s = davisWaveSpeeds(gas, q_left, q_right);
    const FluxVector f_left = gas.physicalFlux(q_left);
    const FluxVector f_right = gas.physicalFlux(q_right);

    if (s.left >= 0.0) return f_left;
    if (s.right <= 0.0) return f_right;

    const ConservedState u_left = gas.toConserved(q_left);
    const ConservedState u_right = gas.toConserved(q_right);

    const double s_star =
        (q_right.p - q_left.p + q_left.rho * q_left.u * (s.left - q_left.u) -
         q_right.rho * q_right.u * (s.right - q_right.u)) /
        (q_left.rho * (s.left - q_left.u) - q_right.rho * (s.right - q_right.u));

    if (s_star >= 0.0) {
        const double coef = q_left.rho * (s.left - q_left.u) / (s.left - s_star);
        const ConservedState u_star{
            coef, coef * s_star,
            coef * (u_left.energy / q_left.rho +
                    (s_star - q_left.u) *
                        (s_star + q_left.p / (q_left.rho * (s.left - q_left.u))))};
        return {f_left.mass + s.left * (u_star.rho - u_left.rho),
                f_left.momentum + s.left * (u_star.momentum - u_left.momentum),
                f_left.energy + s.left * (u_star.energy - u_left.energy)};
    }

    const double coef = q_right.rho * (s.right - q_right.u) / (s.right - s_star);
    const ConservedState u_star{
        coef, coef * s_star,
        coef * (u_right.energy / q_right.rho +
                (s_star - q_right.u) *
                    (s_star + q_right.p / (q_right.rho * (s.right - q_right.u))))};
    return {f_right.mass + s.right * (u_star.rho - u_right.rho),
            f_right.momentum + s.right * (u_star.momentum - u_right.momentum),
            f_right.energy + s.right * (u_star.energy - u_right.energy)};
}

class HllcRiemannSolver final : public RiemannSolver {
public:
    explicit HllcRiemannSolver(const IdealGas& gas) : gas_(gas) {}

    FluxVector flux(const PrimitiveState& q_left, const PrimitiveState& q_right) const override {
        return hllcFlux(gas_, q_left, q_right);
    }

    void computeFluxes(const std::vector<PrimitiveState>& q_left,
                       const std::vector<PrimitiveState>& q_right,
                       std::vector<FluxVector>& out) const override {
        const std::size_t n = q_left.size();
        // Candidato a `#pragma omp parallel for`: caras independientes.
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = hllcFlux(gas_, q_left[i], q_right[i]);
        }
    }

    std::string name() const override { return "hllc"; }

private:
    IdealGas gas_;
};

}  // namespace euler1d
