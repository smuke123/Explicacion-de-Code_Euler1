#include "euler1d/numerical/finite_volume_operator.hpp"

#include <utility>

namespace euler1d {

FiniteVolumeOperator::FiniteVolumeOperator(const Mesh1D& mesh, const IdealGas& gas,
                                           const Reconstruction& reconstruction,
                                           const RiemannSolver& riemann,
                                           const DivergenceOperator& divergence,
                                           const BoundaryCondition& boundary,
                                           std::vector<const SourceTerm*> sources,
                                           HydrostaticContext hydrostatic)
    : mesh_(&mesh),
      gas_(gas),
      reconstruction_(&reconstruction),
      riemann_(&riemann),
      divergence_(&divergence),
      boundary_(&boundary),
      sources_(std::move(sources)),
      hydrostatic_(hydrostatic) {
    // N+1 caras (k = 0..N). Unica reserva de memoria del solver: el ciclo
    // temporal no vuelve a asignar.
    const std::size_t faces = static_cast<std::size_t>(mesh.cellCount()) + 1;
    q_left_.resize(faces);
    q_right_.resize(faces);
    fluxes_.resize(faces);
}

void FiniteVolumeOperator::applyUpdate(const ConservedField& u_in, const PrimitiveField& w,
                                       double dt, ConservedField& u_out) {
    reconstruction_->computeInterfaceStates(w, hydrostatic_, q_left_, q_right_);

    // Positividad de los estados de cara. Es la proteccion prevista por el
    // algoritmo (una pendiente limitada puede sobrepasar el cero cerca de un
    // choque fuerte), no un parche para ocultar un estado invalido: se
    // contabiliza aparte de los pisos de la conversion a primitivas.
    for (std::size_t i = 0; i < q_left_.size(); ++i) {
        gas_.applyFloors(q_left_[i]);
        gas_.applyFloors(q_right_[i]);
    }

    riemann_->computeFluxes(q_left_, q_right_, fluxes_);
    divergence_->apply(u_in, w, fluxes_, dt, u_out);
    for (const SourceTerm* source : sources_) {
        source->accumulate(w, dt, u_out);
    }
}

void FiniteVolumeOperator::synchronize(ConservedField& u, PrimitiveField& w) {
    const int n = mesh_->cellCount();
    // Conversion celda a celda, independiente: candidato a paralelizacion.
    for (int k = 1; k <= n; ++k) {
        w[k] = gas_.toPrimitive(u[k], &floors_);
    }
    boundary_->apply(u, w);
}

}  // namespace euler1d
