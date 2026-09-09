#pragma once

#include "euler1d/physics/field.hpp"

namespace euler1d {

/// Estado evolutivo de la simulacion.
///
/// `conserved` y `primitive` son redundantes por construccion: `primitive`
/// es la imagen de `conserved` bajo la EOS. Se mantienen ambos porque el
/// esquema original reconstruye sobre primitivas y porque recalcularlas en
/// cada acceso costaria una raiz y una division por celda. La invariante es
/// "primitive esta sincronizado con conserved", y solo la restablece
/// `SemiDiscreteOperator::synchronize`.
struct SimulationState {
    ConservedField conserved;
    PrimitiveField primitive;
    double time = 0.0;
    long long step = 0;

    explicit SimulationState(int cell_count)
        : conserved(cell_count), primitive(cell_count) {}

    int cellCount() const { return conserved.cellCount(); }
};

}  // namespace euler1d
