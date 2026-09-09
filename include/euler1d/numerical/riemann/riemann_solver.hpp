#pragma once

#include <string>
#include <vector>

#include "euler1d/physics/ideal_gas.hpp"
#include "euler1d/physics/state.hpp"

namespace euler1d {

/// Estimacion de velocidades de senal de Davis (Toro sec. 10.5.1):
/// cotas directas a partir de las velocidades caracteristicas de los dos
/// estados, sin promedio de Roe.
struct WaveSpeeds {
    double left{};
    double right{};
};

inline WaveSpeeds davisWaveSpeeds(const IdealGas& gas,
                                  const PrimitiveState& q_left,
                                  const PrimitiveState& q_right) {
    const double a_left = gas.soundSpeed(q_left);
    const double a_right = gas.soundSpeed(q_right);
    return {std::min(q_left.u - a_left, q_right.u - a_right),
            std::max(q_left.u + a_left, q_right.u + a_right)};
}

/// Interfaz de solver de Riemann aproximado para el avance temporal.
///
/// Hay dos niveles a proposito:
///   - `flux(...)`  resuelve UNA cara. Es lo que se usa en los tests y lo que
///     hace legible la formula.
///   - `computeFluxes(...)` resuelve un BARRIDO completo. Es el punto de
///     entrada del solver de volumenes finitos: una unica llamada virtual por
///     paso de Runge-Kutta, y dentro un bucle plano sin indireccion. Anadir
///     HLLE o Roe no exige tocar el nucleo del metodo.
class RiemannSolver {
public:
    virtual ~RiemannSolver() = default;

    virtual FluxVector flux(const PrimitiveState& q_left, const PrimitiveState& q_right) const = 0;

    virtual void computeFluxes(const std::vector<PrimitiveState>& q_left,
                               const std::vector<PrimitiveState>& q_right,
                               std::vector<FluxVector>& out) const = 0;

    virtual std::string name() const = 0;
};

}  // namespace euler1d
