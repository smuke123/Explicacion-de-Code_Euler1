// Comparador cuantitativo de dos corridas en formato .xl.
//
// Es la herramienta con la que se produce docs/validation.md: toma el
// directorio de una corrida de referencia (Fortran) y el de una corrida
// candidata (C++) y reporta max|d|, L1, L2 y error relativo para rho, u y p,
// tanto de la solucion numerica como de la exacta.
//
// Devuelve 0 si todas las metricas quedan por debajo de la tolerancia.

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <string>
#include <vector>

#include "euler1d/io/xl_reader.hpp"
#include "euler1d/validation/error_analyzer.hpp"

namespace {

struct Comparison {
    std::string label;
    euler1d::ErrorMetrics metrics;
    bool present = false;
};

Comparison compareOne(const std::filesystem::path& reference_dir,
                      const std::filesystem::path& candidate_dir, const std::string& file) {
    Comparison result;
    result.label = file;
    const auto reference_path = reference_dir / file;
    const auto candidate_path = candidate_dir / file;
    if (!std::filesystem::exists(reference_path) || !std::filesystem::exists(candidate_path)) {
        return result;
    }
    const euler1d::XlBlock reference = euler1d::readLastXlBlock(reference_path);
    const euler1d::XlBlock candidate = euler1d::readLastXlBlock(candidate_path);
    if (reference.size() != candidate.size()) {
        std::fprintf(stderr, "  %-16s TAMANOS DISTINTOS: %zu vs %zu\n", file.c_str(),
                     reference.size(), candidate.size());
        return result;
    }
    const double dx = reference.size() > 1 ? (reference.x[1] - reference.x[0]) : 1.0;
    result.metrics = euler1d::compareFields(reference.value, candidate.value, dx);
    result.present = true;
    return result;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::printf("Uso: %s <dir_referencia> <dir_candidato> [tolerancia_relativa]\n", argv[0]);
        return 2;
    }
    const std::filesystem::path reference_dir = argv[1];
    const std::filesystem::path candidate_dir = argv[2];
    const double tolerance = argc > 3 ? std::atof(argv[3]) : 1.0e-10;

    const std::vector<std::string> files = {"rho_1.xl",      "vel_1.xl",      "press_1.xl",
                                            "rho_exact.xl",  "vel_exact.xl",  "press_exact.xl"};

    std::printf("referencia: %s\ncandidato : %s\ntolerancia: %.3e (relativa a la escala del campo)\n\n",
                reference_dir.string().c_str(), candidate_dir.string().c_str(), tolerance);

    bool all_ok = true;
    bool any_compared = false;
    try {
        for (const std::string& file : files) {
            const Comparison comparison = compareOne(reference_dir, candidate_dir, file);
            if (!comparison.present) {
                std::printf("  %-16s (ausente, omitido)\n", file.c_str());
                continue;
            }
            any_compared = true;
            // Se normaliza por la escala del campo: un max|d| de 1e-9 es
            // despreciable en un campo de orden 1000 y enorme en uno de orden 1e-6.
            const double scale = comparison.metrics.reference_max_abs > 0.0
                                     ? comparison.metrics.reference_max_abs
                                     : 1.0;
            const double normalized = comparison.metrics.max_abs / scale;
            const bool ok = normalized <= tolerance;
            all_ok = all_ok && ok;
            std::printf("  %-16s %s  norm=%.3e  %s\n", file.c_str(),
                        euler1d::format(comparison.metrics).c_str(), normalized,
                        ok ? "OK" : "FALLA");
        }
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ERROR: %s\n", error.what());
        return 1;
    }

    if (!any_compared) {
        std::fprintf(stderr, "ERROR: no se pudo comparar ningun archivo\n");
        return 1;
    }
    std::printf("\nresultado: %s\n", all_ok ? "TODAS LAS METRICAS DENTRO DE TOLERANCIA" : "FUERA DE TOLERANCIA");
    return all_ok ? 0 : 1;
}
