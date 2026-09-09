#pragma once

#include <cassert>
#include <cstddef>
#include <vector>

#include "euler1d/physics/state.hpp"

namespace euler1d {

/// Numero de celdas fantasma por lado. El Fortran declara `U(3,-1:N+2)`,
/// es decir dos fantasmas a cada lado; la reconstruccion MUSCL en la cara
/// k+1/2 necesita las celdas k-1, k, k+1, k+2.
inline constexpr int kGhostCells = 2;

/// Almacenamiento contiguo de un campo de 3 componentes sobre la malla 1D.
///
/// CONVENCION DE INDICES (identica a la del Fortran para poder auditar la
/// migracion linea a linea):
///
///     k = 1 .. N        celdas fisicas
///     k = 0, -1         fantasmas izquierdos
///     k = N+1, N+2      fantasmas derechos
///
/// El slot interno es `k + 1`, de modo que k = -1 cae en el slot 0 y
/// k = N+2 en el slot N+3. El layout es AoS (las tres componentes de una
/// celda son contiguas), igual que el orden column-major de Fortran para
/// `U(3,k)`; asi se preserva el patron de acceso a memoria del original.
template <typename State>
class CellField {
public:
    CellField() = default;

    explicit CellField(int cell_count) { resize(cell_count); }

    void resize(int cell_count) {
        cell_count_ = cell_count;
        data_.assign(static_cast<std::size_t>(cell_count + 2 * kGhostCells), State{});
    }

    int cellCount() const { return cell_count_; }

    /// Indices validos: [-kGhostCells+1, N+kGhostCells] == [-1, N+2].
    State& operator[](int k) {
        assert(k >= -1 && k <= cell_count_ + 2);
        return data_[static_cast<std::size_t>(k + 1)];
    }
    const State& operator[](int k) const {
        assert(k >= -1 && k <= cell_count_ + 2);
        return data_[static_cast<std::size_t>(k + 1)];
    }

    /// Acceso crudo para bucles vectorizables sobre celdas fisicas.
    State* rawData() { return data_.data(); }
    const State* rawData() const { return data_.data(); }
    std::size_t rawSize() const { return data_.size(); }

private:
    int cell_count_ = 0;
    std::vector<State> data_;
};

using ConservedField = CellField<ConservedState>;
using PrimitiveField = CellField<PrimitiveState>;

}  // namespace euler1d
