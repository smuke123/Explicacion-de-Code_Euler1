#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "euler1d/io/xl_reader.hpp"
#include "euler1d/simulation/simulation.hpp"
#include "euler1d/validation/error_analyzer.hpp"

using namespace euler1d;

// Regresion contra las salidas ARCHIVADAS del codigo Fortran.
//
// Cada subdirectorio de tests/reference contiene el `input.par` con el que se
// produjo la corrida y los `.xl` resultantes. El test vuelve a correr esa misma
// configuracion con el codigo C++ y compara celda a celda.
//
// TOLERANCIA (criterio mixto, absoluto + relativo):
//
//     max|ref - cpp|  <=  kAbsoluteTolerance + kRelativeTolerance * max|ref|
//
// La parte relativa (1e-9) es la que manda en los campos de magnitud normal:
// queda muy por encima del redondeo de doble precision y muy por debajo del
// error de discretizacion del esquema (~1e-2), asi que detecta una diferencia
// real de formulacion pero tolera el reordenamiento de operaciones.
//
// La parte absoluta (1e-12) es imprescindible: en el test `atmosfera` el campo
// de velocidad del Fortran vale 6.7e-16 —cero maquina, que es exactamente lo
// que debe dar un esquema bien balanceado— y normalizar por esa escala
// convertiria una coincidencia perfecta en un error relativo de 1e3. La
// referencia de la parte absoluta es la escala fisica de estos problemas, donde
// rho, p y c son de orden unidad.

