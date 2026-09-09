#include "euler1d/boundary/boundary_condition.hpp"

namespace euler1d {

void TransmissiveBoundary::apply(ConservedField& u, PrimitiveField& w) const {
    const int n = u.cellCount();
    // Extrapolacion CONSTANTE: los dos fantasmas de cada lado copian la misma
    // celda de borde. El Fortran hace `Uc(:,-1) = Uc(:,1)`, no `Uc(:,2)`,
    // de modo que la pendiente reconstruida en la cara extrema se anula.
    u[0] = u[1];
    u[-1] = u[1];
    u[n + 1] = u[n];
    u[n + 2] = u[n];
    w[0] = w[1];
    w[-1] = w[1];
    w[n + 1] = w[n];
    w[n + 2] = w[n];
}

void PeriodicBoundary::apply(ConservedField& u, PrimitiveField& w) const {
    const int n = u.cellCount();
    u[0] = u[n];
    u[-1] = u[n - 1];
    u[n + 1] = u[1];
    u[n + 2] = u[2];
    w[0] = w[n];
    w[-1] = w[n - 1];
    w[n + 1] = w[1];
    w[n + 2] = w[2];
}

void ReflectiveBoundary::apply(ConservedField& u, PrimitiveField& w) const {
    const int n = u.cellCount();

    // Espejo en densidad, antisimetria en velocidad.
    w[0].rho = w[1].rho;
    w[0].u = -w[1].u;
    w[-1].rho = w[2].rho;
    w[-1].u = -w[2].u;
    w[n + 1].rho = w[n].rho;
    w[n + 1].u = -w[n].u;
    w[n + 2].rho = w[n - 1].rho;
    w[n + 2].u = -w[n - 1].u;

    if (gravity_ != 0.0) {
        // La presion NO se refleja: continua el equilibrio hidrostatico
        // discreto trapezoidal. Con el espejo plano (dp = 0) las desviaciones
        // hidrostaticas no se cancelan en las caras de pared y un fluido en
        // reposo genera espuriamente |u| ~ 1e-3.
        w[0].p = w[1].p + gravity_ * dx_ * 0.5 * (w[0].rho + w[1].rho);
        w[-1].p = w[0].p + gravity_ * dx_ * 0.5 * (w[-1].rho + w[0].rho);
        w[n + 1].p = w[n].p - gravity_ * dx_ * 0.5 * (w[n + 1].rho + w[n].rho);
        w[n + 2].p = w[n + 1].p - gravity_ * dx_ * 0.5 * (w[n + 2].rho + w[n + 1].rho);
    } else {
        w[0].p = w[1].p;
        w[-1].p = w[2].p;
        w[n + 1].p = w[n].p;
        w[n + 2].p = w[n - 1].p;
    }

    for (const int k : {-1, 0, n + 1, n + 2}) {
        u[k] = gas_.toConserved(w[k]);
    }
}

}  // namespace euler1d
