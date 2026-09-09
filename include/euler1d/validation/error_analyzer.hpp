#pragma once

#include <string>
#include <vector>

namespace euler1d {

/// Metricas de discrepancia entre dos muestreos del mismo campo.
///
/// `l1` y `l2` llevan el peso dx para ser normas discretas de funcion
/// (comparables entre mallas distintas); `max_abs` y `max_relative` son
/// puntuales y sirven como criterio de aceptacion de la migracion.
struct ErrorMetrics {
    double max_abs = 0.0;
    double l1 = 0.0;
    double l2 = 0.0;
    double max_relative = 0.0;
    /// Escala del campo de referencia, para interpretar `max_abs`.
    double reference_max_abs = 0.0;
    std::size_t samples = 0;
};

/// Compara dos vectores del mismo tamano.
/// `dx` pondera L1 y L2; use 1.0 para normas sin peso.
ErrorMetrics compareFields(const std::vector<double>& reference,
                           const std::vector<double>& candidate,
                           double dx);

std::string format(const ErrorMetrics& metrics);

}  // namespace euler1d
