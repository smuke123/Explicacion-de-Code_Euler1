#include "euler1d/numerical/riemann/exact_riemann_solver.hpp"

#include <algorithm>
#include <cmath>

// Solver de Riemann exacto (Toro, cap. 4). Traduccion fiel de
// `exact_riemann.f90`, incluidos el orden de las operaciones y las
// tolerancias, porque este modulo es la referencia contra la que se mide la
// migracion completa.

namespace euler1d {

void ExactRiemannSolver::pressureFunction(double p, double rho_k, double p_k, double& f,
                                          double& df) const {
    const double g = gamma_;
    const double c_k = std::sqrt(g * p_k / rho_k);
    if (p > p_k) {
        // Rama de choque: relaciones de Rankine-Hugoniot (ecs. 4.6-4.8).
        const double a_k = 2.0 / ((g + 1.0) * rho_k);
        const double b_k = (g - 1.0) / (g + 1.0) * p_k;
        const double sq = std::sqrt(a_k / (p + b_k));
        f = (p - p_k) * sq;
        df = sq * (1.0 - 0.5 * (p - p_k) / (p + b_k));
    } else {
        // Rama de rarefaccion: invariante de Riemann (ec. 4.37).
        f = 2.0 * c_k / (g - 1.0) * (std::pow(p / p_k, (g - 1.0) / (2.0 * g)) - 1.0);
        df = (1.0 / (rho_k * c_k)) * std::pow(p / p_k, -(g + 1.0) / (2.0 * g));
    }
}

StarState ExactRiemannSolver::solveStar(const PrimitiveState& left,
                                        const PrimitiveState& right) const {
    const double g = gamma_;
    const double a_left = std::sqrt(g * left.p / left.rho);
    const double a_right = std::sqrt(g * right.p / right.rho);
    const double du = right.u - left.u;

    StarState star;

    // Criterio de generacion de vacio (ec. 4.40). Si se cumple, las dos
    // rarefacciones no llegan a comunicarse y no existe presion estrella
    // positiva. Se senala con pressure = 0, igual que el Fortran, y el
    // muestreo toma la rama de vacio.
    if (2.0 * (a_left + a_right) / (g - 1.0) <= du) {
        star.pressure = 0.0;
        star.velocity = 0.5 * (left.u + right.u);
        star.vacuum = true;
        star.residual = 0.0;
        star.converged = true;
        star.iterations = 0;
        return star;
    }

    // Semilla de dos rarefacciones (TRRS, ec. 4.46): positiva por
    // construccion y robusta cerca del vacio, que es donde una semilla
    // aritmetica como (pL+pR)/2 falla (test 2 de Toro).
    const double z = (g - 1.0) / (2.0 * g);
    double p = std::pow((a_left + a_right - 0.5 * (g - 1.0) * du) /
                            (a_left / std::pow(left.p, z) + a_right / std::pow(right.p, z)),
                        1.0 / z);
    p = std::max(p, 1.0e-14);

    double f_left = 0.0;
    double f_right = 0.0;
    double df_left = 0.0;
    double df_right = 0.0;
    int iterations = 0;

    for (int it = 1; it <= kMaxIterations; ++it) {
        iterations = it;
        const double p_old = p;
        pressureFunction(p, left.rho, left.p, f_left, df_left);
        pressureFunction(p, right.rho, right.p, f_right, df_right);
        p = p - (f_left + f_right + du) / (df_left + df_right);
        // Salvaguarda del Fortran: si Newton se sale a presion no fisica, se
        // biseca hacia 0+ en lugar de abortar.
        if (p <= 0.0) p = 0.5 * p_old;
        if (2.0 * std::abs(p - p_old) / (p + p_old) < kNewtonTolerance) break;
    }

    star.pressure = p;
    pressureFunction(p, left.rho, left.p, f_left, df_left);
    pressureFunction(p, right.rho, right.p, f_right, df_right);
    star.velocity = 0.5 * (left.u + right.u) + 0.5 * (f_right - f_left);
    star.iterations = iterations;
    star.residual = std::abs(f_left + f_right + du);
    star.converged = star.residual <= kResidualTolerance * std::max(1.0, std::abs(star.pressure));
    return star;
}

PrimitiveState ExactRiemannSolver::sample(const PrimitiveState& left, const PrimitiveState& right,
                                          const StarState& star, double s) const {
    const double g = gamma_;
    const double a_left = std::sqrt(g * left.p / left.rho);
    const double a_right = std::sqrt(g * right.p / right.rho);
    const double gm = (g - 1.0) / (g + 1.0);
    const double gp = (g + 1.0) / (2.0 * g);

    PrimitiveState out{};

    if (star.pressure <= 0.0) {
        // REGION DE VACIO (Toro sec. 4.6). Sin esta rama, el muestreo cae en
        // la formula del interior del abanico con base negativa y devuelve NaN
        // en rho y p. Los cinco tests de Toro no la disparan, pero cualquier
        // condicion inicial mas violenta si.
        const double head_left = left.u - a_left;
        const double front_left = left.u + 2.0 * a_left / (g - 1.0);
        const double front_right = right.u - 2.0 * a_right / (g - 1.0);
        const double head_right = right.u + a_right;
        if (s <= head_left) {
            out = left;
        } else if (s < front_left) {
            out.u = (2.0 / (g + 1.0)) * (a_left + 0.5 * (g - 1.0) * left.u + s);
            const double r = std::max(2.0 / (g + 1.0) + gm / a_left * (left.u - s), 0.0);
            out.rho = left.rho * std::pow(r, 2.0 / (g - 1.0));
            out.p = left.p * std::pow(r, 2.0 * g / (g - 1.0));
        } else if (s <= front_right) {
            out.rho = 0.0;
            out.p = 0.0;
            out.u = 0.5 * (front_left + front_right);
        } else if (s < head_right) {
            out.u = (2.0 / (g + 1.0)) * (-a_right + 0.5 * (g - 1.0) * right.u + s);
            const double r = std::max(2.0 / (g + 1.0) - gm / a_right * (right.u - s), 0.0);
            out.rho = right.rho * std::pow(r, 2.0 / (g - 1.0));
            out.p = right.p * std::pow(r, 2.0 * g / (g - 1.0));
        } else {
            out = right;
        }
        return out;
    }

    if (s <= star.velocity) {
        // Lado izquierdo del contacto.
        if (star.pressure > left.p) {
            // Choque izquierdo (ec. 4.52).
            const double shock =
                left.u - a_left * std::sqrt(gp * star.pressure / left.p + (g - 1.0) / (2.0 * g));
            if (s <= shock) {
                out = left;
            } else {
                out.rho = left.rho * ((star.pressure / left.p + gm) /
                                      (gm * star.pressure / left.p + 1.0));
                out.u = star.velocity;
                out.p = star.pressure;
            }
        } else {
            // Rarefaccion izquierda (ecs. 4.54-4.56).
            const double head = left.u - a_left;
            const double a_star = a_left * std::pow(star.pressure / left.p, (g - 1.0) / (2.0 * g));
            const double tail = star.velocity - a_star;
            if (s <= head) {
                out = left;
            } else if (s >= tail) {
                out.rho = left.rho * std::pow(star.pressure / left.p, 1.0 / g);
                out.u = star.velocity;
                out.p = star.pressure;
            } else {
                out.u = (2.0 / (g + 1.0)) * (a_left + 0.5 * (g - 1.0) * left.u + s);
                const double r = std::max(2.0 / (g + 1.0) + gm / a_left * (left.u - s), 0.0);
                out.rho = left.rho * std::pow(r, 2.0 / (g - 1.0));
                out.p = left.p * std::pow(r, 2.0 * g / (g - 1.0));
            }
        }
    } else {
        // Lado derecho del contacto.
        if (star.pressure > right.p) {
            // Choque derecho (ec. 4.59).
            const double shock =
                right.u + a_right * std::sqrt(gp * star.pressure / right.p + (g - 1.0) / (2.0 * g));
            if (s >= shock) {
                out = right;
            } else {
                out.rho = right.rho * ((star.pressure / right.p + gm) /
                                       (gm * star.pressure / right.p + 1.0));
                out.u = star.velocity;
                out.p = star.pressure;
            }
        } else {
            // Rarefaccion derecha (ecs. 4.61-4.63).
            const double head = right.u + a_right;
            const double a_star =
                a_right * std::pow(star.pressure / right.p, (g - 1.0) / (2.0 * g));
            const double tail = star.velocity + a_star;
            if (s >= head) {
                out = right;
            } else if (s <= tail) {
                out.rho = right.rho * std::pow(star.pressure / right.p, 1.0 / g);
                out.u = star.velocity;
                out.p = star.pressure;
            } else {
                out.u = (2.0 / (g + 1.0)) * (-a_right + 0.5 * (g - 1.0) * right.u + s);
                const double r = std::max(2.0 / (g + 1.0) - gm / a_right * (right.u - s), 0.0);
                out.rho = right.rho * std::pow(r, 2.0 / (g - 1.0));
                out.p = right.p * std::pow(r, 2.0 * g / (g - 1.0));
            }
        }
    }
    return out;
}

}  // namespace euler1d
