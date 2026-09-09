#include "test_support.hpp"

#include "euler1d/initial/test_case.hpp"
#include "euler1d/numerical/riemann/exact_riemann_solver.hpp"

using namespace euler1d;

// El solver exacto es la referencia de toda la validacion, asi que se comprueba
// contra una fuente INDEPENDIENTE del codigo migrado: los valores estrella
// publicados por Toro (Tabla 4.3 y sec. 4.3.3). Se dan con 5-6 cifras
// significativas, de ahi la tolerancia relativa de 1e-5.

namespace {
constexpr double kToroTolerance = 1.0e-5;
const ExactRiemannSolver solver(1.4);

PrimitiveState left(int test) { return testCaseDefinition(test).left; }
PrimitiveState right(int test) { return testCaseDefinition(test).right; }
}  // namespace

TEST(toro_test1_valores_estrella_publicados) {
    const StarState star = solver.solveStar(left(1), right(1));
    CHECK(star.converged);
    CHECK(!star.vacuum);
    CHECK_REL(star.pressure, 0.46629, kToroTolerance);
    CHECK_REL(star.velocity, 1.36091, kToroTolerance);
}

TEST(toro_test2_casi_vacio_valores_estrella_publicados) {
    const StarState star = solver.solveStar(left(2), right(2));
    CHECK(star.converged);
    // p* = 0.0018939 y u* = 0 por simetria del test 123.
    CHECK_REL(star.pressure, 0.0018939, 1.0e-4);
    CHECK_NEAR(star.velocity, 0.0, 1.0e-12);
}

TEST(toro_test3_blast_valores_estrella_publicados) {
    const StarState star = solver.solveStar(left(3), right(3));
    CHECK(star.converged);
    CHECK_REL(star.pressure, 460.894, kToroTolerance);
    CHECK_REL(star.velocity, 19.5975, kToroTolerance);
}

TEST(toro_test4_colision_valores_estrella_publicados) {
    const StarState star = solver.solveStar(left(4), right(4));
    CHECK(star.converged);
    CHECK_REL(star.pressure, 1691.65, kToroTolerance);
    CHECK_REL(star.velocity, 8.68977, kToroTolerance);
}

TEST(toro_test5_contacto_estacionario) {
    const StarState star = solver.solveStar(left(5), right(5));
    CHECK(star.converged);
    CHECK_REL(star.pressure, 460.894, kToroTolerance);
    // El test 5 es el 3 desplazado en -19.59745 para dejar el contacto casi
    // quieto: u* debe salir practicamente cero.
    CHECK(std::abs(star.velocity) < 1.0e-3);
}

TEST(el_newton_converge_en_pocas_iteraciones) {
    for (int test = 1; test <= 5; ++test) {
        const StarState star = solver.solveStar(left(test), right(test));
        CHECK(star.converged);
        CHECK(star.residual <= ExactRiemannSolver::kResidualTolerance *
                                   std::max(1.0, std::abs(star.pressure)));
        // La semilla TRRS deja el Newton muy cerca: agotar 200 iteraciones
        // indicaria un problema.
        CHECK(star.iterations < 60);
    }
}

TEST(la_semilla_trrs_es_positiva_incluso_cerca_del_vacio) {
    // Test 2 es el caso duro: p* ~ 1.9e-3 partiendo de p_L = p_R = 0.4.
    const StarState star = solver.solveStar(left(2), right(2));
    CHECK(star.pressure > 0.0);
}

// ------------------------------------------------------- region de vacio

TEST(el_criterio_de_vacio_se_dispara_y_no_produce_nan) {
    // 2(a_L + a_R)/(gamma-1) = 7.483 < u_R - u_L = 10, luego hay vacio.
    const PrimitiveState l{1.0, -5.0, 0.4};
    const PrimitiveState r{1.0, 5.0, 0.4};
    const StarState star = solver.solveStar(l, r);
    CHECK(star.vacuum);
    CHECK_NEAR(star.pressure, 0.0, 0.0);

    // Sin la rama de vacio, el muestreo entraba en la formula del abanico con
    // base negativa y devolvia NaN silencioso en rho y p.
    for (double s = -12.0; s <= 12.0; s += 0.25) {
        const PrimitiveState w = solver.sample(l, r, star, s);
        CHECK(std::isfinite(w.rho));
        CHECK(std::isfinite(w.u));
        CHECK(std::isfinite(w.p));
        CHECK(w.rho >= 0.0);
        CHECK(w.p >= 0.0);
    }
    // En el centro queda vacio propiamente dicho.
    const PrimitiveState middle = solver.sample(l, r, star, 0.0);
    CHECK_NEAR(middle.rho, 0.0, 0.0);
    CHECK_NEAR(middle.p, 0.0, 0.0);
}

