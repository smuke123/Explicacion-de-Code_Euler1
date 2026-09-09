#pragma once

#include <string>

namespace euler1d {

/// Reproduce el descriptor de edicion `ESw.dEe` de Fortran.
///
/// Se necesita literalmente: los archivos `.xl` del Fortran usan
/// `es20.10e3` para los datos y `es20.10` para la cabecera de tiempo, y la
/// validacion compara los archivos producidos por ambos codigos. `printf`
/// de C no garantiza el numero de digitos del exponente (2 en glibc, 3 en
/// MSVC), asi que se normaliza a mano.
///
/// `exponent_digits <= 0` reproduce el minimo de Fortran (2 digitos).
std::string formatEs(double value, int width, int decimals, int exponent_digits);

/// Formato de una fila de datos: dos campos `es20.10e3` concatenados.
std::string formatDataRow(double x, double value);

/// Cabecera de bloque temporal: `" #Time = "` seguido de `es20.10`.
std::string formatTimeHeader(double time);

}  // namespace euler1d
