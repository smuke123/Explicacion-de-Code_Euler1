#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <utility>
#include <vector>

#include "euler1d/config/simulation_config.hpp"

// Parser del namelist Fortran `&euler ... /`.
//
// DECISION DE DISENO: se conserva el formato original en lugar de inventar
// uno nuevo (JSON/TOML) para que cualquier `input.par` del proyecto Fortran
// —incluidos los archivados en runs/— sirva sin editarlo. Eso es lo que
// permite validar ambos codigos con EL MISMO archivo de entrada.
//
// El parser es deliberadamente MAS estricto que gfortran en un punto: una
// clave desconocida es un error. En el Fortran, la lectura de namelist es
// secuencial y un error a media lista deja asignado lo anterior y el resto en
// su valor por defecto; el codigo original detecta el `iostat` y aborta, pero
// una clave mal escrita que gfortran acepte pasaria inadvertida.

namespace euler1d {
namespace {

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

std::string trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto begin = s.find_first_not_of(ws);
    if (begin == std::string::npos) return {};
    const auto end = s.find_last_not_of(ws);
    return s.substr(begin, end - begin + 1);
}

[[noreturn]] void fail(const std::string& origin, const std::string& message) {
    throw ConfigError(origin + ": " + message);
}

/// Quita comillas simples o dobles de un valor de cadena.
std::string unquote(const std::string& raw, const std::string& key, const std::string& origin) {
    if (raw.size() >= 2 && (raw.front() == '\'' || raw.front() == '"') && raw.back() == raw.front()) {
        return raw.substr(1, raw.size() - 2);
    }
    fail(origin, "el valor de '" + key + "' debe ir entre comillas; recibido: " + raw);
}

/// Acepta la notacion de doble precision de Fortran (`0.9d0`).
double parseReal(const std::string& raw, const std::string& key, const std::string& origin) {
    std::string text = raw;
    for (char& c : text) {
        if (c == 'd' || c == 'D') c = 'e';
    }
    try {
        std::size_t consumed = 0;
        const double value = std::stod(text, &consumed);
        if (trim(text.substr(consumed)).empty()) return value;
    } catch (const std::exception&) {
    }
    fail(origin, "'" + key + "' no es un numero real valido: " + raw);
}

int parseInt(const std::string& raw, const std::string& key, const std::string& origin) {
    try {
        std::size_t consumed = 0;
        const int value = std::stoi(raw, &consumed);
        if (trim(raw.substr(consumed)).empty()) return value;
    } catch (const std::exception&) {
    }
    fail(origin, "'" + key + "' no es un entero valido: " + raw);
}

LimiterKind parseLimiter(const std::string& text, const std::string& origin) {
    if (text == "fo") return LimiterKind::FirstOrder;
    if (text == "mm") return LimiterKind::MinMod;
    if (text == "vl") return LimiterKind::VanLeer;
    if (text == "mc") return LimiterKind::MonotonizedCentral;
    if (text == "sb") return LimiterKind::Superbee;
    fail(origin, "limiter debe ser fo|mm|vl|mc|sb; recibido: '" + text + "'");
}

RiemannSolverKind parseSolver(const std::string& text, const std::string& origin) {
    if (text == "hll") return RiemannSolverKind::HLL;
    if (text == "hllc") return RiemannSolverKind::HLLC;
    fail(origin, "solver debe ser hll|hllc; recibido: '" + text + "'");
}

BoundaryKind parseBoundary(const std::string& text, const std::string& origin) {
    if (text == "trans") return BoundaryKind::Transmissive;
    if (text == "per") return BoundaryKind::Periodic;
    if (text == "refl") return BoundaryKind::Reflective;
    fail(origin, "bc debe ser trans|per|refl; recibido: '" + text + "'");
}

GeometryKind parseGeometry(int alpha, const std::string& origin) {
    switch (alpha) {
        case 0: return GeometryKind::Planar;
        case 1: return GeometryKind::Cylindrical;
        case 2: return GeometryKind::Spherical;
        default: break;
    }
    fail(origin, "alpha_geo debe ser 0, 1 o 2; recibido: " + std::to_string(alpha));
}

/// Elimina el comentario final (`!`) respetando el contenido entre comillas.
std::string stripComment(const std::string& line) {
    char quote = '\0';
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (quote != '\0') {
            if (c == quote) quote = '\0';
        } else if (c == '\'' || c == '"') {
            quote = c;
        } else if (c == '!') {
            return line.substr(0, i);
        }
    }
    return line;
}

