#pragma once

namespace euler1d {

/// Variables primitivas W = (rho, u, p).
struct PrimitiveState {
    double rho{};
    double u{};
    double p{};
};

/// Variables conservadas U = (rho, rho*u, E) con E = p/(gamma-1) + rho*u^2/2.
struct ConservedState {
    double rho{};
    double momentum{};
    double energy{};
};

/// Flujo fisico F(U) = (rho*u, rho*u^2 + p, u*(E+p)).
struct FluxVector {
    double mass{};
    double momentum{};
    double energy{};
};

inline ConservedState operator-(const ConservedState& a, const ConservedState& b) {
    return {a.rho - b.rho, a.momentum - b.momentum, a.energy - b.energy};
}

inline ConservedState operator+(const ConservedState& a, const ConservedState& b) {
    return {a.rho + b.rho, a.momentum + b.momentum, a.energy + b.energy};
}

inline ConservedState operator*(double s, const ConservedState& a) {
    return {s * a.rho, s * a.momentum, s * a.energy};
}

}  // namespace euler1d
