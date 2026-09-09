#include "euler1d/io/output_manager.hpp"

#include <fstream>
#include <stdexcept>
#include <utility>

#include "euler1d/io/fortran_format.hpp"

namespace euler1d {
namespace {

const char* kNumericFiles[3] = {"rho_1.xl", "vel_1.xl", "press_1.xl"};
const char* kExactFiles[3] = {"rho_exact.xl", "vel_exact.xl", "press_exact.xl"};

double component(const PrimitiveState& w, int index) {
    switch (index) {
        case 0: return w.rho;
        case 1: return w.u;
        default: return w.p;
    }
}

}  // namespace

OutputManager::OutputManager(std::filesystem::path directory, const Mesh1D& mesh)
    : directory_(std::move(directory)), mesh_(&mesh) {
    std::filesystem::create_directories(directory_);
}

void OutputManager::writeBlock(const std::string& filename, bool truncate, const PrimitiveField* w,
                               const std::vector<PrimitiveState>* profile, int index, double time) {
    const std::filesystem::path path = directory_ / filename;
    // Modo binario y terminador LF explicito: en Windows el modo texto
    // insertaria CRLF y los archivos dejarian de ser comparables byte a byte
    // con los que produce el Fortran.
    std::ofstream out(path, std::ios::binary | (truncate ? std::ios::trunc : std::ios::app));
    if (!out) throw std::runtime_error("no se pudo escribir " + path.string());

    out << formatTimeHeader(time) << '\n';
    const int n = mesh_->cellCount();
    for (int k = 1; k <= n; ++k) {
        const PrimitiveState& state =
            w ? (*w)[k] : (*profile)[static_cast<std::size_t>(k - 1)];
        out << formatDataRow(mesh_->center(k), component(state, index)) << '\n';
    }
    // El Fortran cierra cada bloque con `write(un,*)`, una linea en blanco.
    out << '\n';
}

void OutputManager::writeInitial(const PrimitiveField& w, double time) {
    for (int m = 0; m < 3; ++m) {
        writeBlock(kNumericFiles[m], true, &w, nullptr, m, time);
    }
}

void OutputManager::appendSnapshot(const PrimitiveField& w, double time) {
    for (int m = 0; m < 3; ++m) {
        writeBlock(kNumericFiles[m], false, &w, nullptr, m, time);
    }
}

void OutputManager::writeExact(const std::vector<PrimitiveState>& profile, double time) {
    for (int m = 0; m < 3; ++m) {
        writeBlock(kExactFiles[m], true, nullptr, &profile, m, time);
    }
}

void OutputManager::writeRunInfo(const SimulationConfig& config, const std::string& details,
                                 long long steps, double final_time, double wall_seconds) {
    const std::filesystem::path path = directory_ / "run_info.txt";
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("no se pudo escribir " + path.string());

    // Reproducibilidad: la configuracion EFECTIVA (ya resuelta por el test)
    // queda junto a los resultados, para que una figura siempre se pueda
    // atribuir a una corrida concreta.
    out << "# Euler1D C++ -- configuracion efectiva de la corrida\n";
    out << "test         = " << config.test << '\n';
    out << "N            = " << config.cell_count << '\n';
    out << "cfl          = " << config.cfl << '\n';
    out << "gamma        = " << config.gamma << '\n';
    out << "limiter      = " << toString(config.limiter) << '\n';
    out << "solver       = " << toString(config.solver) << '\n';
    out << "bc           = " << toString(config.boundary) << '\n';
    out << "alpha_geo    = " << static_cast<int>(config.geometry) << '\n';
    out << "grav         = " << config.gravity << '\n';
    out << "xmin         = " << config.x_min << '\n';
    out << "xmax         = " << config.x_max << '\n';
    out << "x0           = " << config.diaphragm_x0 << '\n';
    out << "tf           = " << config.final_time << '\n';
    out << "every_out    = " << config.output_every << '\n';
    out << "rho_floor    = " << config.density_floor << '\n';
    out << "p_floor      = " << config.pressure_floor << '\n';
    out << "steps        = " << steps << '\n';
    out << "t_final      = " << final_time << '\n';
    out << "wall_seconds = " << wall_seconds << '\n';
    if (!details.empty()) out << details;
}

}  // namespace euler1d
