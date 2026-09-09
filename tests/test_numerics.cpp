#include "test_support.hpp"

#include <vector>

#include "euler1d/boundary/boundary_condition.hpp"
#include "euler1d/numerical/reconstruction/reconstruction.hpp"
#include "euler1d/numerical/reconstruction/slope_limiter.hpp"
#include "euler1d/numerical/riemann/hll.hpp"
#include "euler1d/numerical/riemann/hllc.hpp"

using namespace euler1d;

namespace {
IdealGas makeGas() { return IdealGas(1.4, 1.0e-12, 1.0e-12); }
}  // namespace

// ---------------------------------------------------------- limitadores

TEST(primer_orden_anula_la_pendiente) {
    CHECK_NEAR(limitedSlope(LimiterKind::FirstOrder, 3.0, 4.0), 0.0, 0.0);
}

TEST(todos_los_limitadores_se_anulan_en_un_extremo) {
    // Signos opuestos = extremo local: la pendiente debe anularse para no
    // crear un nuevo extremo (condicion TVD).
    for (const LimiterKind kind :
         {LimiterKind::MinMod, LimiterKind::VanLeer, LimiterKind::MonotonizedCentral,
          LimiterKind::Superbee}) {
        CHECK_NEAR(limitedSlope(kind, 1.0, -1.0), 0.0, 0.0);
        CHECK_NEAR(limitedSlope(kind, -2.0, 3.0), 0.0, 0.0);
    }
}

TEST(minmod_toma_la_pendiente_de_menor_modulo) {
    CHECK_NEAR(limitedSlope(LimiterKind::MinMod, 1.0, 3.0), 1.0, 1.0e-15);
    CHECK_NEAR(limitedSlope(LimiterKind::MinMod, -4.0, -2.0), -2.0, 1.0e-15);
}

TEST(van_leer_es_la_media_armonica) {
    // 2ab/(a+b) para a,b > 0
    CHECK_NEAR(limitedSlope(LimiterKind::VanLeer, 1.0, 3.0), 2.0 * 1.0 * 3.0 / 4.0, 1.0e-15);
}

TEST(monotonized_central_recorta_la_pendiente_central) {
    // a = 1, b = 3: central = 2, cotas 2a = 2 y 2b = 6 -> 2
    CHECK_NEAR(limitedSlope(LimiterKind::MonotonizedCentral, 1.0, 3.0), 2.0, 1.0e-15);
    // a = 1, b = 10: central = 5.5, cota 2a = 2 manda
    CHECK_NEAR(limitedSlope(LimiterKind::MonotonizedCentral, 1.0, 10.0), 2.0, 1.0e-15);
}

TEST(superbee_es_el_mas_compresivo) {
    // En una rampa monotona superbee >= mc >= vl >= mm (mismo signo).
    const double a = 1.0;
    const double b = 3.0;
    const double mm = limitedSlope(LimiterKind::MinMod, a, b);
    const double vl = limitedSlope(LimiterKind::VanLeer, a, b);
    const double mc = limitedSlope(LimiterKind::MonotonizedCentral, a, b);
    const double sb = limitedSlope(LimiterKind::Superbee, a, b);
    CHECK(mm <= vl);
    CHECK(vl <= mc);
    CHECK(mc <= sb);
}

TEST(los_limitadores_reproducen_el_signo_de_fortran_en_cero) {
    // `sign(1.0d0, 0.0d0)` vale +1 en Fortran. Con a = 0 y b > 0, minmod da
    // 0.5*(1+1)*min(0,b) = 0; el detalle importa cuando b tambien es 0.
    CHECK_NEAR(limitedSlope(LimiterKind::MinMod, 0.0, 0.0), 0.0, 0.0);
    CHECK_NEAR(limiters::fortranSign(0.0), 1.0, 0.0);
    CHECK_NEAR(limiters::fortranSign(-0.0), 1.0, 0.0);
}

// -------------------------------------------------------- reconstruccion

