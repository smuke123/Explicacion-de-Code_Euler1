#include "euler1d/config/simulation_config.hpp"

#include <cstdio>

namespace euler1d {

std::string toString(LimiterKind kind) {
    switch (kind) {
        case LimiterKind::FirstOrder: return "fo";
        case LimiterKind::MinMod: return "mm";
        case LimiterKind::VanLeer: return "vl";
        case LimiterKind::MonotonizedCentral: return "mc";
        case LimiterKind::Superbee: return "sb";
    }
    return "?";
}

std::string toString(RiemannSolverKind kind) {
    switch (kind) {
        case RiemannSolverKind::HLL: return "hll";
        case RiemannSolverKind::HLLC: return "hllc";
    }
    return "?";
}

std::string toString(BoundaryKind kind) {
    switch (kind) {
        case BoundaryKind::Transmissive: return "trans";
        case BoundaryKind::Periodic: return "per";
        case BoundaryKind::Reflective: return "refl";
    }
    return "?";
}

void SimulationConfig::validate() const {
    // Replica `validate_input` del Fortran. Los enums ya impiden un solver o
    // un limitador desconocido: el parser rechaza la cadena antes de llegar
    // aqui, que es justo la garantia que se pedia (una opcion invalida nunca
    // selecciona otro algoritmo en silencio).
    if (test < 1 || test > kTestCaseCount) {
        throw ConfigError("test debe estar entre 1 y " + std::to_string(kTestCaseCount) +
                          "; recibido: " + std::to_string(test));
    }
    if (cell_count < 4) {
        throw ConfigError("N debe ser >= 4; recibido: " + std::to_string(cell_count));
    }
    if (!(gamma > 1.0)) {
        throw ConfigError("gamma debe ser > 1; recibido: " + std::to_string(gamma));
    }
    if (!(cfl > 0.0) || cfl > 1.0) {
        throw ConfigError("cfl debe estar en (0,1]; recibido: " + std::to_string(cfl));
    }
    if (!(x_max > x_min)) {
        throw ConfigError("xmax debe ser mayor que xmin");
    }
    if (output_every < 0) {
        throw ConfigError("every_out no puede ser negativo");
    }
    // Los pisos no estan en el namelist del Fortran, pero si alguien los
    // expone conviene que sigan siendo positivos: un piso <= 0 no protege.
    if (!(density_floor > 0.0) || !(pressure_floor > 0.0)) {
        throw ConfigError("los pisos rho_floor y p_floor deben ser positivos");
    }
}

std::string describe(const SimulationConfig& config) {
    // Mismo texto que la linea `config:` del Fortran
    // ('(a,i2,a,i5,a,a,a,a,a,f6.3)'), para poder diferenciar stdout.
    char buffer[256];
    std::snprintf(buffer, sizeof(buffer),
                  " config: test=%2d  N=%5d  solver=%s  limiter=%s  cfl=%6.3f", config.test,
                  config.cell_count, toString(config.solver).c_str(),
                  toString(config.limiter).c_str(), config.cfl);
    return std::string(buffer);
}

}  // namespace euler1d
