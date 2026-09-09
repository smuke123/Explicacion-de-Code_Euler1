#pragma once

#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/physics/field.hpp"
#include "euler1d/physics/ideal_gas.hpp"

namespace euler1d {

/// Paso de tiempo adaptativo por condicion de Courant.
///
///     dt = CFL * dx / max_k ( |u_k| + c_k ),   c = sqrt(gamma p / rho)
///
/// El maximo se inicializa en 1e-30 (como el Fortran) para que un campo
/// exactamente en reposo y frio no divida por cero.
class CflController {
public:
    CflController(double cfl, const Mesh1D& mesh, const IdealGas& gas)
        : cfl_(cfl), mesh_(&mesh), gas_(gas) {}

    double computeTimeStep(const PrimitiveField& w) const;

    /// Velocidad de senal maxima del campo, expuesta para diagnostico y tests.
    double maxSignalSpeed(const PrimitiveField& w) const;

    static constexpr double kMinSignalSpeed = 1.0e-30;

private:
    /// Resultado de un unico barrido sobre el campo.
    ///
    /// `checksum` es la suma de las velocidades de senal y existe solo para
    /// detectar estados no finitos. Hace falta porque `std::max(x, NaN)`
    /// devuelve `x`: un NaN en el campo NO se propaga al maximo y la
    /// simulacion seguiria corriendo sobre basura. La suma si lo propaga, y
    /// cuesta una adicion por celda.
    struct Scan {
        double max_speed;
        double checksum;
    };
    Scan scan(const PrimitiveField& w) const;

    double cfl_;
    const Mesh1D* mesh_;
    IdealGas gas_;
};

}  // namespace euler1d
