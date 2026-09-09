#pragma once

#include <string>

#include "euler1d/physics/state.hpp"

namespace euler1d {

/// Region estrella de un problema de Riemann para gas ideal.
struct StarState {
    double pressure{};
    double velocity{};
    /// El criterio de vacio (Toro ec. 4.40) se disparo: no hay region
    /// estrella, entre las dos rarefacciones queda vacio. Se senala con
    /// `pressure == 0`, igual que el Fortran.
    bool vacuum = false;
    /// Residuo |f(p*)| del Newton. El Fortran solo lo imprime por pantalla;
    /// aqui se devuelve para que los tests puedan comprobarlo.
    double residual{};
    bool converged = true;
    int iterations{};
};

/// Solver de Riemann EXACTO (Toro cap. 4).
///
/// NO participa en el avance temporal. Es la referencia semianalitica con la
/// que se validan HLL y HLLC y con la que se escriben los archivos
/// `*_exact.xl`. La separacion es deliberada: el mismo objeto se usa en los
/// tests unitarios contra la Tabla 4.3 de Toro.
class ExactRiemannSolver {
public:
    explicit ExactRiemannSolver(double gamma) : gamma_(gamma) {}

    double gamma() const { return gamma_; }

    /// f_K(p) y su derivada (Toro ecs. 4.6-4.8 y 4.37): rama de choque si
    /// p > p_K (Rankine-Hugoniot) y de rarefaccion si no (invariante de
    /// Riemann).
    void pressureFunction(double p, double rho_k, double p_k, double& f, double& df) const;

    /// Resuelve p* y u* con Newton-Raphson sobre
    ///     f(p) = f_L(p) + f_R(p) + (u_R - u_L) = 0,
    /// con semilla de dos rarefacciones (TRRS, ec. 4.46).
    StarState solveStar(const PrimitiveState& left, const PrimitiveState& right) const;

    /// Muestrea la solucion autosimilar W(s) con s = x/t (Toro sec. 4.5),
    /// incluyendo el interior de los abanicos y la region de vacio (sec. 4.6).
    PrimitiveState sample(const PrimitiveState& left,
                          const PrimitiveState& right,
                          const StarState& star,
                          double s) const;

    /// Tolerancia del residuo por debajo de la cual se considera convergido.
    static constexpr double kResidualTolerance = 1.0e-10;
    /// Tolerancia relativa de parada del Newton (`tol` del Fortran).
    static constexpr double kNewtonTolerance = 1.0e-14;
    static constexpr int kMaxIterations = 200;

private:
    double gamma_;
};

}  // namespace euler1d
