#pragma once

#include <vector>

#include "euler1d/boundary/boundary_condition.hpp"
#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/numerical/reconstruction/reconstruction.hpp"
#include "euler1d/numerical/riemann/riemann_solver.hpp"
#include "euler1d/numerical/semi_discrete_operator.hpp"
#include "euler1d/numerical/source/source_term.hpp"
#include "euler1d/physics/ideal_gas.hpp"

namespace euler1d {

/// Solver de volumenes finitos tipo Godunov: la composicion concreta de
/// reconstruccion + solver de Riemann + divergencia + fuentes.
///
/// Corresponde a `rhs_fv` del Fortran. La secuencia por subpaso es:
///   1. reconstruir estados de cara (MUSCL con limitador),
///   2. aplicar pisos de positividad a esos estados,
///   3. resolver el Riemann de cada cara (HLL o HLLC),
///   4. ensamblar la divergencia (cartesiana o radial conservativa),
///   5. acumular los terminos fuente (gravedad).
///
/// Todos los buffers se reservan una sola vez en el constructor; el ciclo
/// temporal no hace ninguna asignacion de memoria.
class FiniteVolumeOperator final : public SemiDiscreteOperator {
public:
    FiniteVolumeOperator(const Mesh1D& mesh,
                         const IdealGas& gas,
                         const Reconstruction& reconstruction,
                         const RiemannSolver& riemann,
                         const DivergenceOperator& divergence,
                         const BoundaryCondition& boundary,
                         std::vector<const SourceTerm*> sources,
                         HydrostaticContext hydrostatic);

    void applyUpdate(const ConservedField& u_in,
                     const PrimitiveField& w,
                     double dt,
                     ConservedField& u_out) override;

    void synchronize(ConservedField& u, PrimitiveField& w) override;

    int cellCount() const override { return mesh_->cellCount(); }

    /// Diagnostico de pisos acumulado desde el inicio de la corrida.
    const FloorCounters& floorCounters() const { return floors_; }
    void resetFloorCounters() { floors_.reset(); }

    /// Estados de cara del ultimo barrido (para tests e inspeccion).
    const std::vector<PrimitiveState>& faceLeftStates() const { return q_left_; }
    const std::vector<PrimitiveState>& faceRightStates() const { return q_right_; }
    const std::vector<FluxVector>& faceFluxes() const { return fluxes_; }

private:
    const Mesh1D* mesh_;
    IdealGas gas_;
    const Reconstruction* reconstruction_;
    const RiemannSolver* riemann_;
    const DivergenceOperator* divergence_;
    const BoundaryCondition* boundary_;
    std::vector<const SourceTerm*> sources_;
    HydrostaticContext hydrostatic_;

    std::vector<PrimitiveState> q_left_;
    std::vector<PrimitiveState> q_right_;
    std::vector<FluxVector> fluxes_;
    FloorCounters floors_;
};

}  // namespace euler1d
