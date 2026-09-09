#pragma once

#include <string>

#include "euler1d/physics/field.hpp"
#include "euler1d/physics/ideal_gas.hpp"

namespace euler1d {

/// Actualiza las celdas fantasma.
///
/// Contrato heredado del Fortran (`boundary_of`) y que hay que respetar:
/// la condicion de frontera escribe TANTO las conservadas COMO las
/// primitivas de los fantasmas. La reconstruccion lee las primitivas, asi
/// que dejar `w` sin rellenar produciria basura en las caras extremas.
class BoundaryCondition {
public:
    virtual ~BoundaryCondition() = default;

    virtual void apply(ConservedField& u, PrimitiveField& w) const = 0;

    virtual std::string name() const = 0;
};

/// Gradiente cero: las ondas salen del dominio.
///
/// Nota: el Fortran copia la celda de borde en LOS DOS fantasmas
/// (`Uc(:,-1) = Uc(:,1)`, no `Uc(:,2)`), lo que da extrapolacion constante.
class TransmissiveBoundary final : public BoundaryCondition {
public:
    void apply(ConservedField& u, PrimitiveField& w) const override;
    std::string name() const override { return "trans"; }
};

class PeriodicBoundary final : public BoundaryCondition {
public:
    void apply(ConservedField& u, PrimitiveField& w) const override;
    std::string name() const override { return "per"; }
};

/// Pared solida: espejo en rho y p, antisimetria en u.
///
/// Con gravedad, la presion de los fantasmas NO se refleja: continua el
/// equilibrio hidrostatico discreto trapezoidal
///     p_{ghost} = p_{interior} +/- g dx (rho_ghost + rho_interior)/2,
/// porque con el espejo plano (dp = 0) las desviaciones hidrostaticas no se
/// cancelan en las caras de pared y el reposo genera |u| ~ 1e-3.
class ReflectiveBoundary final : public BoundaryCondition {
public:
    ReflectiveBoundary(const IdealGas& gas, double gravity, double dx)
        : gas_(gas), gravity_(gravity), dx_(dx) {}

    void apply(ConservedField& u, PrimitiveField& w) const override;
    std::string name() const override { return "refl"; }

private:
    IdealGas gas_;
    double gravity_;
    double dx_;
};

}  // namespace euler1d
