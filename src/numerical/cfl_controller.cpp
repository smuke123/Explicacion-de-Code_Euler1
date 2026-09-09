#include "euler1d/numerical/cfl_controller.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace euler1d {

CflController::Scan CflController::scan(const PrimitiveField& w) const {
    const int n = mesh_->cellCount();
    // Semilla 1e-30 (como el Fortran): un campo exactamente en reposo y frio
    // no debe producir una division por cero.
    double max_speed = kMinSignalSpeed;
    double checksum = 0.0;
    // Reduccion sobre celdas: `#pragma omp parallel for reduction(max:max_speed)
    // reduction(+:checksum)`.
    for (int k = 1; k <= n; ++k) {
        const double speed = std::abs(w[k].u) + gas_.soundSpeed(w[k]);
        max_speed = std::max(max_speed, speed);
        checksum += speed;
    }
    return {max_speed, checksum};
}

double CflController::maxSignalSpeed(const PrimitiveField& w) const {
    return scan(w).max_speed;
}

double CflController::computeTimeStep(const PrimitiveField& w) const {
    const Scan result = scan(w);

    // Un estado no finito es un FALLO, no algo que se pueda pisar con un
    // floor: significa que el esquema ya perdio la solucion. Se detecta con la
    // suma porque el maximo no propaga NaN.
    if (!std::isfinite(result.checksum)) {
        throw std::runtime_error(
            "el campo contiene valores no finitos (NaN o Inf): la simulacion "
            "perdio validez; revise CFL, condicion inicial y fronteras");
    }

    const double dt = cfl_ * mesh_->dx() / result.max_speed;
    if (!std::isfinite(dt) || dt <= 0.0) {
        throw std::runtime_error(
            "paso de tiempo invalido (dt = " + std::to_string(dt) +
            ", velocidad de senal maxima = " + std::to_string(result.max_speed) + ")");
    }
    return dt;
}

}  // namespace euler1d
