#include "euler1d/numerical/time/time_integrator.hpp"

namespace euler1d {

void ForwardEuler::advance(SimulationState& state, double dt, SemiDiscreteOperator& op) {
    op.applyUpdate(state.conserved, state.primitive, dt, scratch_);
    const int n = op.cellCount();
    for (int k = 1; k <= n; ++k) {
        state.conserved[k] = scratch_[k];
    }
    op.synchronize(state.conserved, state.primitive);
}

void SspRk2::advance(SimulationState& state, double dt, SemiDiscreteOperator& op) {
    // Etapa 1. Al entrar se cumple la invariante primitive == EOS(conserved),
    // que es justo lo que `applyUpdate` necesita para reconstruir.
    op.applyUpdate(state.conserved, state.primitive, dt, stage1_);

    // Resincronizar antes de la segunda etapa. En el Fortran esto eran dos
    // llamadas sueltas (`cons2prim_all` + `boundary_of`) sobre un array global
    // W; olvidarlas habria evaluado L en el estado equivocado sin ningun aviso.
    op.synchronize(stage1_, state.primitive);

    // Etapa 2 con array destino SEPARADO. Una version anterior del Fortran
    // llamaba `rhs_fv(U1, U1)`, el mismo array como intent(in) e intent(out):
    // aliasing ilegal que funcionaba por casualidad del compilador. El propio
    // original ya lo dejo anotado y corregido; aqui la separacion es
    // estructural, no una convencion que se pueda olvidar.
    op.applyUpdate(stage1_, state.primitive, dt, stage2_);

    const int n = op.cellCount();
    for (int k = 1; k <= n; ++k) {
        state.conserved[k] = 0.5 * (state.conserved[k] + stage2_[k]);
    }
    op.synchronize(state.conserved, state.primitive);
}

}  // namespace euler1d
