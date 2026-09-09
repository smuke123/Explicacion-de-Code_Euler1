#pragma once

#include "euler1d/physics/field.hpp"

namespace euler1d {

/// Operador semidiscreto que el integrador temporal aplica.
///
/// El contrato explicita una dependencia que en el Fortran era IMPLICITA y
/// peligrosa: `rhs_fv` reconstruia leyendo el array GLOBAL `W`, no el `Uin`
/// que recibia. Aqui `w` es un parametro, de modo que la obligacion "sincroniza
/// las primitivas con el estado que vas a evolucionar" es visible en la firma
/// y no se puede olvidar.
class SemiDiscreteOperator {
public:
    virtual ~SemiDiscreteOperator() = default;

    /// u_out = u_in + dt * L(u_in), evaluando L con las primitivas `w`.
    /// Solo escribe celdas fisicas 1..N.
    virtual void applyUpdate(const ConservedField& u_in,
                             const PrimitiveField& w,
                             double dt,
                             ConservedField& u_out) = 0;

    /// Restablece la invariante: w <- EOS(u) en 1..N y fantasmas de ambos.
    /// Equivale a `cons2prim_all` seguido de `boundary`.
    virtual void synchronize(ConservedField& u, PrimitiveField& w) = 0;

    virtual int cellCount() const = 0;
};

}  // namespace euler1d
