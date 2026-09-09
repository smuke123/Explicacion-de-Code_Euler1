#include "test_support.hpp"

#include "euler1d/io/fortran_format.hpp"
#include "euler1d/mesh/mesh1d.hpp"
#include "euler1d/numerical/cfl_controller.hpp"
#include "euler1d/physics/field.hpp"
#include "euler1d/physics/ideal_gas.hpp"

using namespace euler1d;

namespace {
IdealGas makeGas() { return IdealGas(1.4, 1.0e-12, 1.0e-12); }
}  // namespace

// --------------------------------------------------------------- EOS

TEST(conversion_primitiva_conservada_es_involutiva) {
    const IdealGas gas = makeGas();
    const PrimitiveState w{1.3, -0.7, 2.5};
    const ConservedState u = gas.toConserved(w);
    const PrimitiveState back = gas.toPrimitive(u);
    // Ida y vuelta exacta salvo redondeo de la resta de la energia cinetica.
    CHECK_NEAR(back.rho, w.rho, 1.0e-15);
    CHECK_NEAR(back.u, w.u, 1.0e-15);
    CHECK_NEAR(back.p, w.p, 1.0e-14);
}

TEST(energia_total_sigue_la_ecuacion_de_estado) {
    const IdealGas gas = makeGas();
    const PrimitiveState w{2.0, 3.0, 5.0};
    // E = p/(gamma-1) + rho u^2/2 = 5/0.4 + 2*9/2 = 12.5 + 9 = 21.5
    CHECK_NEAR(gas.totalEnergy(w), 21.5, 1.0e-13);
}

TEST(velocidad_del_sonido) {
    const IdealGas gas = makeGas();
    // c = sqrt(gamma p / rho) = sqrt(1.4 * 0.4 / 1.0)
    CHECK_NEAR(gas.soundSpeed({1.0, 0.0, 0.4}), std::sqrt(0.56), 1.0e-15);
    // Estado de referencia de aire: rho = 1, p = 1 -> c = sqrt(1.4)
    CHECK_NEAR(gas.soundSpeed({1.0, 0.0, 1.0}), std::sqrt(1.4), 1.0e-15);
}

TEST(flujo_fisico_tiene_las_tres_componentes_correctas) {
    const IdealGas gas = makeGas();
    const PrimitiveState w{2.0, 3.0, 5.0};
    const FluxVector f = gas.physicalFlux(w);
    const double energy = gas.totalEnergy(w);
    CHECK_NEAR(f.mass, 6.0, 1.0e-13);                    // rho u
    CHECK_NEAR(f.momentum, 2.0 * 9.0 + 5.0, 1.0e-13);    // rho u^2 + p
    CHECK_NEAR(f.energy, 3.0 * (energy + 5.0), 1.0e-13); // u (E + p)
}

TEST(los_pisos_se_aplican_en_el_orden_del_fortran) {
    const IdealGas gas = makeGas();
    FloorCounters counters;
    // rho por debajo del piso: se pisa ANTES de dividir, asi que la velocidad
    // resultante usa la densidad ya corregida y no explota.
    const PrimitiveState w = gas.toPrimitive({1.0e-20, 1.0e-20, -5.0}, &counters);
    CHECK_NEAR(w.rho, 1.0e-12, 0.0);
    CHECK(std::isfinite(w.u));
    CHECK_NEAR(w.p, 1.0e-12, 0.0);
    CHECK(counters.density == 1);
    CHECK(counters.pressure == 1);
    CHECK(counters.triggered());
}

TEST(sin_estados_invalidos_los_contadores_quedan_a_cero) {
    const IdealGas gas = makeGas();
    FloorCounters counters;
    gas.toPrimitive(gas.toConserved({1.0, 0.5, 1.0}), &counters);
    CHECK(!counters.triggered());
}

// -------------------------------------------------------------- malla

TEST(centros_y_caras_de_la_malla) {
    const Mesh1D mesh(0.0, 1.0, 4);
    CHECK_NEAR(mesh.dx(), 0.25, 1.0e-15);
    // x_k = xmin + (k - 1/2) dx con k = 1..N
    CHECK_NEAR(mesh.center(1), 0.125, 1.0e-15);
    CHECK_NEAR(mesh.center(4), 0.875, 1.0e-15);
    // Caras k+1/2 con k = 0..N
    CHECK_NEAR(mesh.face(0), 0.0, 1.0e-15);
    CHECK_NEAR(mesh.face(4), 1.0, 1.0e-15);
}