TEST(un_campo_uniforme_se_reconstruye_sin_salto) {
    const MusclReconstruction muscl(LimiterKind::VanLeer);
    PrimitiveField w(8);
    for (int k = -1; k <= 10; ++k) w[k] = {2.0, 0.3, 1.5};

    std::vector<PrimitiveState> left(9);
    std::vector<PrimitiveState> right(9);
    muscl.computeInterfaceStates(w, HydrostaticContext{}, left, right);

    for (std::size_t i = 0; i < left.size(); ++i) {
        CHECK_NEAR(left[i].rho, 2.0, 1.0e-15);
        CHECK_NEAR(right[i].rho, 2.0, 1.0e-15);
        CHECK_NEAR(left[i].p, 1.5, 1.0e-15);
        CHECK_NEAR(right[i].p, 1.5, 1.0e-15);
    }
}

TEST(una_rampa_lineal_se_reconstruye_exactamente) {
    // MUSCL con van Leer es exacto de segundo orden en un perfil lineal: el
    // estado de cara debe caer en el valor analitico del borde de celda.
    const MusclReconstruction muscl(LimiterKind::VanLeer);
    const int n = 8;
    PrimitiveField w(n);
    for (int k = -1; k <= n + 2; ++k) {
        w[k] = {1.0 + 0.1 * k, 0.0, 1.0};
    }
    std::vector<PrimitiveState> left(static_cast<std::size_t>(n) + 1);
    std::vector<PrimitiveState> right(static_cast<std::size_t>(n) + 1);
    muscl.computeInterfaceStates(w, HydrostaticContext{}, left, right);

    for (int k = 1; k < n; ++k) {
        const double face_value = 1.0 + 0.1 * (k + 0.5);
        CHECK_NEAR(left[static_cast<std::size_t>(k)].rho, face_value, 1.0e-14);
        CHECK_NEAR(right[static_cast<std::size_t>(k)].rho, face_value, 1.0e-14);
    }
}

TEST(primer_orden_da_estados_de_cara_constantes_por_celda) {
    const MusclReconstruction muscl(LimiterKind::FirstOrder);
    const int n = 6;
    PrimitiveField w(n);
    for (int k = -1; k <= n + 2; ++k) w[k] = {1.0 + 0.1 * k, 0.0, 1.0};

    std::vector<PrimitiveState> left(static_cast<std::size_t>(n) + 1);
    std::vector<PrimitiveState> right(static_cast<std::size_t>(n) + 1);
    muscl.computeInterfaceStates(w, HydrostaticContext{}, left, right);

    for (int k = 0; k <= n; ++k) {
        CHECK_NEAR(left[static_cast<std::size_t>(k)].rho, w[k].rho, 0.0);
        CHECK_NEAR(right[static_cast<std::size_t>(k)].rho, w[k + 1].rho, 0.0);
    }
}

TEST(la_reconstruccion_hidrostatica_anula_el_salto_en_equilibrio) {
    // Atmosfera en equilibrio DISCRETO trapezoidal: las desviaciones son cero,
    // asi que los dos estados de cara deben coincidir y el flujo cancelar la
    // fuente -rho g exactamente.
    const int n = 16;
    const double gravity = 1.0;
    const double dx = 1.0 / n;
    PrimitiveField w(n);
    for (int k = -1; k <= n + 2; ++k) {
        w[k].rho = std::exp(-gravity * (0.5 * dx + (k - 1) * dx));
        w[k].u = 0.0;
    }
    w[-1].p = w[-1].rho;
    for (int k = 0; k <= n + 2; ++k) {
        w[k].p = w[k - 1].p - gravity * dx * 0.5 * (w[k - 1].rho + w[k].rho);
    }

    const MusclReconstruction muscl(LimiterKind::VanLeer);
    std::vector<PrimitiveState> left(static_cast<std::size_t>(n) + 1);
    std::vector<PrimitiveState> right(static_cast<std::size_t>(n) + 1);
    muscl.computeInterfaceStates(w, HydrostaticContext{gravity, dx}, left, right);

    for (int k = 1; k < n; ++k) {
        const auto i = static_cast<std::size_t>(k);
        CHECK_NEAR(left[i].p, right[i].p, 1.0e-15);
    }
}

// ------------------------------------------------------ solvers de Riemann

