#include "test_support.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "euler1d/numerical/riemann/hllc.hpp"
#include "euler1d/simulation/simulation.hpp"
#include "euler1d/validation/error_analyzer.hpp"

using namespace euler1d;

namespace {

/// Directorio temporal propio de cada caso, borrado al terminar (RAII).
class ScratchDirectory {
public:
    explicit ScratchDirectory(const std::string& label) {
        path_ = std::filesystem::temp_directory_path() / ("euler1d_test_" + label);
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

SimulationConfig makeConfig(int test, int cells, LimiterKind limiter, RiemannSolverKind solver) {
    SimulationConfig config;
    config.test = test;
    config.cell_count = cells;
    config.limiter = limiter;
    config.solver = solver;
    config.validate();
    return resolveTestCase(config);
}

/// Error L1 de la densidad frente al perfil de referencia del caso.
double densityErrorL1(const Simulation& simulation, double time) {
    const auto reference = simulation.referenceProfile(time);
    if (!reference) throw std::runtime_error("el caso no tiene referencia analitica");
    const int n = simulation.mesh().cellCount();
    std::vector<double> exact(static_cast<std::size_t>(n));
    std::vector<double> numeric(static_cast<std::size_t>(n));
    for (int k = 1; k <= n; ++k) {
        exact[static_cast<std::size_t>(k - 1)] = (*reference)[static_cast<std::size_t>(k - 1)].rho;
        numeric[static_cast<std::size_t>(k - 1)] = simulation.state().primitive[k].rho;
    }
    return compareFields(exact, numeric, simulation.mesh().dx()).l1;
}

double totalMass(const Simulation& simulation) {
    double sum = 0.0;
    for (int k = 1; k <= simulation.mesh().cellCount(); ++k) {
        sum += simulation.state().conserved[k].rho;
    }
    return sum * simulation.mesh().dx();
}

/// Energia total con el peso de volumen de la geometria activa.
double totalEnergy(const Simulation& simulation) {
    const Mesh1D& mesh = simulation.mesh();
    const int alpha = static_cast<int>(simulation.config().geometry);
    const double dx = mesh.dx();
    double sum = 0.0;
    for (int k = 1; k <= mesh.cellCount(); ++k) {
        double volume = dx;
        if (alpha > 0) {
            const double r_plus = mesh.center(k) + 0.5 * dx;
            const double r_minus = mesh.center(k) - 0.5 * dx;
            volume = (std::pow(r_plus, alpha + 1) - std::pow(r_minus, alpha + 1)) / (alpha + 1);
        }
        sum += simulation.state().conserved[k].energy * volume;
    }
    return sum;
}

}  // namespace

// ------------------------------------------------ corridas completas

TEST(toro1_corre_y_termina_exactamente_en_tf) {
    ScratchDirectory scratch("toro1");
    Simulation simulation(makeConfig(1, 100, LimiterKind::VanLeer, RiemannSolverKind::HLLC),
                          scratch.path());
    const SimulationReport report = simulation.run(false);
    // El recorte del ultimo paso debe aterrizar en tf sin sobrepasarlo.
    CHECK_NEAR(report.final_time, 0.2, 1.0e-14);
    CHECK(report.steps > 10);
    CHECK(!report.floors.triggered());
}

TEST(toro1_genera_los_seis_archivos_de_salida) {
    ScratchDirectory scratch("toro1_files");
    Simulation simulation(makeConfig(1, 50, LimiterKind::VanLeer, RiemannSolverKind::HLLC),
                          scratch.path());
    simulation.run(false);
    for (const char* file : {"rho_1.xl", "vel_1.xl", "press_1.xl", "rho_exact.xl", "vel_exact.xl",
                             "press_exact.xl", "run_info.txt"}) {
        CHECK(std::filesystem::exists(scratch.path() / file));
    }
}

TEST(los_cinco_tests_de_toro_terminan_con_estados_fisicos) {
    for (int test = 1; test <= 5; ++test) {
        ScratchDirectory scratch("toro_all_" + std::to_string(test));
        Simulation simulation(makeConfig(test, 100, LimiterKind::VanLeer, RiemannSolverKind::HLLC),
                              scratch.path());
        const SimulationReport report = simulation.run(false);
        CHECK(report.steps > 0);
        for (int k = 1; k <= simulation.mesh().cellCount(); ++k) {
            const PrimitiveState& w = simulation.state().primitive[k];
            CHECK(std::isfinite(w.rho));
            CHECK(std::isfinite(w.u));
            CHECK(std::isfinite(w.p));
            CHECK(w.rho > 0.0);
            CHECK(w.p > 0.0);
        }
    }
}

TEST(todos_los_limitadores_y_solvers_completan_toro1) {
    for (const LimiterKind limiter :
         {LimiterKind::FirstOrder, LimiterKind::MinMod, LimiterKind::VanLeer,
          LimiterKind::MonotonizedCentral, LimiterKind::Superbee}) {
        for (const RiemannSolverKind solver : {RiemannSolverKind::HLL, RiemannSolverKind::HLLC}) {
            ScratchDirectory scratch("combo");
            Simulation simulation(makeConfig(1, 50, limiter, solver), scratch.path());
            const SimulationReport report = simulation.run(false);
            CHECK(report.steps > 0);
            CHECK(std::isfinite(densityErrorL1(simulation, report.final_time)));
        }
    }
}

// ------------------------------------------------------ conservacion

TEST(el_test_periodico_conserva_la_masa_a_precision_de_maquina) {
    // Con fronteras periodicas los flujos de las caras extremas coinciden, asi
    // que la suma telescopica se anula: es la comprobacion mas directa de que
    // la actualizacion es realmente conservativa.
    ScratchDirectory scratch("mass");
    Simulation simulation(makeConfig(6, 100, LimiterKind::VanLeer, RiemannSolverKind::HLLC),
                          scratch.path());
    const double initial = totalMass(simulation);
    simulation.run(false);
    const double final_mass = totalMass(simulation);
    CHECK_REL(final_mass, initial, 1.0e-12);
}

TEST(la_geometria_esferica_conserva_la_energia_de_sedov) {
    // La forma conservativa (areas y volumenes exactos) debe conservar la
    // energia total. La forma fuente -(alpha/r)F no lo hace: segun la auditoria
    // del Fortran inyectaba un 42% de energia espuria al arrancar el Sedov.
    ScratchDirectory scratch("sedov");
    Simulation simulation(makeConfig(8, 64, LimiterKind::VanLeer, RiemannSolverKind::HLLC),
                          scratch.path());
    const double initial = totalEnergy(simulation);
    simulation.run(false);
    const double final_energy = totalEnergy(simulation);
    CHECK_REL(final_energy, initial, 1.0e-8);
}

// ------------------------------------------------- balance hidrostatico

TEST(la_atmosfera_hidrostatica_permanece_en_reposo) {
    // La reconstruccion hidrostatica y la presion de los fantasmas de pared
    // deben cancelar la fuente -rho g celda a celda. Un esquema no balanceado
    // genera |u| ~ 1e-3 en este test.
    ScratchDirectory scratch("atmosfera");
    SimulationConfig config = makeConfig(7, 100, LimiterKind::VanLeer, RiemannSolverKind::HLLC);
    config.final_time = 1.0;  // 1 unidad basta para ver el desequilibrio
    Simulation simulation(config, scratch.path());
    const SimulationReport report = simulation.run(false);
    CHECK(report.hydrostatic_residual.has_value());
    CHECK(*report.hydrostatic_residual < 1.0e-10);
}

// -------------------------------------------------------- convergencia

TEST(el_error_l1_decrece_al_refinar_la_malla) {
    double previous = 0.0;
    for (const int cells : {50, 100, 200}) {
        ScratchDirectory scratch("conv_" + std::to_string(cells));
        Simulation simulation(makeConfig(6, cells, LimiterKind::VanLeer, RiemannSolverKind::HLLC),
                              scratch.path());
        const SimulationReport report = simulation.run(false);
        const double error = densityErrorL1(simulation, report.final_time);
        if (previous > 0.0) CHECK(error < previous);
        previous = error;
    }
}

TEST(muscl_converge_mas_rapido_que_primer_orden) {
    // En la onda de entropia suave, MUSCL debe mostrar un orden claramente
    // superior al de Godunov de primer orden.
    auto errorAt = [](int cells, LimiterKind limiter) {
        ScratchDirectory scratch("order_" + std::to_string(cells));
        Simulation simulation(makeConfig(6, cells, limiter, RiemannSolverKind::HLLC),
                              scratch.path());
        const SimulationReport report = simulation.run(false);
        return densityErrorL1(simulation, report.final_time);
    };

    const double order_first =
        std::log2(errorAt(50, LimiterKind::FirstOrder) / errorAt(100, LimiterKind::FirstOrder));
    const double order_muscl =
        std::log2(errorAt(50, LimiterKind::VanLeer) / errorAt(100, LimiterKind::VanLeer));

    // Primer orden: pendiente cercana a 1. MUSCL: claramente por encima.
    CHECK(order_first > 0.6);
    CHECK(order_first < 1.4);
    CHECK(order_muscl > 1.4);
    CHECK(order_muscl > order_first);
}

// ------------------------------------------- comparacion entre solvers

TEST(hllc_resuelve_el_contacto_mejor_que_hll_en_toro1) {
    auto errorFor = [](RiemannSolverKind solver) {
        ScratchDirectory scratch("solver_cmp");
        Simulation simulation(makeConfig(1, 200, LimiterKind::VanLeer, solver), scratch.path());
        const SimulationReport report = simulation.run(false);
        return densityErrorL1(simulation, report.final_time);
    };
    // HLL promedia sobre el contacto y lo difumina; HLLC lo resuelve.
    CHECK(errorFor(RiemannSolverKind::HLLC) < errorFor(RiemannSolverKind::HLL));
}

// -------------------------------------------- intercambio de componentes

TEST(el_integrador_es_intercambiable_sin_tocar_el_solver_espacial) {
    // Comprueba que la interfaz de TimeIntegrator es real: el MISMO operador
    // espacial se avanza con Euler y con SSP-RK2 sin modificarlo. Es la
    // propiedad que permitira añadir RK3 o RK4 mas adelante.
    const int cells = 64;
    const SimulationConfig config =
        makeConfig(1, cells, LimiterKind::VanLeer, RiemannSolverKind::HLLC);
    const Mesh1D mesh(config.x_min, config.x_max, cells);
    const IdealGas gas(config.gamma, config.density_floor, config.pressure_floor);
    const MusclReconstruction reconstruction(config.limiter);
    const HllcRiemannSolver riemann(gas);
    const CartesianDivergence divergence(mesh.dx());
    const TransmissiveBoundary boundary;
    FiniteVolumeOperator fv(mesh, gas, reconstruction, riemann, divergence, boundary, {},
                            HydrostaticContext{0.0, mesh.dx()});

    auto initialState = [&]() {
        SimulationState state(cells);
        applyInitialCondition(config, testCaseDefinition(config.test), mesh, state.primitive);
        for (int k = 1; k <= cells; ++k) {
            state.conserved[k] = gas.toConserved(state.primitive[k]);
        }
        fv.synchronize(state.conserved, state.primitive);
        return state;
    };

    const double dt = 1.0e-4;
    SimulationState euler_state = initialState();
    SimulationState rk2_state = initialState();
    ForwardEuler euler(cells);
    SspRk2 rk2(cells);
    euler.advance(euler_state, dt, fv);
    rk2.advance(rk2_state, dt, fv);

    CHECK(euler.name() == "euler");
    CHECK(rk2.name() == "ssp-rk2");
    bool differs = false;
    for (int k = 1; k <= cells; ++k) {
        CHECK(std::isfinite(euler_state.primitive[k].rho));
        CHECK(std::isfinite(rk2_state.primitive[k].rho));
        CHECK(euler_state.primitive[k].rho > 0.0);
        CHECK(rk2_state.primitive[k].rho > 0.0);
        if (std::abs(euler_state.conserved[k].rho - rk2_state.conserved[k].rho) > 1.0e-14) {
            differs = true;
        }
    }
    // Los dos integradores no pueden dar lo mismo: si coincidieran, la segunda
    // etapa de RK2 no se estaria aplicando.
    CHECK(differs);
}

TEST(la_salida_registra_la_configuracion_efectiva) {
    ScratchDirectory scratch("runinfo");
    Simulation simulation(makeConfig(8, 32, LimiterKind::MinMod, RiemannSolverKind::HLL),
                          scratch.path());
    simulation.run(false);
    std::ifstream info(scratch.path() / "run_info.txt");
    CHECK(info.good());
    const std::string text((std::istreambuf_iterator<char>(info)),
                           std::istreambuf_iterator<char>());
    // La geometria y la frontera impuestas por el test 8 deben aparecer ya
    // resueltas, no como las dio el usuario.
    CHECK(text.find("alpha_geo    = 2") != std::string::npos);
    CHECK(text.find("bc           = refl") != std::string::npos);
    CHECK(text.find("limiter      = mm") != std::string::npos);
    CHECK(text.find("solver       = hll") != std::string::npos);
    CHECK(text.find("xmax         = 1.2") != std::string::npos);
}
