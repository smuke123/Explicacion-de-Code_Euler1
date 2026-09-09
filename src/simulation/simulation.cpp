#include "euler1d/simulation/simulation.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <utility>

#include "euler1d/io/fortran_format.hpp"
#include "euler1d/numerical/riemann/hll.hpp"
#include "euler1d/numerical/riemann/hllc.hpp"

namespace euler1d {
namespace {

constexpr double kPi = 3.14159265358979323846;

}  // namespace

Simulation::Simulation(SimulationConfig config, std::filesystem::path output_directory)
    : config_(std::move(config)),
      test_case_(&testCaseDefinition(config_.test)),
      mesh_(config_.x_min, config_.x_max, config_.cell_count),
      gas_(config_.gamma, config_.density_floor, config_.pressure_floor),
      state_(config_.cell_count),
      output_(std::move(output_directory), mesh_) {
    buildComponents();
    initialize();
}

void Simulation::buildComponents() {
    // Toda la seleccion de algoritmo ocurre AQUI, una sola vez por corrida.
    // Los bucles numericos reciben ya el componente concreto: no queda ninguna
    // comparacion de cadenas ni busqueda de opcion dentro del ciclo temporal.
    reconstruction_ = std::make_unique<MusclReconstruction>(config_.limiter);

    switch (config_.solver) {
        case RiemannSolverKind::HLL:
            riemann_ = std::make_unique<HllRiemannSolver>(gas_);
            break;
        case RiemannSolverKind::HLLC:
            riemann_ = std::make_unique<HllcRiemannSolver>(gas_);
            break;
    }

    switch (config_.boundary) {
        case BoundaryKind::Transmissive:
            boundary_ = std::make_unique<TransmissiveBoundary>();
            break;
        case BoundaryKind::Periodic:
            boundary_ = std::make_unique<PeriodicBoundary>();
            break;
        case BoundaryKind::Reflective:
            boundary_ = std::make_unique<ReflectiveBoundary>(gas_, config_.gravity, mesh_.dx());
            break;
    }

    const int alpha = static_cast<int>(config_.geometry);
    if (alpha == 0) {
        divergence_ = std::make_unique<CartesianDivergence>(mesh_.dx());
    } else {
        divergence_ = std::make_unique<RadialDivergence>(mesh_, alpha);
    }

    if (config_.gravity != 0.0) {
        source_storage_.push_back(std::make_unique<GravitySource>(config_.gravity));
    }
    sources_.clear();
    for (const auto& source : source_storage_) {
        sources_.push_back(source.get());
    }

    operator_ = std::make_unique<FiniteVolumeOperator>(
        mesh_, gas_, *reconstruction_, *riemann_, *divergence_, *boundary_, sources_,
        HydrostaticContext{config_.gravity, mesh_.dx()});
    integrator_ = std::make_unique<SspRk2>(config_.cell_count);
    cfl_ = std::make_unique<CflController>(config_.cfl, mesh_, gas_);
}

void Simulation::initialize() {
    applyInitialCondition(config_, *test_case_, mesh_, state_.primitive);
    const int n = mesh_.cellCount();
    for (int k = 1; k <= n; ++k) {
        state_.conserved[k] = gas_.toConserved(state_.primitive[k]);
    }
    boundary_->apply(state_.conserved, state_.primitive);
    state_.time = 0.0;
    state_.step = 0;
}

std::optional<std::vector<PrimitiveState>> Simulation::referenceProfile(double time) const {
    const int n = mesh_.cellCount();
    std::vector<PrimitiveState> profile(static_cast<std::size_t>(n));

    if (config_.test == 6) {
        // Exacta del test 6: la sinusoide simplemente advectada, rho(x - u t).
        for (int k = 1; k <= n; ++k) {
            const double arg = 2.0 * kPi * (mesh_.center(k) - time);
            profile[static_cast<std::size_t>(k - 1)] = {1.0 + 0.2 * std::sin(arg), 1.0, 1.0};
        }
        return profile;
    }

    if (!test_case_->has_exact_riemann_reference) return std::nullopt;

    const ExactRiemannSolver exact(config_.gamma);
    const StarState star = exact.solveStar(test_case_->left, test_case_->right);
    for (int k = 1; k <= n; ++k) {
        const double s = (mesh_.center(k) - config_.diaphragm_x0) / std::max(time, 1.0e-30);
        profile[static_cast<std::size_t>(k - 1)] =
            exact.sample(test_case_->left, test_case_->right, star, s);
    }
    return profile;
}

SimulationReport Simulation::run(bool verbose) {
    using clock = std::chrono::steady_clock;
    const auto wall_start = clock::now();

    SimulationReport report;
    output_.writeInitial(state_.primitive, state_.time);

    const double final_time = config_.final_time;
    long long step = 0;
    // El Fortran NO inicializa `ultimo_guardado` antes del bucle; si el bucle
    // no llega a ejecutarse (tf <= 0) la comparacion posterior lee una
    // variable indefinida. Aqui se inicializa a -1, que reproduce el caso
    // normal y hace determinista el degenerado (ver docs/inconsistencies.md).
    long long last_saved = -1;

    while (state_.time < final_time) {
        double dt = cfl_->computeTimeStep(state_.primitive);
        // Recorte del ultimo paso para aterrizar exactamente en tf.
        if (state_.time + dt > final_time) dt = final_time - state_.time;

        integrator_->advance(state_, dt, *operator_);

        state_.time += dt;
        ++step;
        state_.step = step;
        last_saved = -1;
        if (config_.output_every > 0 && step % config_.output_every == 0) {
            output_.appendSnapshot(state_.primitive, state_.time);
            last_saved = step;
        }
    }

    // No repetir el bloque si el ultimo paso ya se volco.
    if (last_saved != step) output_.appendSnapshot(state_.primitive, state_.time);

    if (config_.test == 6) {
        const auto profile = referenceProfile(state_.time);
        if (profile) output_.writeExact(*profile, state_.time);
    } else {
        const ExactRiemannSolver exact(config_.gamma);
        const StarState star = exact.solveStar(test_case_->left, test_case_->right);
        report.exact_star = star;
        if (verbose) {
            std::printf(" exacto: p* =%s   u* =%s\n",
                        formatEs(star.pressure, 14, 6, 0).c_str(),
                        formatEs(star.velocity, 14, 6, 0).c_str());
            if (!star.converged) {
                std::printf(" AVISO exact_star: Newton no convergio, |f(p*)| =%s  p* =%s\n",
                            formatEs(star.residual, 12, 4, 0).c_str(),
                            formatEs(star.pressure, 12, 4, 0).c_str());
            }
        }
        const auto profile = referenceProfile(state_.time);
        if (profile) output_.writeExact(*profile, state_.time);
    }

    if (config_.test == 7) {
        double max_abs_u = 0.0;
        for (int k = 1; k <= mesh_.cellCount(); ++k) {
            max_abs_u = std::max(max_abs_u, std::abs(state_.primitive[k].u));
        }
        report.hydrostatic_residual = max_abs_u;
        if (verbose) {
            std::printf(" balance hidrostatico: max|u| = %s\n",
                        formatEs(max_abs_u, 12, 4, 0).c_str());
        }
    }

    report.steps = step;
    report.final_time = state_.time;
    report.floors = operator_->floorCounters();
    report.wall_seconds =
        std::chrono::duration<double>(clock::now() - wall_start).count();

    if (verbose) {
        std::printf(" pasos: %6lld   t final: %10.6f\n", step, state_.time);
        if (report.floors.triggered()) {
            // Un piso que se dispara no es "proteccion exitosa": avisa de que
            // el resultado pudo perder validez fisica.
            std::printf(
                " AVISO pisos de positividad activados: rho=%llu  p=%llu"
                "  (e_int especifica minima = %.3e)\n",
                static_cast<unsigned long long>(report.floors.density),
                static_cast<unsigned long long>(report.floors.pressure),
                report.floors.min_internal_energy);
        }
    }

    std::string details;
    if (report.exact_star) {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
                      "exact_p_star = %.10e\nexact_u_star = %.10e\nexact_residual = %.3e\n",
                      report.exact_star->pressure, report.exact_star->velocity,
                      report.exact_star->residual);
        details += buffer;
    }
    if (report.hydrostatic_residual) {
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "hydrostatic_max_abs_u = %.6e\n",
                      *report.hydrostatic_residual);
        details += buffer;
    }
    {
        char buffer[256];
        std::snprintf(buffer, sizeof(buffer),
                      "floor_rho_hits = %llu\nfloor_p_hits = %llu\nreconstruction = %s\n"
                      "riemann = %s\nintegrator = %s\ndivergence = %s\n",
                      static_cast<unsigned long long>(report.floors.density),
                      static_cast<unsigned long long>(report.floors.pressure),
                      reconstruction_->name().c_str(), riemann_->name().c_str(),
                      integrator_->name().c_str(), divergence_->name().c_str());
        details += buffer;
    }
    output_.writeRunInfo(config_, details, report.steps, report.final_time, report.wall_seconds);

    return report;
}

}  // namespace euler1d
