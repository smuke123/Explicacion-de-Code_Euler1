#pragma once

#include <filesystem>
#include <vector>

namespace euler1d {

/// Un bloque temporal de un archivo `.xl`.
struct XlBlock {
    double time{};
    std::vector<double> x;
    std::vector<double> value;

    std::size_t size() const { return x.size(); }
};

/// Lee todos los bloques de un archivo `.xl` (formato de salida del Fortran).
/// Se usa en los tests de regresion y en la herramienta de comparacion.
std::vector<XlBlock> readXlFile(const std::filesystem::path& path);

/// Devuelve el ultimo bloque. Lanza std::runtime_error si el archivo esta vacio.
XlBlock readLastXlBlock(const std::filesystem::path& path);

}  // namespace euler1d
