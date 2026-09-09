#include "euler1d/initial/test_case.hpp"

#include <array>
#include <cmath>

namespace euler1d {
namespace {

constexpr double kPi = 3.14159265358979323846;

/// r^n por multiplicaciones, igual que `__builtin_powi` de gfortran.
inline double ipow(double base, int exponent) {
    double result = 1.0;
    double factor = base;
    int e = exponent;
    while (e > 0) {
        if (e & 1) result *= factor;
        e >>= 1;
        if (e > 0) factor *= factor;
    }
    return result;
}

/// Tabla de casos: replica `set_test` del Fortran.
///
/// Los tests 1-5 son la Tabla 4.1 de Toro. Los tests 6-8 son extensiones del
/// proyecto para medir orden de convergencia, balance hidrostatico y
/// geometria esferica.
/// La tabla se dimensiona con `kTestCaseCount`, de modo que anadir una entrada
/// sin incrementar la constante (o al contrario) no compila.
const std::array<TestCaseDefinition, kTestCaseCount>& table() {
    static const std::array<TestCaseDefinition, kTestCaseCount> cases = {{
        {"toro1", "Sod modificado (rarefaccion sonica en el abanico)",
         {1.0, 0.75, 1.0}, {0.125, 0.0, 0.1}, 0.3, 0.2,
         std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, true},
        {"toro2", "Test 123: dos rarefacciones, region casi de vacio",
         {1.0, -2.0, 0.4}, {1.0, 2.0, 0.4}, 0.5, 0.15,
         std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, true},
        {"toro3", "Blast izquierdo (mitad izquierda de Woodward-Colella)",
         {1.0, 0.0, 1000.0}, {1.0, 0.0, 0.01}, 0.5, 0.012,
         std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, true},
        {"toro4", "Colision de los dos choques de los blasts",
         {5.99924, 19.5975, 460.894}, {5.99242, -6.19633, 46.0950}, 0.4, 0.035,
         std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, true},
        {"toro5", "Test 3 boosteado: contacto casi estacionario",
         {1.0, -19.59745, 1000.0}, {1.0, -19.59745, 0.01}, 0.8, 0.012,
         std::nullopt, std::nullopt, std::nullopt, std::nullopt, true, true},
        {"entropia", "Onda de entropia suave (estudio de convergencia)",
         {1.0, 1.0, 1.0}, {1.0, 1.0, 1.0}, 0.5, 1.0,
         BoundaryKind::Periodic, std::nullopt, std::nullopt, std::nullopt, false, false},
        {"atmosfera", "Atmosfera isoterma hidrostatica (esquema bien balanceado)",
         {1.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, 0.5, 10.0,
         BoundaryKind::Reflective, std::nullopt, 1.0, std::nullopt, false, true},
        {"sedov", "Sedov-Taylor esferico (fuente geometrica alpha = 2)",
         {1.0, 0.0, 1.0}, {1.0, 0.0, 1.0}, 0.5, 0.5,
         BoundaryKind::Reflective, GeometryKind::Spherical, std::nullopt, 1.2, false, true},
    }};
    return cases;
}

}  // namespace

const TestCaseDefinition& testCaseDefinition(int test) {
    if (test < 1 || test > kTestCaseCount) {
        throw ConfigError("test debe estar entre 1 y " + std::to_string(kTestCaseCount) +
                          "; recibido: " + std::to_string(test));
    }
    return table()[static_cast<std::size_t>(test - 1)];
}

SimulationConfig resolveTestCase(const SimulationConfig& raw, std::string* overrides_report) {
    SimulationConfig config = raw;
    const TestCaseDefinition& def = testCaseDefinition(config.test);
    std::string report;

    // Un test puede IMPONER frontera, geometria o dominio. El Fortran lo hacia
    // en silencio sobre variables globales; aqui se deja constancia cuando la
    // imposicion pisa una eleccion explicita del usuario.
    if (def.forced_boundary && config.boundary != *def.forced_boundary) {
        if (config.boundary_was_set) {
            report += "  bc: '" + toString(config.boundary) + "' -> '" +
                      toString(*def.forced_boundary) + "' (impuesto por el test " +
                      std::to_string(config.test) + ")\n";
        }
        config.boundary = *def.forced_boundary;
    }
    if (def.forced_geometry && config.geometry != *def.forced_geometry) {
        if (config.geometry_was_set) {
            report += "  alpha_geo: " + std::to_string(static_cast<int>(config.geometry)) + " -> " +
                      std::to_string(static_cast<int>(*def.forced_geometry)) +
                      " (impuesto por el test)\n";
        }
        config.geometry = *def.forced_geometry;
    }
    if (def.forced_x_max && config.x_max != *def.forced_x_max) {
        if (config.x_max_was_set) {
            report += "  xmax: " + std::to_string(config.x_max) + " -> " +
                      std::to_string(*def.forced_x_max) + " (impuesto por el test)\n";
        }
        config.x_max = *def.forced_x_max;
    }
    // La gravedad del test 7 solo se aplica si el usuario no puso ninguna:
    // el Fortran escribe `if (grav == 0.0d0) grav = 1.0d0`.
    if (def.default_gravity && config.gravity == 0.0) {
        config.gravity = *def.default_gravity;
    }

    // Centinelas negativos: el test fija el valor por defecto.
    if (config.diaphragm_x0 < 0.0) config.diaphragm_x0 = def.default_x0;
    if (config.final_time < 0.0) config.final_time = def.default_final_time;

    if (overrides_report) *overrides_report = report;
    return config;
}

void applyInitialCondition(const SimulationConfig& config, const TestCaseDefinition& test_case,
                           const Mesh1D& mesh, PrimitiveField& w) {
    const int n = mesh.cellCount();
    const double dx = mesh.dx();

    for (int k = 1; k <= n; ++k) {
        const double x = mesh.center(k);
        switch (config.test) {
            case 6:
                // Onda de entropia: densidad advectada con u = p = 1.
                w[k] = {1.0 + 0.2 * std::sin(2.0 * kPi * x), 1.0, 1.0};
                break;
            case 7:
                // Atmosfera isoterma rho = exp(-g x / T0) con T0 = 1.
                // La presion se sobreescribe abajo con el equilibrio DISCRETO.
                w[k] = {std::exp(-config.gravity * x), 0.0, 1.0};
                break;
            case 8:
                // Sedov: ambiente frio; la energia se deposita despues.
                w[k] = {1.0, 0.0, 1.0e-9};
                break;
            default:
                w[k] = (x < config.diaphragm_x0) ? test_case.left : test_case.right;
                break;
        }
    }

    if (config.test == 7) {
        // Equilibrio hidrostatico DISCRETO por regla trapezoidal, la misma
        // cuadratura que usa el esquema:  p_{i+1} = p_i - g dx (rho_i + rho_{i+1})/2.
        // Imponer el equilibrio continuo p = rho dejaria un residuo O(dx^2)
        // que el test confundiria con error del esquema.
        w[1].p = w[1].rho * 1.0;
        for (int k = 2; k <= n; ++k) {
            w[k].p = w[k - 1].p - config.gravity * dx * 0.5 * (w[k - 1].rho + w[k].rho);
        }
    }

    if (config.test == 8) {
        // E0 = 1 repartida como presion en las primeras 4 celdas:
        //   p = 3 (gamma-1) E0 / (4 pi r_ini^3),  r_ini = borde de la celda 4.
        const double r_init = mesh.center(4) + 0.5 * dx;
        const double p_hot = 3.0 * (config.gamma - 1.0) * 1.0 / (4.0 * kPi * ipow(r_init, 3));
        for (int k = 1; k <= 4; ++k) {
            w[k].p = p_hot;
        }
    }
}

}  // namespace euler1d
