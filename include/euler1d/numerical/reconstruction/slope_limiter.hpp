#pragma once

#include <algorithm>
#include <cmath>

#include "euler1d/config/simulation_config.hpp"

namespace euler1d {

/// Limitadores de pendiente con las MISMAS formulas que `reconstructor` del
/// Fortran: `a` es la diferencia hacia atras y `b` la diferencia hacia
/// delante de la variable reconstruida.
///
/// Son tipos-politica sin estado y con metodos estaticos `inline`: el kernel
/// plantilla de `MusclReconstruction` los instancia una vez por barrido, de
/// modo que la eleccion del limitador no cuesta ninguna comparacion dentro
/// del bucle de celdas. El Fortran hacia `select case (trim(limiter))` en el
/// bucle interno, es decir una comparacion de cadenas por celda y componente.
namespace limiters {

/// Signo con la convencion de Fortran: `sign(1.0d0, x)` vale +1 cuando x = 0.
/// Esto importa en minmod y superbee, donde `0.5*(sign(a)+sign(b))` decide si
/// la pendiente se anula.
inline double fortranSign(double x) { return x >= 0.0 ? 1.0 : -1.0; }

struct FirstOrder {
    static double slope(double, double) { return 0.0; }
    static constexpr const char* name = "fo";
};

struct MinMod {
    static double slope(double a, double b) {
        return 0.5 * (fortranSign(a) + fortranSign(b)) * std::min(std::abs(a), std::abs(b));
    }
    static constexpr const char* name = "mm";
};

/// van Leer en forma de media armonica.
struct VanLeer {
    static double slope(double a, double b) {
        if (a * b > 0.0) {
            return (a * std::abs(b) + b * std::abs(a)) / (std::abs(a) + std::abs(b));
        }
        return 0.0;
    }
    static constexpr const char* name = "vl";
};

struct MonotonizedCentral {
    static double slope(double a, double b) {
        if (a * b > 0.0) {
            const double c = 0.5 * (a + b);
            return fortranSign(c) * std::min({std::abs(c), 2.0 * std::abs(a), 2.0 * std::abs(b)});
        }
        return 0.0;
    }
    static constexpr const char* name = "mc";
};

struct Superbee {
    static double slope(double a, double b) {
        const double s1 =
            0.5 * (fortranSign(2.0 * a) + fortranSign(b)) * std::min(2.0 * std::abs(a), std::abs(b));
        const double s2 =
            0.5 * (fortranSign(a) + fortranSign(2.0 * b)) * std::min(std::abs(a), 2.0 * std::abs(b));
        if (s1 * s2 > 0.0) {
            return fortranSign(s1) * std::max(std::abs(s1), std::abs(s2));
        }
        return 0.0;
    }
    static constexpr const char* name = "sb";
};

}  // namespace limiters

/// Despacho dinamico del limitador. Pensado para tests y diagnostico:
/// dentro de los bucles numericos se usa el kernel plantilla.
double limitedSlope(LimiterKind kind, double a, double b);

}  // namespace euler1d
