#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "euler1d/config/simulation_config.hpp"
#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/physics/field.hpp"

namespace euler1d {

/// Escritura de resultados en el formato del Fortran.
///
/// Archivos `rho_1.xl`, `vel_1.xl`, `press_1.xl` (numerica) y
/// `rho_exact.xl`, `vel_exact.xl`, `press_exact.xl` (referencia). Cada
/// instante volcado es un bloque:
///
///     " #Time = " <es20.10>
///     <es20.10e3><es20.10e3>      (x, valor)  x N filas
///     <linea vacia>
///
/// Los archivos se abren en modo binario y se terminan las lineas con LF
/// para que sean identicos byte a byte a los del Fortran en Linux, que es
/// lo que permite la comparacion directa.
class OutputManager {
public:
    OutputManager(std::filesystem::path directory, const Mesh1D& mesh);

    /// Primer volcado: crea/trunca los tres archivos numericos.
    void writeInitial(const PrimitiveField& w, double time);
    /// Volcados sucesivos: anaden un bloque.
    void appendSnapshot(const PrimitiveField& w, double time);

    /// Escribe los tres archivos `*_exact.xl` a partir de perfiles ya
    /// muestreados en los centros de celda.
    void writeExact(const std::vector<PrimitiveState>& profile, double time);

    /// Metadatos de la corrida, para reproducibilidad.
    void writeRunInfo(const SimulationConfig& config,
                      const std::string& details,
                      long long steps,
                      double final_time,
                      double wall_seconds);

    const std::filesystem::path& directory() const { return directory_; }

private:
    void writeBlock(const std::string& filename, bool truncate,
                    const PrimitiveField* w,
                    const std::vector<PrimitiveState>* profile,
                    int component, double time);

    std::filesystem::path directory_;
    const Mesh1D* mesh_;
};

}  // namespace euler1d
