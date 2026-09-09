#pragma once

#include <optional>
#include <string>

#include "euler1d/config/simulation_config.hpp"
#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/physics/field.hpp"
#include "euler1d/physics/ideal_gas.hpp"

namespace euler1d {

/// Definicion estatica de un caso de prueba: replica `set_test`.
///
/// Un test puede imponer condiciones de frontera, gravedad, geometria o
/// dominio. En el Fortran eso se hacia mutando variables globales; aqui es
/// un dato declarativo que se aplica en `resolveTestCase`.
struct TestCaseDefinition {
    std::string name;
    std::string description;
    PrimitiveState left{};
    PrimitiveState right{};
    double default_x0 = 0.5;
    double default_final_time = 1.0;
    std::optional<BoundaryKind> forced_boundary;
    std::optional<GeometryKind> forced_geometry;
    /// Gravedad impuesta solo si el usuario dejo `grav = 0` (test 7).
    std::optional<double> default_gravity;
    std::optional<double> forced_x_max;
    /// Los tests 6, 7 y 8 no usan un diafragma: tienen perfil propio.
    bool uses_riemann_initial_data = true;
    /// La solucion de referencia es un problema de Riemann exacto.
    bool has_exact_riemann_reference = true;
};

/// Devuelve la definicion del test 1..8. Lanza ConfigError fuera de rango.
const TestCaseDefinition& testCaseDefinition(int test);

/// Aplica al config los valores derivados del test (x0, tf, bc, grav, ...).
///
/// Corresponde a `set_test`: es el unico punto donde el caso de prueba puede
/// cambiar la configuracion, y deja registro de lo que sobreescribio.
SimulationConfig resolveTestCase(const SimulationConfig& raw, std::string* overrides_report = nullptr);

/// Rellena las primitivas de las celdas fisicas con la condicion inicial.
void applyInitialCondition(const SimulationConfig& config,
                           const TestCaseDefinition& test_case,
                           const Mesh1D& mesh,
                           PrimitiveField& w);

}  // namespace euler1d
