#pragma once

// Arnes de pruebas minimo.
//
// DECISION: no se añade GoogleTest ni Catch2. El encargo pide no introducir
// dependencias externas innecesarias, y lo unico que hace falta son casos
// registrados, comparaciones con tolerancia y un codigo de salida para CTest.
// Cada archivo de test es un ejecutable independiente y define su propio main
// al incluir esta cabecera.

#include <cmath>
#include <cstdio>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace testing {

struct Failure {
    std::string message;
};

struct Case {
    std::string name;
    std::function<void()> body;
};

inline std::vector<Case>& registry() {
    static std::vector<Case> cases;
    return cases;
}

struct Registrar {
    Registrar(std::string name, std::function<void()> body) {
        registry().push_back({std::move(name), std::move(body)});
    }
};

[[noreturn]] inline void fail(const std::string& file, int line, const std::string& message) {
    throw Failure{file + ":" + std::to_string(line) + ": " + message};
}

inline std::string describeNear(const char* a_text, double a, const char* b_text, double b,
                                double tolerance) {
    char buffer[512];
    std::snprintf(buffer, sizeof(buffer),
                  "%s = %.17g\n           %s = %.17g\n           |diff| = %.3e > tol = %.3e",
                  a_text, a, b_text, b, std::abs(a - b), tolerance);
    return buffer;
}

inline int runAll() {
    int failures = 0;
    for (const Case& test : registry()) {
        try {
            test.body();
            std::printf("[  OK  ] %s\n", test.name.c_str());
        } catch (const Failure& failure) {
            std::printf("[ FAIL ] %s\n           %s\n", test.name.c_str(),
                        failure.message.c_str());
            ++failures;
        } catch (const std::exception& error) {
            std::printf("[ FAIL ] %s\n           excepcion inesperada: %s\n", test.name.c_str(),
                        error.what());
            ++failures;
        }
    }
    std::printf("\n%zu casos, %d fallos\n", registry().size(), failures);
    return failures == 0 ? 0 : 1;
}

}  // namespace testing

#define TEST(name)                                                     \
    static void name();                                                \
    static ::testing::Registrar registrar_##name(#name, name);         \
    static void name()

#define CHECK(condition)                                                          \
    do {                                                                          \
        if (!(condition)) ::testing::fail(__FILE__, __LINE__, "CHECK(" #condition ")"); \
    } while (false)

#define CHECK_NEAR(a, b, tolerance)                                                        \
    do {                                                                                   \
        const double check_a_ = (a);                                                       \
        const double check_b_ = (b);                                                       \
        const double check_tol_ = (tolerance);                                             \
        if (!(std::abs(check_a_ - check_b_) <= check_tol_)) {                              \
            ::testing::fail(__FILE__, __LINE__,                                            \
                            ::testing::describeNear(#a, check_a_, #b, check_b_, check_tol_)); \
        }                                                                                  \
    } while (false)

/// Compara con tolerancia RELATIVA a la escala de la referencia.
#define CHECK_REL(value, reference, relative_tolerance)                                    \
    do {                                                                                   \
        const double rel_v_ = (value);                                                     \
        const double rel_r_ = (reference);                                                 \
        const double rel_t_ = (relative_tolerance);                                        \
        const double rel_scale_ = std::abs(rel_r_) > 0.0 ? std::abs(rel_r_) : 1.0;         \
        if (!(std::abs(rel_v_ - rel_r_) / rel_scale_ <= rel_t_)) {                         \
            ::testing::fail(__FILE__, __LINE__,                                            \
                            ::testing::describeNear(#value, rel_v_, #reference, rel_r_,    \
                                                    rel_t_ * rel_scale_));                 \
        }                                                                                  \
    } while (false)

#define CHECK_THROWS(expression, ExceptionType)                                          \
    do {                                                                                 \
        bool threw_ = false;                                                             \
        try {                                                                            \
            (void)(expression);                                                           \
        } catch (const ExceptionType&) {                                                 \
            threw_ = true;                                                               \
        } catch (...) {                                                                  \
            ::testing::fail(__FILE__, __LINE__,                                          \
                            "se lanzo una excepcion de otro tipo en " #expression);      \
        }                                                                                \
        if (!threw_) {                                                                   \
            ::testing::fail(__FILE__, __LINE__,                                          \
                            "se esperaba " #ExceptionType " en " #expression);           \
        }                                                                                \
    } while (false)

int main() { return ::testing::runAll(); }
