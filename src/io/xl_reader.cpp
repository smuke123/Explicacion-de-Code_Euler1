#include "euler1d/io/xl_reader.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace euler1d {

std::vector<XlBlock> readXlFile(const std::filesystem::path& path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("no se pudo abrir " + path.string());

    std::vector<XlBlock> blocks;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();

        const auto marker = line.find("#Time");
        if (marker != std::string::npos) {
            // Cabecera de bloque: " #Time = <valor>".
            const auto equals = line.find('=', marker);
            if (equals == std::string::npos) {
                throw std::runtime_error("cabecera #Time malformada en " + path.string());
            }
            XlBlock block;
            std::istringstream(line.substr(equals + 1)) >> block.time;
            blocks.push_back(std::move(block));
            continue;
        }

        double x = 0.0;
        double value = 0.0;
        std::istringstream row(line);
        if (row >> x >> value) {
            if (blocks.empty()) {
                throw std::runtime_error("datos antes de la primera cabecera en " + path.string());
            }
            blocks.back().x.push_back(x);
            blocks.back().value.push_back(value);
        }
        // Las lineas en blanco separan bloques y no requieren accion.
    }
    return blocks;
}

XlBlock readLastXlBlock(const std::filesystem::path& path) {
    std::vector<XlBlock> blocks = readXlFile(path);
    if (blocks.empty()) throw std::runtime_error("archivo .xl sin bloques: " + path.string());
    return blocks.back();
}

}  // namespace euler1d
