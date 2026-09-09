#pragma once

#include <filesystem>
#include <stdexcept>
#include <string>

namespace euler1d {

/// Error de configuracion. Nunca se degrada a un valor por defecto:
/// el Fortran demostro que una errata de un caracter podia hacer correr
/// otro experimento en silencio.
class ConfigError : public std::runtime_error {
public:
    explicit ConfigError(const std::string& message) : std::runtime_error(message) {}
};

/// Numero de casos de prueba definidos (`test = 1 .. kTestCaseCount`).
///
/// Es la UNICA declaracion de este limite. `test_case.cpp` dimensiona su tabla
/// con esta constante, asi que anadir un caso sin actualizarla —o al reves— es
/// un error de compilacion, no un caso que se rechaza en tiempo de ejecucion.
/// Ver docs/extending.md.
inline constexpr int kTestCaseCount = 8;

enum class LimiterKind { FirstOrder, MinMod, VanLeer, MonotonizedCentral, Superbee };
enum class RiemannSolverKind { HLL, HLLC };
enum class BoundaryKind { Transmissive, Periodic, Reflective };
enum class GeometryKind { Planar = 0, Cylindrical = 1, Spherical = 2 };

std::string toString(LimiterKind kind);
std::string toString(RiemannSolverKind kind);
std::string toString(BoundaryKind kind);

/// Parametros de una corrida. Sustituye al modulo global `global_numbers`:
/// se pasa por valor/const-ref, no existe estado global.
///
/// Los centinelas negativos de `x0` y `final_time` significan "usar el valor
/// por defecto del test", igual que en el Fortran.
struct SimulationConfig {
    int test = 1;
    int cell_count = 400;
    double cfl = 0.9;
    double gamma = 1.4;
    LimiterKind limiter = LimiterKind::VanLeer;
    RiemannSolverKind solver = RiemannSolverKind::HLLC;
    BoundaryKind boundary = BoundaryKind::Transmissive;
    double gravity = 0.0;
    GeometryKind geometry = GeometryKind::Planar;
    double x_min = 0.0;
    double x_max = 1.0;
    double diaphragm_x0 = -1.0;
    double final_time = -1.0;
    int output_every = 0;

    /// Pisos de positividad (parametros del Fortran, no del namelist).
    double density_floor = 1.0e-12;
    double pressure_floor = 1.0e-12;

    /// Marcas de que campos fijo explicitamente el usuario. Sirven para
    /// avisar cuando `set_test` sobreescribe una eleccion del usuario.
    bool boundary_was_set = false;
    bool gravity_was_set = false;
    bool geometry_was_set = false;
    bool x_max_was_set = false;

    /// Comprueba rangos y valores admisibles. Replica `validate_input`.
    void validate() const;
};

/// Lee un namelist Fortran `&euler ... /`.
///
/// Se mantiene el formato original para que un `input.par` del proyecto
/// Fortran sirva sin cambios. El parser es estricto igual que la lectura de
/// namelist de gfortran: una clave desconocida es un error, no un aviso.
SimulationConfig readConfigFile(const std::filesystem::path& path);

/// Variante para tests: parsea desde una cadena en memoria.
SimulationConfig parseNamelist(const std::string& text, const std::string& origin = "<memoria>");

/// Resumen de una linea equivalente al `config:` que imprime el Fortran.
std::string describe(const SimulationConfig& config);

}  // namespace euler1d
