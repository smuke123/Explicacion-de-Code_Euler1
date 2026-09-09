#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "euler1d/physics/state.hpp"

namespace euler1d {

/// Contador de activaciones de los pisos de positividad.
///
/// El Fortran aplica `max(..., floor)` en silencio dentro de `cons2prim_all`.
/// Conservamos ese comportamiento (es la proteccion prevista por el algoritmo),
/// pero lo instrumentamos: un piso que se dispara miles de veces indica que la
/// simulacion perdio validez fisica, no que este "protegida".
struct FloorCounters {
    std::uint64_t density{};
    std::uint64_t pressure{};
    /// Energia interna especifica mas negativa vista antes de aplicar el piso.
    double min_internal_energy = 0.0;

    void reset() { *this = FloorCounters{}; }
    bool triggered() const { return density > 0 || pressure > 0; }
};

/// Ecuacion de estado de gas ideal politropico y conversiones asociadas.
///
/// Es el unico lugar del codigo donde se codifica `E = p/(gamma-1) + rho u^2/2`.
class IdealGas {
public:
    IdealGas(double gamma, double density_floor, double pressure_floor)
        : gamma_(gamma), density_floor_(density_floor), pressure_floor_(pressure_floor) {}

    double gamma() const { return gamma_; }
    double densityFloor() const { return density_floor_; }
    double pressureFloor() const { return pressure_floor_; }

    /// c = sqrt(gamma p / rho)
    double soundSpeed(const PrimitiveState& w) const {
        return std::sqrt(gamma_ * w.p / w.rho);
    }

    /// W -> U. Sin pisos: replica `prim2cons_all` del Fortran.
    ConservedState toConserved(const PrimitiveState& w) const {
        return {w.rho, w.rho * w.u, totalEnergy(w)};
    }

    double totalEnergy(const PrimitiveState& w) const {
        return w.p / (gamma_ - 1.0) + 0.5 * w.rho * w.u * w.u;
    }

    /// U -> W replicando `cons2prim_all`.
    ///
    /// Orden exacto del Fortran, que importa bit a bit:
    ///   rho <- max(U1, rho_floor)      (piso ANTES de dividir)
    ///   u   <- U2 / rho                (usa la rho ya pisada)
    ///   p   <- max((g-1)(U3 - rho u^2/2), p_floor)
    PrimitiveState toPrimitive(const ConservedState& u, FloorCounters* counters = nullptr) const {
        PrimitiveState w;
        w.rho = std::max(u.rho, density_floor_);
        if (counters && u.rho < density_floor_) ++counters->density;
        w.u = u.momentum / w.rho;
        const double internal = u.energy - 0.5 * w.rho * w.u * w.u;
        const double p_raw = (gamma_ - 1.0) * internal;
        if (counters) {
            const double eint_specific = internal / w.rho;
            counters->min_internal_energy = std::min(counters->min_internal_energy, eint_specific);
            if (p_raw < pressure_floor_) ++counters->pressure;
        }
        w.p = std::max(p_raw, pressure_floor_);
        return w;
    }

    /// F(U) evaluado a partir de primitivas (como `phys_flux` del Fortran).
    FluxVector physicalFlux(const PrimitiveState& w) const {
        const double e_total = totalEnergy(w);
        return {w.rho * w.u, w.rho * w.u * w.u + w.p, w.u * (e_total + w.p)};
    }

    /// Recorta un estado reconstruido a valores positivos (pisos de cara).
    void applyFloors(PrimitiveState& w) const {
        w.rho = std::max(w.rho, density_floor_);
        w.p = std::max(w.p, pressure_floor_);
    }

private:
    double gamma_;
    double density_floor_;
    double pressure_floor_;
};

}  // namespace euler1d
