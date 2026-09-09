#include "euler1d/io/fortran_format.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace euler1d {

std::string formatEs(double value, int width, int decimals, int exponent_digits) {
    // `%E` de C da la mantisa en el mismo rango que el descriptor ES de
    // Fortran (un digito no nulo antes del punto), pero el numero de digitos
    // del exponente depende de la biblioteca: 2 en glibc, 3 en MSVC. Se
    // normaliza a mano para que `es20.10e3` salga identico al del Fortran.
    char buffer[64];
    std::snprintf(buffer, sizeof(buffer), "%.*E", decimals, value);
    std::string text(buffer);

    const auto marker = text.find('E');
    if (marker == std::string::npos) {
        // Solo ocurre con NaN o Inf, donde Fortran escribe su propio texto.
        if (static_cast<int>(text.size()) < width) {
            text.insert(0, static_cast<std::size_t>(width) - text.size(), ' ');
        }
        return text;
    }

    const std::string mantissa = text.substr(0, marker);
    const int exponent = std::atoi(text.c_str() + marker + 1);
    const int digits = exponent_digits > 0 ? exponent_digits : 2;

    char exponent_text[32];
    std::snprintf(exponent_text, sizeof(exponent_text), "E%c%0*d", exponent < 0 ? '-' : '+', digits,
                  std::abs(exponent));

    std::string out = mantissa + exponent_text;
    if (static_cast<int>(out.size()) < width) {
        out.insert(0, static_cast<std::size_t>(width) - out.size(), ' ');
    }
    return out;
}

std::string formatDataRow(double x, double value) {
    return formatEs(x, 20, 10, 3) + formatEs(value, 20, 10, 3);
}

std::string formatTimeHeader(double time) {
    return " #Time = " + formatEs(time, 20, 10, 0);
}

}  // namespace euler1d
