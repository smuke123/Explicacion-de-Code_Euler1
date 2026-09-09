#pragma once

#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "euler1d/boundary/boundary_condition.hpp"
#include "euler1d/config/simulation_config.hpp"
#include "euler1d/initial/test_case.hpp"
#include "euler1d/io/output_manager.hpp"
#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/numerical/cfl_controller.hpp"
#include "euler1d/numerical/finite_volume_operator.hpp"
#include "euler1d/numerical/reconstruction/reconstruction.hpp"
#include "euler1d/numerical/riemann/exact_riemann_solver.hpp"
#include "euler1d/numerical/riemann/riemann_solver.hpp"
#include "euler1d/numerical/source/source_term.hpp"
#include "euler1d/numerical/time/time_integrator.hpp"
#include "euler1d/simulation/simulation_state.hpp"

namespace euler1d {

/// Resumen de una corrida, pensado para reproducibilidad y para los tests.
struct SimulationReport {
    long long steps = 0;
    double final_time = 0.0;
    double wall_seconds = 0.0;
    std::optional<StarState> exact_star;
    /// Solo para el test 7: max|u| al final, que mide el desequilibrio
    /// hidrostatico introducido por el esquema.
    std::optional<double> hydrostatic_residual;
    FloorCounters floors;
};

/// Orquestador: construye los componentes segun la configuracion y ejecuta
/// el ciclo temporal. No contiene formulas; solo composicion y control.
class Simulation {
public:
    /// `config` debe venir ya validado y resuelto (`resolveTestCase`).
    Simulation(SimulationConfig config, std::filesystem::path output_directory);

    SimulationReport run(bool verbose = true);

    const SimulationConfig& config() const { return config_; }
    const Mesh1D& mesh() const { return mesh_; }
    const SimulationState& state() const { return state_; }
    const IdealGas& gas() const { return gas_; }

    /// Perfil de referencia muestreado en los centros de celda al tiempo `t`.
    /// Para los tests 1-5 y 8 es el Riemann exacto; para el 6 la sinusoide
    /// advectada. Devuelve nullopt si el caso no tiene referencia analitica.
    std::optional<std::vector<PrimitiveState>> referenceProfile(double time) const;

private:
    void buildComponents();
    void initialize();

    SimulationConfig config_;
    const TestCaseDefinition* test_case_;
    Mesh1D mesh_;
    IdealGas gas_;
    SimulationState state_;

    std::unique_ptr<Reconstruction> reconstruction_;
    std::unique_ptr<RiemannSolver> riemann_;
    std::unique_ptr<BoundaryCondition> boundary_;
    std::unique_ptr<DivergenceOperator> divergence_;
    std::vector<std::unique_ptr<SourceTerm>> source_storage_;
    std::vector<const SourceTerm*> sources_;
    std::unique_ptr<FiniteVolumeOperator> operator_;
    std::unique_ptr<TimeIntegrator> integrator_;
    std::unique_ptr<CflController> cfl_;
    OutputManager output_;
};

}  // namespace euler1d
