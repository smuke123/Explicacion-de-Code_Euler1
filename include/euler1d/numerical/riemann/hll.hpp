#pragma once

#include "euler1d/numerical/riemann/riemann_solver.hpp"

namespace euler1d {

/// Flujo HLL (Harten-Lax-van Leer, Toro sec. 10.3) con velocidades de Davis.
///
/// El orden de las operaciones replica `riemann_flux` del Fortran: se evaluan
/// primero las velocidades de senal y los flujos/estados de ambos lados, y
/// solo despues se decide la rama. Mantenerlo asi evita diferencias de
/// redondeo frente al original.
inline FluxVector hllFlux(const IdealGas& gas,
                          const PrimitiveState& q_left,
                          const PrimitiveState& q_right) {
    const WaveSpeeds s = davisWaveSpeeds(gas, q_left, q_right);
    const FluxVector f_left = gas.physicalFlux(q_left);
    const FluxVector f_right = gas.physicalFlux(q_right);

    if (s.left >= 0.0) return f_left;
    if (s.right <= 0.0) return f_right;

    const ConservedState u_left = gas.toConserved(q_left);
    const ConservedState u_right = gas.toConserved(q_right);
    // Division explicita, no multiplicacion por el reciproco: el Fortran
    // divide y el reciproco cambiaria el ultimo bit del resultado.
    const double den = s.right - s.left;
    return {(s.right * f_left.mass - s.left * f_right.mass +
             s.left * s.right * (u_right.rho - u_left.rho)) /
                den,
            (s.right * f_left.momentum - s.left * f_right.momentum +
             s.left * s.right * (u_right.momentum - u_left.momentum)) /
                den,
            (s.right * f_left.energy - s.left * f_right.energy +
             s.left * s.right * (u_right.energy - u_left.energy)) /
                den};
}

class HllRiemannSolver final : public RiemannSolver {
public:
    explicit HllRiemannSolver(const IdealGas& gas) : gas_(gas) {}

    FluxVector flux(const PrimitiveState& q_left, const PrimitiveState& q_right) const override {
        return hllFlux(gas_, q_left, q_right);
    }

    void computeFluxes(const std::vector<PrimitiveState>& q_left,
                       const std::vector<PrimitiveState>& q_right,
                       std::vector<FluxVector>& out) const override {
        const std::size_t n = q_left.size();
        // Candidato a `#pragma omp parallel for`: caras independientes.
        for (std::size_t i = 0; i < n; ++i) {
            out[i] = hllFlux(gas_, q_left[i], q_right[i]);
        }
    }

    std::string name() const override { return "hll"; }

private:
    IdealGas gas_;
};

}  // namespace euler1d