/// Trocea el cuerpo del namelist en asignaciones `clave = valor`.
///
/// Los separadores validos son coma y salto de linea, igual que en Fortran.
std::vector<std::pair<std::string, std::string>> splitAssignments(const std::string& body,
                                                                  const std::string& origin) {
    std::vector<std::pair<std::string, std::string>> result;
    std::string current;
    char quote = '\0';
    auto flush = [&]() {
        const std::string item = trim(current);
        current.clear();
        if (item.empty()) return;
        const auto eq = item.find('=');
        if (eq == std::string::npos) {
            fail(origin, "se esperaba 'clave = valor'; recibido: " + item);
        }
        result.emplace_back(toLower(trim(item.substr(0, eq))), trim(item.substr(eq + 1)));
    };

    for (const char c : body) {
        if (quote != '\0') {
            current.push_back(c);
            if (c == quote) quote = '\0';
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            current.push_back(c);
        } else if (c == ',' || c == '\n') {
            flush();
        } else {
            current.push_back(c);
        }
    }
    flush();
    return result;
}

}  // namespace

SimulationConfig parseNamelist(const std::string& text, const std::string& origin) {
    std::string clean;
    clean.reserve(text.size());
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        clean += stripComment(line);
        clean.push_back('\n');
    }

    const auto group = toLower(clean).find("&euler");
    if (group == std::string::npos) {
        fail(origin, "no se encontro el grupo '&euler'");
    }
    const auto body_begin = group + std::string("&euler").size();
    // El terminador de un namelist es '/' en su propia posicion; se busca el
    // primero que aparezca fuera de comillas.
    std::size_t body_end = std::string::npos;
    {
        char quote = '\0';
        for (std::size_t i = body_begin; i < clean.size(); ++i) {
            const char c = clean[i];
            if (quote != '\0') {
                if (c == quote) quote = '\0';
            } else if (c == '\'' || c == '"') {
                quote = c;
            } else if (c == '/') {
                body_end = i;
                break;
            }
        }
    }
    if (body_end == std::string::npos) {
        fail(origin, "el grupo '&euler' no esta cerrado con '/'");
    }

    SimulationConfig config;
    bool alpha_seen = false;
    int alpha_value = 0;

    for (const auto& [key, raw] : splitAssignments(clean.substr(body_begin, body_end - body_begin),
                                                   origin)) {
        if (raw.empty()) fail(origin, "'" + key + "' no tiene valor");

        if (key == "test") {
            config.test = parseInt(raw, key, origin);
        } else if (key == "n") {
            config.cell_count = parseInt(raw, key, origin);
        } else if (key == "cfl") {
            config.cfl = parseReal(raw, key, origin);
        } else if (key == "gamma") {
            config.gamma = parseReal(raw, key, origin);
        } else if (key == "limiter") {
            config.limiter = parseLimiter(toLower(trim(unquote(raw, key, origin))), origin);
        } else if (key == "solver") {
            config.solver = parseSolver(toLower(trim(unquote(raw, key, origin))), origin);
        } else if (key == "bc") {
            config.boundary = parseBoundary(toLower(trim(unquote(raw, key, origin))), origin);
            config.boundary_was_set = true;
        } else if (key == "grav") {
            config.gravity = parseReal(raw, key, origin);
            config.gravity_was_set = true;
        } else if (key == "alpha_geo") {
            alpha_value = parseInt(raw, key, origin);
            alpha_seen = true;
        } else if (key == "xmin") {
            config.x_min = parseReal(raw, key, origin);
        } else if (key == "xmax") {
            config.x_max = parseReal(raw, key, origin);
            config.x_max_was_set = true;
        } else if (key == "x0") {
            config.diaphragm_x0 = parseReal(raw, key, origin);
        } else if (key == "tf") {
            config.final_time = parseReal(raw, key, origin);
        } else if (key == "every_out") {
            config.output_every = parseInt(raw, key, origin);
        } else if (key == "rho_floor") {
            config.density_floor = parseReal(raw, key, origin);
        } else if (key == "p_floor") {
            config.pressure_floor = parseReal(raw, key, origin);
        } else {
            fail(origin, "clave desconocida en &euler: '" + key + "'");
        }
    }

    if (alpha_seen) {
        config.geometry = parseGeometry(alpha_value, origin);
        config.geometry_was_set = true;
    }
    return config;
}

SimulationConfig readConfigFile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file) {
        throw ConfigError("no se pudo abrir el archivo de configuracion: " + path.string());
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return parseNamelist(buffer.str(), path.string());
}

}  // namespace euler1d