TEST(el_campo_admite_los_indices_fantasma_del_fortran) {
    ConservedField field(10);
    field[-1].rho = 1.0;
    field[0].rho = 2.0;
    field[10].rho = 3.0;
    field[11].rho = 4.0;
    field[12].rho = 5.0;
    CHECK_NEAR(field[-1].rho, 1.0, 0.0);
    CHECK_NEAR(field[12].rho, 5.0, 0.0);
    // N celdas fisicas + 2 fantasmas por lado, contiguos.
    CHECK(field.rawSize() == 14);
}

// ---------------------------------------------------------------- CFL

TEST(el_paso_de_tiempo_sigue_la_formula_cfl) {
    const Mesh1D mesh(0.0, 1.0, 100);
    const IdealGas gas = makeGas();
    const CflController cfl(0.9, mesh, gas);

    PrimitiveField w(100);
    for (int k = 1; k <= 100; ++k) w[k] = {1.0, 0.0, 1.0};

    const double c = std::sqrt(1.4);
    CHECK_NEAR(cfl.maxSignalSpeed(w), c, 1.0e-15);
    CHECK_NEAR(cfl.computeTimeStep(w), 0.9 * mesh.dx() / c, 1.0e-16);
}

TEST(la_velocidad_de_senal_toma_el_maximo_sobre_la_malla) {
    const Mesh1D mesh(0.0, 1.0, 10);
    const IdealGas gas = makeGas();
    const CflController cfl(0.5, mesh, gas);

    PrimitiveField w(10);
    for (int k = 1; k <= 10; ++k) w[k] = {1.0, 0.0, 1.0};
    w[7] = {1.0, -20.0, 1.0};  // |u| + c domina, y el signo no debe importar
    CHECK_NEAR(cfl.maxSignalSpeed(w), 20.0 + std::sqrt(1.4), 1.0e-14);
}

TEST(un_campo_en_reposo_y_frio_no_divide_por_cero) {
    const Mesh1D mesh(0.0, 1.0, 10);
    const IdealGas gas = makeGas();
    const CflController cfl(0.9, mesh, gas);
    PrimitiveField w(10);
    // La semilla 1e-30 del maximo evita el 0/0; el dt sale enorme pero finito.
    for (int k = 1; k <= 10; ++k) w[k] = {1.0, 0.0, 0.0};
    CHECK(std::isfinite(cfl.computeTimeStep(w)));
}

TEST(un_campo_con_nan_es_un_error_no_algo_que_pisar) {
    const Mesh1D mesh(0.0, 1.0, 10);
    const IdealGas gas = makeGas();
    const CflController cfl(0.9, mesh, gas);
    PrimitiveField w(10);
    for (int k = 1; k <= 10; ++k) w[k] = {1.0, 0.0, 1.0};
    w[3] = {1.0, std::nan(""), 1.0};
    CHECK_THROWS(cfl.computeTimeStep(w), std::runtime_error);
}

// ------------------------------------------------- formato de salida

TEST(el_formato_es_reproduce_el_descriptor_de_fortran) {
    // es20.10e3: ancho 20, 10 decimales, exponente de 3 digitos.
    CHECK(formatEs(0.5, 20, 10, 3) == "   5.0000000000E-001");
    CHECK(formatEs(-0.5, 20, 10, 3) == "  -5.0000000000E-001");
    CHECK(formatEs(0.0, 20, 10, 3) == "   0.0000000000E+000");
    // es20.10: exponente de 2 digitos (minimo de Fortran).
    CHECK(formatEs(0.2, 20, 10, 0) == "    2.0000000000E-01");
}

TEST(la_fila_de_datos_son_dos_campos_de_ancho_20) {
    const std::string row = formatDataRow(0.125, 1.0);
    CHECK(row.size() == 40);
    CHECK(row.substr(0, 20) == "   1.2500000000E-001");
}

TEST(la_cabecera_de_tiempo_lleva_el_prefijo_del_fortran) {
    CHECK(formatTimeHeader(0.2).substr(0, 9) == " #Time = ");
}
