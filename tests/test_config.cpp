#include "test_support.hpp"

#include "euler1d/initial/test_case.hpp"

using namespace euler1d;

namespace {

const char* kMinimal = "&euler\n test = 1\n N = 400\n cfl = 0.9\n gamma = 1.4\n"
                       " limiter = 'vl'\n solver = 'hllc'\n/\n";

}  // namespace

TEST(lee_el_namelist_del_proyecto_fortran_sin_cambios) {
    const SimulationConfig config = parseNamelist(kMinimal);
    CHECK(config.test == 1);
    CHECK(config.cell_count == 400);
    CHECK_NEAR(config.cfl, 0.9, 0.0);
    CHECK_NEAR(config.gamma, 1.4, 0.0);
    CHECK(config.limiter == LimiterKind::VanLeer);
    CHECK(config.solver == RiemannSolverKind::HLLC);
}

TEST(acepta_la_notacion_de_doble_precision_de_fortran) {
    const SimulationConfig config =
        parseNamelist("&euler\n cfl = 0.5d0\n gamma = 1.6666666667d0\n/\n");
    CHECK_NEAR(config.cfl, 0.5, 0.0);
    CHECK_NEAR(config.gamma, 1.6666666667, 1.0e-15);
}

TEST(las_claves_no_distinguen_mayusculas_como_en_fortran) {
    const SimulationConfig config = parseNamelist("&euler\n TEST = 3\n n = 50\n CFL = 0.4\n/\n");
    CHECK(config.test == 3);
    CHECK(config.cell_count == 50);
    CHECK_NEAR(config.cfl, 0.4, 0.0);
}

TEST(ignora_comentarios_y_comas_separadoras) {
    const SimulationConfig config = parseNamelist(
        "&euler\n test = 2, N = 64  ! comentario al final\n"
        " ! linea entera de comentario\n limiter = 'mm'\n/\n");
    CHECK(config.test == 2);
    CHECK(config.cell_count == 64);
    CHECK(config.limiter == LimiterKind::MinMod);
}

// -------------------------------------------------- errores explicitos

TEST(una_clave_desconocida_es_un_error) {
    // El punto clave de la migracion: nada se degrada a un valor por defecto.
    CHECK_THROWS(parseNamelist("&euler\n test = 1\n limitador = 'vl'\n/\n"), ConfigError);
}

TEST(un_solver_inexistente_es_un_error_y_no_selecciona_otro) {
    CHECK_THROWS(parseNamelist("&euler\n solver = 'roe'\n/\n"), ConfigError);
    CHECK_THROWS(parseNamelist("&euler\n solver = 'hlle'\n/\n"), ConfigError);
}

TEST(un_limitador_inexistente_es_un_error) {
    CHECK_THROWS(parseNamelist("&euler\n limiter = 'weno'\n/\n"), ConfigError);
}

TEST(una_frontera_inexistente_es_un_error) {
    CHECK_THROWS(parseNamelist("&euler\n bc = 'outflow'\n/\n"), ConfigError);
}

TEST(alpha_geo_solo_admite_0_1_o_2) {
    CHECK_THROWS(parseNamelist("&euler\n alpha_geo = 3\n/\n"), ConfigError);
    CHECK(parseNamelist("&euler\n alpha_geo = 2\n/\n").geometry == GeometryKind::Spherical);
}

TEST(el_grupo_euler_debe_existir_y_estar_cerrado) {
    CHECK_THROWS(parseNamelist(" test = 1\n"), ConfigError);
    CHECK_THROWS(parseNamelist("&euler\n test = 1\n"), ConfigError);
}

TEST(una_cadena_sin_comillas_es_un_error) {
    CHECK_THROWS(parseNamelist("&euler\n limiter = vl\n/\n"), ConfigError);
}

TEST(un_valor_no_numerico_es_un_error) {
    CHECK_THROWS(parseNamelist("&euler\n cfl = mucho\n/\n"), ConfigError);
    CHECK_THROWS(parseNamelist("&euler\n N = 12.5\n/\n"), ConfigError);
}

// El Fortran compara `trim(solver)` con 'hll' de forma sensible a mayusculas,
// asi que rechaza 'HLL'. Aqui se acepta: los namelists de Fortran son
// insensibles a mayusculas en las CLAVES, y aceptar 'HLL' nunca selecciona
// otro algoritmo, solo el que el usuario pidio. Documentado como desviacion
// deliberada en docs/inconsistencies.md.
TEST(el_valor_del_solver_es_insensible_a_mayusculas) {
    CHECK(parseNamelist("&euler\n solver = 'HLL'\n/\n").solver == RiemannSolverKind::HLL);
    CHECK(parseNamelist("&euler\n solver = 'hll'\n/\n").solver == RiemannSolverKind::HLL);
}

// ------------------------------------------------------- validate()

