#pragma once

#include <vector>

#include "euler1d/config/simulation_config.hpp"
#include "euler1d/physics/field.hpp"
#include "euler1d/physics/ideal_gas.hpp"

namespace euler1d {

/// Parametros que la reconstruccion necesita para el equilibrio hidrostatico.
struct HydrostaticContext {
    double gravity = 0.0;
    double dx = 1.0;
    bool enabled() const { return gravity != 0.0; }
};

/// Interfaz de reconstruccion de estados de cara.
///
/// El contrato es por BARRIDO COMPLETO, no por celda: se hace una sola
/// llamada virtual y dentro corre un bucle plano. Asi se puede cambiar de
/// reconstruccion por configuracion sin pagar una indireccion por celda.
///
/// Salida: `left[k]` y `right[k]` son los estados a ambos lados de la cara
/// k+1/2, con k = 0..N (N+1 caras).
class Reconstruction {
public:
    virtual ~Reconstruction() = default;

    virtual void computeInterfaceStates(const PrimitiveField& w,
                                        const HydrostaticContext& hydrostatic,
                                        std::vector<PrimitiveState>& left,
                                        std::vector<PrimitiveState>& right) const = 0;

    virtual std::string name() const = 0;
};

/// MUSCL de segundo orden con limitador de pendiente.
///
/// Con `LimiterKind::FirstOrder` degenera exactamente en Godunov de primer
/// orden (pendiente nula), igual que la opcion 'fo' del Fortran.
class MusclReconstruction final : public Reconstruction {
public:
    explicit MusclReconstruction(LimiterKind limiter) : limiter_(limiter) {}

    void computeInterfaceStates(const PrimitiveField& w,
                                const HydrostaticContext& hydrostatic,
                                std::vector<PrimitiveState>& left,
                                std::vector<PrimitiveState>& right) const override;

    std::string name() const override;

    LimiterKind limiter() const { return limiter_; }

private:
    LimiterKind limiter_;
};

}  // namespace euler1d