TEST(hll_y_hllc_son_consistentes_en_un_estado_uniforme) {
    // Consistencia: con W_L = W_R el flujo numerico debe ser F(W).
    const IdealGas gas = makeGas();
    const PrimitiveState w{1.2, 0.4, 2.0};
    const FluxVector exact = gas.physicalFlux(w);
    for (const FluxVector f : {hllFlux(gas, w, w), hllcFlux(gas, w, w)}) {
        CHECK_NEAR(f.mass, exact.mass, 1.0e-13);
        CHECK_NEAR(f.momentum, exact.momentum, 1.0e-13);
        CHECK_NEAR(f.energy, exact.energy, 1.0e-13);
    }
}

TEST(flujo_supersonico_hacia_la_derecha_es_el_flujo_izquierdo) {
    // Si S_L >= 0 toda la informacion viene de la izquierda (upwind puro).
    const IdealGas gas = makeGas();
    const PrimitiveState left{1.0, 10.0, 1.0};
    const PrimitiveState right{1.0, 10.0, 1.0};
    const FluxVector expected = gas.physicalFlux(left);
    const FluxVector hll = hllFlux(gas, left, right);
    CHECK_NEAR(hll.mass, expected.mass, 1.0e-14);
    const FluxVector hllc = hllcFlux(gas, left, right);
    CHECK_NEAR(hllc.mass, expected.mass, 1.0e-14);
}

TEST(flujo_supersonico_hacia_la_izquierda_es_el_flujo_derecho) {
    const IdealGas gas = makeGas();
    const PrimitiveState left{1.0, -10.0, 1.0};
    const PrimitiveState right{1.0, -10.0, 1.0};
    const FluxVector expected = gas.physicalFlux(right);
    CHECK_NEAR(hllFlux(gas, left, right).mass, expected.mass, 1.0e-14);
    CHECK_NEAR(hllcFlux(gas, left, right).mass, expected.mass, 1.0e-14);
}

TEST(hllc_resuelve_el_contacto_estacionario_que_hll_difumina) {
    // Salto de densidad con u = 0 y p uniforme: el contacto no se mueve, asi
    // que el flujo de masa exacto es cero. HLLC lo captura; HLL, que promedia
    // sobre las dos ondas acusticas, no.
    const IdealGas gas = makeGas();
    const PrimitiveState left{1.0, 0.0, 1.0};
    const PrimitiveState right{0.125, 0.0, 1.0};

    const FluxVector hllc = hllcFlux(gas, left, right);
    CHECK_NEAR(hllc.mass, 0.0, 1.0e-14);
    CHECK_NEAR(hllc.momentum, 1.0, 1.0e-13);

    const FluxVector hll = hllFlux(gas, left, right);
    CHECK(std::abs(hll.mass) > 1.0e-3);
}

TEST(el_barrido_por_lotes_coincide_con_la_cara_individual) {
    // Garantiza que la ruta rapida (una llamada virtual por barrido) y la
    // legible (una cara) calculan lo mismo.
    const IdealGas gas = makeGas();
    const HllcRiemannSolver solver(gas);
    std::vector<PrimitiveState> left = {{1.0, 0.0, 1.0}, {0.5, 0.2, 0.8}, {2.0, -0.3, 3.0}};
    std::vector<PrimitiveState> right = {{0.125, 0.0, 0.1}, {0.4, 0.1, 0.7}, {1.0, 0.5, 1.0}};
    std::vector<FluxVector> batch(left.size());
    solver.computeFluxes(left, right, batch);
    for (std::size_t i = 0; i < left.size(); ++i) {
        const FluxVector single = solver.flux(left[i], right[i]);
        CHECK_NEAR(batch[i].mass, single.mass, 0.0);
        CHECK_NEAR(batch[i].momentum, single.momentum, 0.0);
        CHECK_NEAR(batch[i].energy, single.energy, 0.0);
    }
}

TEST(las_velocidades_de_davis_acotan_las_caracteristicas) {
    const IdealGas gas = makeGas();
    const PrimitiveState left{1.0, 0.75, 1.0};
    const PrimitiveState right{0.125, 0.0, 0.1};
    const WaveSpeeds s = davisWaveSpeeds(gas, left, right);
    CHECK(s.left <= left.u - gas.soundSpeed(left));
    CHECK(s.right >= right.u + gas.soundSpeed(right));
    CHECK(s.left < s.right);
}

