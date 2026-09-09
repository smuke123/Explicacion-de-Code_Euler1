#pragma once

#include <string>

#include "euler1d/numerical/semi_discrete_operator.hpp"
#include "euler1d/simulation/simulation_state.hpp"

namespace euler1d {

/// Integrador temporal. Es independiente del operador espacial: recibe
/// cualquier `SemiDiscreteOperator`, asi que anadir RK3 o RK4 no obliga a
/// tocar el solver de volumenes finitos.
class TimeIntegrator {
public:
    virtual ~TimeIntegrator() = default;

    /// Avanza el estado un paso completo de tamano dt. Al volver, `state`
    /// cumple la invariante primitive == EOS(conserved) con fantasmas puestos.
    virtual void advance(SimulationState& state, double dt, SemiDiscreteOperator& op) = 0;

    virtual std::string name() const = 0;
};

/// Euler explicito de primer orden. No lo usa el Fortran; se incluye para
/// comprobar que la interfaz del integrador es realmente intercambiable.
class ForwardEuler final : public TimeIntegrator {
public:
    explicit ForwardEuler(int cell_count) : scratch_(cell_count) {}

    void advance(SimulationState& state, double dt, SemiDiscreteOperator& op) override;
    std::string name() const override { return "euler"; }

private:
    ConservedField scratch_;
};

/// SSP-RK2 (Heun), el integrador del Fortran:
///     U1   = U  + dt L(U)
///     U2   = U1 + dt L(U1)
///     U^{n+1} = (U + U2) / 2
///
/// Los dos arrays destino son SEPARADOS. El Fortran original llamaba
/// `rhs_fv(U1, U1)`, es decir el mismo array como `intent(in)` e
/// `intent(out)`: aliasing ilegal en Fortran y comportamiento indefinido.
class SspRk2 final : public TimeIntegrator {
public:
    explicit SspRk2(int cell_count) : stage1_(cell_count), stage2_(cell_count) {}

    void advance(SimulationState& state, double dt, SemiDiscreteOperator& op) override;
    std::string name() const override { return "ssp-rk2"; }

private:
    ConservedField stage1_;
    ConservedField stage2_;
};

}  // namespace euler1d