namespace {

constexpr double kRelativeTolerance = 1.0e-9;
constexpr double kAbsoluteTolerance = 1.0e-12;

std::filesystem::path referenceRoot() { return std::filesystem::path(EULER1D_REFERENCE_DIR); }

std::vector<std::filesystem::path> referenceCases() {
    std::vector<std::filesystem::path> cases;
    for (const auto& entry : std::filesystem::directory_iterator(referenceRoot())) {
        if (entry.is_directory() && std::filesystem::exists(entry.path() / "input.par")) {
            cases.push_back(entry.path());
        }
    }
    std::sort(cases.begin(), cases.end());
    return cases;
}

/// Datos que el Fortran deja en `log.txt` (stdout de la corrida original).
struct FortranLog {
    std::optional<double> p_star;
    std::optional<double> u_star;
    std::optional<long long> steps;
    std::optional<double> final_time;
};

FortranLog readLog(const std::filesystem::path& directory) {
    FortranLog log;
    std::ifstream in(directory / "log.txt");
    if (!in) return log;
    std::string line;
    while (std::getline(in, line)) {
        if (line.find("exacto:") != std::string::npos) {
            const auto p_at = line.find("p* =");
            const auto u_at = line.find("u* =");
            if (p_at != std::string::npos) log.p_star = std::stod(line.substr(p_at + 4));
            if (u_at != std::string::npos) log.u_star = std::stod(line.substr(u_at + 4));
        } else if (line.find("pasos:") != std::string::npos) {
            const auto steps_at = line.find("pasos:");
            log.steps = std::stoll(line.substr(steps_at + 6));
            const auto time_at = line.find("t final:");
            if (time_at != std::string::npos) log.final_time = std::stod(line.substr(time_at + 8));
        }
    }
    return log;
}

class ScratchDirectory {
public:
    explicit ScratchDirectory(const std::string& label) {
        path_ = std::filesystem::temp_directory_path() / ("euler1d_reg_" + label);
        std::filesystem::remove_all(path_);
        std::filesystem::create_directories(path_);
    }
    ~ScratchDirectory() {
        std::error_code ignored;
        std::filesystem::remove_all(path_, ignored);
    }
    ScratchDirectory(const ScratchDirectory&) = delete;
    ScratchDirectory& operator=(const ScratchDirectory&) = delete;
    const std::filesystem::path& path() const { return path_; }

private:
    std::filesystem::path path_;
};

struct CaseOutcome {
    std::string name;
    /// Error maximo dividido por su propia tolerancia: <= 1 significa OK, y el
    /// valor es comparable entre campos de escalas muy distintas.
    double worst_ratio = 0.0;
    double worst_max_abs = 0.0;
    std::string worst_field;
    bool compared = false;
};

double toleranceFor(double reference_scale) {
    return kAbsoluteTolerance + kRelativeTolerance * reference_scale;
}

CaseOutcome runCase(const std::filesystem::path& reference_dir) {
    CaseOutcome outcome;
    outcome.name = reference_dir.filename().string();

    SimulationConfig config = readConfigFile(reference_dir / "input.par");
    config.validate();
    config = resolveTestCase(config);
    config.validate();

    ScratchDirectory scratch(outcome.name);
    Simulation simulation(config, scratch.path());
    const SimulationReport report = simulation.run(false);

    // Si el Fortran dejo su stdout, el numero de pasos es una firma muy
    // sensible: solo coincide si toda la secuencia de dt coincidio.
    const FortranLog log = readLog(reference_dir);
    if (log.steps) {
        if (report.steps != *log.steps) {
            ::testing::fail(__FILE__, __LINE__,
                            outcome.name + ": numero de pasos distinto, Fortran=" +
                                std::to_string(*log.steps) + " C++=" +
                                std::to_string(report.steps));
        }
    }
    if (log.p_star && report.exact_star) {
        // El log trae 7 cifras significativas.
        CHECK_REL(report.exact_star->pressure, *log.p_star, 1.0e-6);
    }
    if (log.u_star && report.exact_star && std::abs(*log.u_star) > 1.0e-8) {
        CHECK_REL(report.exact_star->velocity, *log.u_star, 1.0e-6);
    }

    for (const char* file : {"rho_1.xl", "vel_1.xl", "press_1.xl", "rho_exact.xl", "vel_exact.xl",
                             "press_exact.xl"}) {
        const auto reference_path = reference_dir / file;
        const auto candidate_path = scratch.path() / file;
        if (!std::filesystem::exists(reference_path)) continue;
        if (!std::filesystem::exists(candidate_path)) {
            ::testing::fail(__FILE__, __LINE__,
                            outcome.name + ": el C++ no produjo " + std::string(file));
        }
        const XlBlock reference = readLastXlBlock(reference_path);
        const XlBlock candidate = readLastXlBlock(candidate_path);
        if (reference.size() != candidate.size()) {
            ::testing::fail(__FILE__, __LINE__,
                            outcome.name + "/" + file + ": " + std::to_string(reference.size()) +
                                " celdas en la referencia contra " +
                                std::to_string(candidate.size()));
        }
        const double dx = reference.size() > 1 ? reference.x[1] - reference.x[0] : 1.0;
        const ErrorMetrics metrics = compareFields(reference.value, candidate.value, dx);
        const double tolerance = toleranceFor(metrics.reference_max_abs);
        const double ratio = metrics.max_abs / tolerance;
        outcome.compared = true;
        if (ratio > outcome.worst_ratio) {
            outcome.worst_ratio = ratio;
            outcome.worst_max_abs = metrics.max_abs;
            outcome.worst_field = file;
        }
        if (!(metrics.max_abs <= tolerance)) {
            ::testing::fail(__FILE__, __LINE__,
                            outcome.name + "/" + file + ": max|d| = " +
                                std::to_string(metrics.max_abs) + " > tolerancia " +
                                std::to_string(tolerance) + "  [" + format(metrics) + "]");
        }
    }
    return outcome;
}

}  // namespace

TEST(hay_casos_de_referencia_disponibles) {
    const auto cases = referenceCases();
    std::printf("           %zu casos en %s\n", cases.size(), referenceRoot().string().c_str());
    CHECK(!cases.empty());
}

TEST(todas_las_corridas_archivadas_del_fortran_se_reproducen) {
    double worst_ratio = 0.0;
    std::string worst_case;
    int compared = 0;
    for (const auto& directory : referenceCases()) {
        const CaseOutcome outcome = runCase(directory);
        if (!outcome.compared) {
            std::printf("           %-24s (sin archivos .xl comparables)\n",
                        outcome.name.c_str());
            continue;
        }
        ++compared;
        std::printf("           %-24s max|d| = %.3e  (%.4f x tolerancia, en %s)\n",
                    outcome.name.c_str(), outcome.worst_max_abs, outcome.worst_ratio,
                    outcome.worst_field.c_str());
        if (outcome.worst_ratio > worst_ratio) {
            worst_ratio = outcome.worst_ratio;
            worst_case = outcome.name;
        }
    }
    std::printf("           --> peor caso: %s a %.4f x su tolerancia (abs %.1e + rel %.1e)\n",
                worst_case.c_str(), worst_ratio, kAbsoluteTolerance, kRelativeTolerance);
    CHECK(compared > 0);
}