// ----------------------------------------------------------- fronteras

TEST(la_frontera_transmisiva_copia_la_celda_de_borde) {
    const IdealGas gas = makeGas();
    const int n = 5;
    ConservedField u(n);
    PrimitiveField w(n);
    for (int k = 1; k <= n; ++k) {
        w[k] = {1.0 + k, 0.1 * k, 2.0 + k};
        u[k] = gas.toConserved(w[k]);
    }
    TransmissiveBoundary().apply(u, w);
    // Los DOS fantasmas copian la misma celda: extrapolacion constante.
    CHECK_NEAR(w[0].rho, w[1].rho, 0.0);
    CHECK_NEAR(w[-1].rho, w[1].rho, 0.0);
    CHECK_NEAR(w[n + 1].rho, w[n].rho, 0.0);
    CHECK_NEAR(w[n + 2].rho, w[n].rho, 0.0);
}

TEST(la_frontera_periodica_envuelve_el_dominio) {
    const IdealGas gas = makeGas();
    const int n = 5;
    ConservedField u(n);
    PrimitiveField w(n);
    for (int k = 1; k <= n; ++k) {
        w[k] = {1.0 + k, 0.1 * k, 2.0 + k};
        u[k] = gas.toConserved(w[k]);
    }
    PeriodicBoundary().apply(u, w);
    CHECK_NEAR(w[0].rho, w[n].rho, 0.0);
    CHECK_NEAR(w[-1].rho, w[n - 1].rho, 0.0);
    CHECK_NEAR(w[n + 1].rho, w[1].rho, 0.0);
    CHECK_NEAR(w[n + 2].rho, w[2].rho, 0.0);
}

TEST(la_pared_refleja_la_velocidad_y_espeja_la_densidad) {
    const IdealGas gas = makeGas();
    const int n = 5;
    ConservedField u(n);
    PrimitiveField w(n);
    for (int k = 1; k <= n; ++k) {
        w[k] = {1.0 + k, 0.2 * k, 2.0 + k};
        u[k] = gas.toConserved(w[k]);
    }
    ReflectiveBoundary(gas, 0.0, 0.1).apply(u, w);
    CHECK_NEAR(w[0].rho, w[1].rho, 0.0);
    CHECK_NEAR(w[0].u, -w[1].u, 0.0);
    CHECK_NEAR(w[-1].rho, w[2].rho, 0.0);
    CHECK_NEAR(w[-1].u, -w[2].u, 0.0);
    CHECK_NEAR(w[n + 1].u, -w[n].u, 0.0);
    CHECK_NEAR(w[n + 2].u, -w[n - 1].u, 0.0);
    // Sin gravedad la presion se espeja sin desplazamiento.
    CHECK_NEAR(w[0].p, w[1].p, 0.0);
    CHECK_NEAR(w[-1].p, w[2].p, 0.0);
    // Las conservadas de los fantasmas quedan sincronizadas con las primitivas.
    CHECK_NEAR(u[0].momentum, w[0].rho * w[0].u, 1.0e-15);
}

TEST(la_pared_con_gravedad_continua_el_equilibrio_hidrostatico) {
    const IdealGas gas = makeGas();
    const int n = 5;
    const double gravity = 1.0;
    const double dx = 0.1;
    ConservedField u(n);
    PrimitiveField w(n);
    for (int k = 1; k <= n; ++k) {
        w[k] = {1.0, 0.0, 1.0};
        u[k] = gas.toConserved(w[k]);
    }
    ReflectiveBoundary(gas, gravity, dx).apply(u, w);
    // p_ghost = p_int + g dx (rho_ghost + rho_int)/2, NO p_int.
    CHECK_NEAR(w[0].p, 1.0 + gravity * dx * 1.0, 1.0e-15);
    CHECK_NEAR(w[n + 1].p, 1.0 - gravity * dx * 1.0, 1.0e-15);
}
