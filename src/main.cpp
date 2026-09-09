#include <cstdio>
#include <exception>
#include <filesystem>
#include <iostream>
#include <string>

#include "euler1d/simulation/simulation.hpp"

namespace {

void printUsage(const char* program) {
    std::printf(
        "Uso: %s [opciones]\n"
        "  -i, --input <archivo>   namelist &euler (por defecto: input.par)\n"
        "  -o, --output <dir>      directorio de salida (por defecto: el actual)\n"
        "  -q, --quiet             no imprimir el progreso\n"
        "  -h, --help              esta ayuda\n",
        program);
}

}  // namespace

int main(int argc, char** argv) {
    std::filesystem::path input = "input.par";
    std::filesystem::path output = ".";
    bool verbose = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](const char* what) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("falta el valor de ") + what);
            }
            return argv[++i];
        };
        try {
            if (arg == "-i" || arg == "--input") {
                input = next("--input");
            } else if (arg == "-o" || arg == "--output") {
                output = next("--output");
            } else if (arg == "-q" || arg == "--quiet") {
                verbose = false;
            } else if (arg == "-h" || arg == "--help") {
                printUsage(argv[0]);
                return 0;
            } else {
                std::fprintf(stderr, "ERROR: argumento desconocido '%s'\n", arg.c_str());
                printUsage(argv[0]);
                return 2;
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "ERROR: %s\n", error.what());
            return 2;
        }
    }

    try {
        // Orden deliberado, el mismo del Fortran: leer -> validar -> resolver
        // el caso de prueba. Validar antes de resolver hace que un valor
        // invalido del usuario se detecte aunque el test fuera a sobreescribirlo.
        euler1d::SimulationConfig config = euler1d::readConfigFile(input);
        config.validate();

        std::string overrides;
        config = euler1d::resolveTestCase(config, &overrides);
        config.validate();

        if (verbose) {
            std::printf("%s\n", euler1d::describe(config).c_str());
            if (!overrides.empty()) {
                std::printf(" AVISO: el test sobreescribe opciones dadas por el usuario:\n%s",
                            overrides.c_str());
            }
        }

        euler1d::Simulation simulation(config, output);
        simulation.run(verbose);
        return 0;
    } catch (const euler1d::ConfigError& error) {
        std::fprintf(stderr, "ERROR de configuracion: %s\n", error.what());
        return 1;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ERROR: %s\n", error.what());
        return 1;
    }
}