// ---------------------------------------------------------- muestreo

TEST(lejos_del_abanico_el_muestreo_devuelve_los_estados_iniciales) {
    for (int test = 1; test <= 5; ++test) {
        const StarState star = solver.solveStar(left(test), right(test));
        const PrimitiveState far_left = solver.sample(left(test), right(test), star, -1.0e4);
        const PrimitiveState far_right = solver.sample(left(test), right(test), star, 1.0e4);
        CHECK_NEAR(far_left.rho, left(test).rho, 1.0e-14);
        CHECK_NEAR(far_left.p, left(test).p, 1.0e-12);
        CHECK_NEAR(far_right.rho, right(test).rho, 1.0e-14);
        CHECK_NEAR(far_right.p, right(test).p, 1.0e-12);
    }
}

TEST(en_la_region_estrella_la_presion_y_la_velocidad_son_constantes) {
    // Toro test 1: entre la cola de la rarefaccion y el choque, p y u valen
    // p* y u* a ambos lados del contacto; solo salta la densidad.
    const StarState star = solver.solveStar(left(1), right(1));
    const PrimitiveState just_left = solver.sample(left(1), right(1), star, star.velocity - 1.0e-9);
    const PrimitiveState just_right = solver.sample(left(1), right(1), star, star.velocity + 1.0e-9);
    CHECK_REL(just_left.p, star.pressure, 1.0e-9);
    CHECK_REL(just_right.p, star.pressure, 1.0e-9);
    CHECK_REL(just_left.u, star.velocity, 1.0e-9);
    CHECK_REL(just_right.u, star.velocity, 1.0e-9);
    // El contacto es una discontinuidad de densidad.
    CHECK(std::abs(just_left.rho - just_right.rho) > 0.1);
}

TEST(el_muestreo_es_monotono_en_la_rarefaccion) {
    // Dentro del abanico izquierdo del test 1 la densidad debe decrecer de
    // forma monotona desde rho_L hasta el valor estrella.
    const StarState star = solver.solveStar(left(1), right(1));
    const double a_left = std::sqrt(1.4 * left(1).p / left(1).rho);
    const double head = left(1).u - a_left;
    const double tail = star.velocity -
                        a_left * std::pow(star.pressure / left(1).p, (1.4 - 1.0) / (2.0 * 1.4));
    double previous = left(1).rho + 1.0;
    for (int i = 0; i <= 20; ++i) {
        const double s = head + (tail - head) * i / 20.0;
        const PrimitiveState w = solver.sample(left(1), right(1), star, s);
        CHECK(w.rho <= previous + 1.0e-12);
        previous = w.rho;
    }
}

TEST(las_funciones_de_presion_son_continuas_en_el_cambio_de_rama) {
    // f_K cambia de choque a rarefaccion en p = p_K; ambas ramas deben
    // coincidir ahi (f = 0) y tener la misma derivada 1/(rho c).
    double f_shock = 0.0;
    double df_shock = 0.0;
    double f_rare = 0.0;
    double df_rare = 0.0;
    const double rho_k = 1.0;
    const double p_k = 2.0;
    solver.pressureFunction(p_k * (1.0 + 1.0e-12), rho_k, p_k, f_shock, df_shock);
    solver.pressureFunction(p_k * (1.0 - 1.0e-12), rho_k, p_k, f_rare, df_rare);
    CHECK_NEAR(f_shock, 0.0, 1.0e-10);
    CHECK_NEAR(f_rare, 0.0, 1.0e-10);
    CHECK_NEAR(df_shock, df_rare, 1.0e-9);
    const double c_k = std::sqrt(1.4 * p_k / rho_k);
    CHECK_NEAR(df_rare, 1.0 / (rho_k * c_k), 1.0e-9);
}
