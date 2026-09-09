#include "euler1d/validation/error_analyzer.hpp"

#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace euler1d {

ErrorMetrics compareFields(const std::vector<double>& reference,
                           const std::vector<double>& candidate, double dx) {
    if (reference.size() != candidate.size()) {
        throw std::runtime_error("compareFields: tamanos distintos (" +
                                 std::to_string(reference.size()) + " vs " +
                                 std::to_string(candidate.size()) + ")");
    }

    ErrorMetrics metrics;
    metrics.samples = reference.size();
    double sum_squares = 0.0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
        const double diff = std::abs(candidate[i] - reference[i]);
        metrics.max_abs = std::max(metrics.max_abs, diff);
        metrics.l1 += diff * dx;
        sum_squares += diff * diff * dx;
        metrics.reference_max_abs = std::max(metrics.reference_max_abs, std::abs(reference[i]));
        // El error relativo solo tiene sentido donde la referencia no se anula.
        if (std::abs(reference[i]) > 0.0) {
            metrics.max_relative =
                std::max(metrics.max_relative, diff / std::abs(reference[i]));
        }
    }
    metrics.l2 = std::sqrt(sum_squares);
    return metrics;
}

std::string format(const ErrorMetrics& metrics) {
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  "max|d|=%.3e  L1=%.3e  L2=%.3e  rel_max=%.3e  |ref|_max=%.3e  n=%zu",
                  metrics.max_abs, metrics.l1, metrics.l2, metrics.max_relative,
                  metrics.reference_max_abs, metrics.samples);
    return std::string(buffer);
}

}  // namespace euler1d