TEST(validate_rechaza_rangos_imposibles) {
    SimulationConfig config;
    config.test = kTestCaseCount + 1;
    CHECK_THROWS(config.validate(), ConfigError);

    config = SimulationConfig{};
    config.cell_count = 3;
    CHECK_THROWS(config.validate(), ConfigError);

    config = SimulationConfig{};
    config.gamma = 1.0;
    CHECK_THROWS(config.validate(), ConfigError);

    config = SimulationConfig{};
    config.cfl = 1.5;
    CHECK_THROWS(config.validate(), ConfigError);

    config = SimulationConfig{};
    config.cfl = 0.0;
    CHECK_THROWS(config.validate(), ConfigError);

    config = SimulationConfig{};
    config.x_max = config.x_min;
    CHECK_THROWS(config.validate(), ConfigError);

    config = SimulationConfig{};
    config.density_floor = 0.0;
    CHECK_THROWS(config.validate(), ConfigError);
}

TEST(la_configuracion_por_defecto_es_valida) {
    SimulationConfig config;
    config.validate();
    CHECK(config.test == 1);
}

// --------------------------------------------- resolucion de los tests

TEST(los_centinelas_negativos_los_fija_el_test) {
    SimulationConfig raw;
    raw.test = 1;
    CHECK(raw.diaphragm_x0 < 0.0);
    CHECK(raw.final_time < 0.0);
    const SimulationConfig resolved = resolveTestCase(raw);
    CHECK_NEAR(resolved.diaphragm_x0, 0.3, 0.0);
    CHECK_NEAR(resolved.final_time, 0.2, 0.0);
}

TEST(un_x0_explicito_del_usuario_se_respeta) {
    SimulationConfig raw;
    raw.test = 1;
    raw.diaphragm_x0 = 0.45;
    raw.final_time = 0.05;
    const SimulationConfig resolved = resolveTestCase(raw);
    CHECK_NEAR(resolved.diaphragm_x0, 0.45, 0.0);
    CHECK_NEAR(resolved.final_time, 0.05, 0.0);
}

TEST(el_test_6_impone_fronteras_periodicas) {
    SimulationConfig raw;
    raw.test = 6;
    const SimulationConfig resolved = resolveTestCase(raw);
    CHECK(resolved.boundary == BoundaryKind::Periodic);
    CHECK_NEAR(resolved.final_time, 1.0, 0.0);
}

TEST(el_test_7_impone_pared_y_gravedad_unidad) {
    SimulationConfig raw;
    raw.test = 7;
    const SimulationConfig resolved = resolveTestCase(raw);
    CHECK(resolved.boundary == BoundaryKind::Reflective);
    CHECK_NEAR(resolved.gravity, 1.0, 0.0);
    CHECK_NEAR(resolved.final_time, 10.0, 0.0);
}

TEST(el_test_7_respeta_una_gravedad_dada_por_el_usuario) {
    // El Fortran escribe `if (grav == 0) grav = 1`, luego un valor explicito
    // sobrevive.
    SimulationConfig raw;
    raw.test = 7;
    raw.gravity = 0.25;
    raw.gravity_was_set = true;
    CHECK_NEAR(resolveTestCase(raw).gravity, 0.25, 0.0);
}

TEST(el_test_8_impone_geometria_esferica_y_dominio) {
    SimulationConfig raw;
    raw.test = 8;
    const SimulationConfig resolved = resolveTestCase(raw);
    CHECK(resolved.geometry == GeometryKind::Spherical);
    CHECK(resolved.boundary == BoundaryKind::Reflective);
    CHECK_NEAR(resolved.x_max, 1.2, 0.0);
}

TEST(se_avisa_cuando_el_test_pisa_una_eleccion_explicita) {
    SimulationConfig raw;
    raw.test = 6;
    raw.boundary = BoundaryKind::Transmissive;
    raw.boundary_was_set = true;
    std::string report;
    const SimulationConfig resolved = resolveTestCase(raw, &report);
    CHECK(resolved.boundary == BoundaryKind::Periodic);
    // El Fortran lo hacia en silencio; aqui debe quedar constancia.
    CHECK(!report.empty());
}

TEST(sin_eleccion_explicita_no_se_avisa_de_nada) {
    SimulationConfig raw;
    raw.test = 6;
    std::string report;
    resolveTestCase(raw, &report);
    CHECK(report.empty());
}

TEST(un_test_fuera_de_rango_es_un_error) {
    // Expresado en funcion de kTestCaseCount para que el test siga siendo
    // correcto cuando se añadan casos nuevos.
    CHECK_THROWS(testCaseDefinition(0), ConfigError);
    CHECK_THROWS(testCaseDefinition(kTestCaseCount + 1), ConfigError);
    // Los extremos validos si deben existir.
    CHECK(!testCaseDefinition(1).name.empty());
    CHECK(!testCaseDefinition(kTestCaseCount).name.empty());
}

TEST(los_estados_iniciales_de_los_cinco_tests_de_toro) {
    // Tabla 4.1 de Toro, tal como los fija `set_test`.
    CHECK_NEAR(testCaseDefinition(1).left.u, 0.75, 0.0);
    CHECK_NEAR(testCaseDefinition(1).right.rho, 0.125, 0.0);
    CHECK_NEAR(testCaseDefinition(2).left.u, -2.0, 0.0);
    CHECK_NEAR(testCaseDefinition(3).left.p, 1000.0, 0.0);
    CHECK_NEAR(testCaseDefinition(4).left.rho, 5.99924, 0.0);
    CHECK_NEAR(testCaseDefinition(4).right.u, -6.19633, 0.0);
    CHECK_NEAR(testCaseDefinition(5).left.u, -19.59745, 0.0);
}
