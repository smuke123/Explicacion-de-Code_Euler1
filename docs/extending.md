# Cómo añadir tus propias pruebas y métodos

Cinco recetas, de la más simple a la que más toca el código. Todas están
verificadas contra el estado actual del repositorio.

| Quiero… | Sección | Toca el solver |
|---|---|---|
| Probar una configuración distinta | [1](#1-una-corrida-propia-sin-tocar-código) | No |
| Comprobar una propiedad matemática | [2](#2-un-test-unitario) | No |
| Comprobar una simulación completa | [3](#3-un-test-de-integración) | No |
| Congelar un resultado como referencia | [4](#4-un-caso-de-regresión-propio) | No |
| Añadir un caso físico nuevo (`test = 9`) | [5](#5-un-caso-físico-propio-test--9) | Sí |
| Añadir un método numérico (HLLE, RK3…) | [6](#6-un-método-numérico-nuevo) | Sí |

---

## 1. Una corrida propia, sin tocar código

Todo lo configurable está en el namelist. Copia un ejemplo y edítalo:

```bash
cp examples/toro1_hllc_vl.par mis_pruebas/mi_caso.par
./build/xEuler1D.exe -i mis_pruebas/mi_caso.par -o out/mi_caso
```

Puedes cambiar el test, la resolución, el solver, el limitador, la frontera, la
gravedad, la geometría, el dominio, la posición del diafragma y el tiempo final
sin recompilar. La lista completa de claves está en el
[README](../README.md#configuración).

Un ejemplo útil: barrer resoluciones para medir el orden de convergencia.

```bash
for n in 50 100 200 400 800; do
  printf "&euler\n test = 6\n N = %d\n limiter = 'vl'\n solver = 'hllc'\n/\n" $n \
    > /tmp/conv_$n.par
  ./build/xEuler1D.exe -q -i /tmp/conv_$n.par -o out/conv_$n
done
```

Después compara cada `out/conv_N/rho_1.xl` con su `rho_exact.xl`. El propio
`euler1d_compare` te da las normas $L_1$ y $L_2$:

```bash
./build/euler1d_compare.exe out/conv_100 out/conv_100
```

(Para comparar numérica contra exacta lo más directo es el script de Python del
punto 9 de la [guía de MSYS2](msys2_guide.md), porque `euler1d_compare` compara
archivos del mismo nombre en dos directorios.)

---

## 2. Un test unitario

Los tests están en `tests/`. Para comprobar una propiedad de un componente,
añade un `TEST(...)` al archivo que le corresponda:

| Archivo | Qué contiene |
|---|---|
| `test_physics.cpp` | EOS, malla, `CellField`, CFL, formato de salida |
| `test_numerics.cpp` | Limitadores, reconstrucción, HLL/HLLC, fronteras |
| `test_exact_riemann.cpp` | Solver exacto contra Toro |
| `test_config.cpp` | Parser, validación, resolución de casos |
| `test_integration.cpp` | Simulaciones completas |

Se registra solo, no hay que declararlo en ningún sitio:

```cpp
TEST(el_flujo_hllc_conserva_la_masa_en_un_contacto) {
    const IdealGas gas(1.4, 1.0e-12, 1.0e-12);
    const FluxVector f = hllcFlux(gas, {1.0, 0.0, 1.0}, {0.125, 0.0, 1.0});
    CHECK_NEAR(f.mass, 0.0, 1.0e-14);
}
```

```bash
cmake --build build && ./build/tests/test_numerics.exe
```

### Las macros disponibles

| Macro | Cuándo usarla |
|---|---|
| `CHECK(cond)` | Condición booleana |
| `CHECK_NEAR(a, b, tol)` | Diferencia **absoluta** ≤ `tol`. Para magnitudes de orden conocido |
| `CHECK_REL(v, ref, tol)` | Diferencia **relativa** a `\|ref\|`. Para magnitudes muy grandes o muy pequeñas |
| `CHECK_THROWS(expr, Tipo)` | Debe lanzar esa excepción |

`CHECK_NEAR(x, y, 0.0)` exige igualdad exacta, y es lo correcto cuando la
operación solo copia valores (fronteras) o devuelve una constante (pisos).

### Cómo elegir la tolerancia

No pongas `1e-6` «por si acaso»: una tolerancia holgada convierte el test en
decorativo. Piensa de dónde sale el error:

| Origen del error | Orden razonable |
|---|---|
| Una o dos operaciones aritméticas | `1e-15` (o exacto) |
| Cancelación catastrófica (p. ej. $E - \rho u^2/2$) | `1e-14` |
| Cadena de ~10 operaciones sobre magnitudes O(10–1000) | `1e-13` |
| Comparación con un valor publicado de 6 cifras | `1e-5` **relativo** |
| Suma de $N$ términos (masa, energía total) | `1e-12` relativo |
| Resultado de una simulación completa | depende; mídelo antes de fijarlo |

Para el último caso, el método honesto es: escribe el test con una tolerancia
absurdamente estricta, míralo fallar, lee el valor real que imprime el arnés y
**entonces** fija el umbral con margen razonado. Así el número queda justificado
en lugar de adivinado.

### Un archivo de test nuevo

```bash
# tests/test_mis_pruebas.cpp
```

```cpp
#include "test_support.hpp"

#include "euler1d/physics/ideal_gas.hpp"

using namespace euler1d;

TEST(mi_primera_comprobacion) {
    CHECK_NEAR(IdealGas(1.4, 1e-12, 1e-12).soundSpeed({1.0, 0.0, 1.0}),
               std::sqrt(1.4), 1.0e-15);
}
```

Y añade el nombre **sin `.cpp`** a la lista de `tests/CMakeLists.txt`:

```cmake
set(EULER1D_TESTS
    test_physics
    test_numerics
    test_exact_riemann
    test_config
    test_integration
    test_regression
    test_mis_pruebas)      # <-- nuevo
```

Luego hay que reconfigurar, porque cambió `CMakeLists.txt`:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build -R test_mis_pruebas --output-on-failure
```

El `main()` lo pone `test_support.hpp`; no lo escribas tú.

---

## 3. Un test de integración

Si lo que quieres comprobar necesita una simulación completa, usa
`test_integration.cpp`, que ya tiene las dos piezas de apoyo:
`ScratchDirectory` (directorio temporal que se borra solo) y `makeConfig`.

```cpp
TEST(mi_caso_conserva_la_masa_con_fronteras_periodicas) {
    ScratchDirectory scratch("mi_caso");
    Simulation simulation(makeConfig(6, 128, LimiterKind::Superbee,
                                     RiemannSolverKind::HLL),
                          scratch.path());
    const double inicial = totalMass(simulation);
    const SimulationReport report = simulation.run(false);
    CHECK_REL(totalMass(simulation), inicial, 1.0e-12);
    CHECK(report.steps > 0);
    CHECK(!report.floors.triggered());
}
```

`run(false)` silencia la salida por pantalla. `SimulationReport` te da `steps`,
`final_time`, `wall_seconds`, `floors`, y —según el caso— `exact_star` y
`hydrostatic_residual`.

Para medir error contra la solución de referencia usa
`simulation.referenceProfile(t)`, que devuelve el perfil analítico muestreado en
los centros de celda, y `compareFields` de `error_analyzer.hpp`. La función
`densityErrorL1` del archivo ya hace exactamente eso y puedes copiarla.

> **Un test de integración que solo comprueba «no explota» no comprueba casi
> nada.** Busca una propiedad que un error real rompería: una cantidad
> conservada, una simetría, un orden de convergencia, o una comparación entre
> dos configuraciones que deban ordenarse de una forma concreta (como
> `hllc_resuelve_el_contacto_mejor_que_hll_en_toro1`).

---

## 4. Un caso de regresión propio

Sirve para **congelar** un resultado que hoy consideras correcto y detectar si
un cambio futuro lo altera. `test_regression` recorre automáticamente todos los
subdirectorios de `tests/reference/`, así que no hay que escribir código.

```bash
mkdir -p tests/reference/mi_caso
cp mis_pruebas/mi_caso.par tests/reference/mi_caso/input.par
./build/xEuler1D.exe -i tests/reference/mi_caso/input.par \
                     -o tests/reference/mi_caso > tests/reference/mi_caso/log.txt
rm tests/reference/mi_caso/run_info.txt
```

Con eso, `ctest -R test_regression` volverá a correr esa configuración y
comparará los `.xl` con criterio $10^{-12} + 10^{-9}\max|\text{ref}|$, y además
exigirá que el **número de pasos** siga siendo el mismo (lo lee de `log.txt`).

Dos advertencias importantes:

- **Si generas la referencia con el propio C++, estás congelando el
  comportamiento actual, no validándolo.** Sirve para detectar regresiones, no
  para demostrar que es correcto. Las referencias que sí validan son las que
  produjo el Fortran (las 13 que ya están) o una solución analítica.
- **`run_info.txt` incluye `wall_seconds`**, que cambia en cada ejecución. No lo
  dejes en el directorio de referencia; el test no lo mira, pero ensucia el
  historial de git en cada regeneración.

Para generar la referencia con el **Fortran** en lugar del C++, y que entonces sí
valide:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cd $env:TEMP; mkdir fort; cd fort
copy C:\Users\USER\OneDrive\Documentos\Tesis\Euler1D_Cpp\reference_fortran\*.f90 .
gfortran -O2 -c global_numbers.f90 exact_riemann.f90 euler1d.f90
gfortran -O2 -o xF.exe global_numbers.o exact_riemann.o euler1d.o
copy <tu input.par> input.par
.\xF.exe > log.txt
# copia los .xl y el log.txt a tests/reference/mi_caso/
```

---

## 5. Un caso físico propio (`test = 9`)

Aquí sí hay que editar el solver. Son **dos** cambios obligatorios, y el
compilador te avisa si haces solo uno.

### Paso 1 — subir el contador

`include/euler1d/config/simulation_config.hpp`:

```cpp
inline constexpr int kTestCaseCount = 9;   // era 8
```

Esta constante es la única declaración del límite: la tabla de casos se
dimensiona con ella, así que si la subes y no añades la entrada, la
inicialización queda incompleta; y si añades la entrada sin subirla, hay
demasiados inicializadores. En ambos casos es un error de compilación, no un
fallo en tiempo de ejecución.

### Paso 2 — añadir la entrada a la tabla

`src/initial/test_case.cpp`, al final del `array` de `table()`. Los campos van
en el orden en que están declarados en `TestCaseDefinition`:

```cpp
{"mi_caso", "Descripcion breve de que problema es",
 /* left            */ {1.0, 0.0, 1.0},
 /* right           */ {0.2, 0.0, 0.2},
 /* default_x0      */ 0.5,
 /* default_tf      */ 0.25,
 /* forced_boundary */ std::nullopt,
 /* forced_geometry */ std::nullopt,
 /* default_gravity */ std::nullopt,
 /* forced_x_max    */ std::nullopt,
 /* uses_riemann_ic */ true,
 /* has_exact_ref   */ true},
```

Si tu caso **es** un problema de Riemann (dos estados y un diafragma), esto es
todo: la condición inicial y la solución exacta de referencia funcionan sin más.

```bash
cmake --build build
printf "&euler\n test = 9\n N = 400\n/\n" > /tmp/t9.par
./build/xEuler1D.exe -i /tmp/t9.par -o out/t9
```

### Paso 3 (solo si NO es un problema de Riemann)

Si tu condición inicial es un perfil (una sinusoide, una atmósfera, una
explosión…), pon `uses_riemann_initial_data = false` y añade la rama en
`applyInitialCondition`, en el mismo archivo:

```cpp
case 9:
    // rho gaussiana, en reposo, presion uniforme
    w[k] = {1.0 + 0.5 * std::exp(-100.0 * (x - 0.5) * (x - 0.5)), 0.0, 1.0};
    break;
```

Y si además no tiene solución exacta conocida, pon
`has_exact_riemann_reference = false`: entonces `referenceProfile` devuelve
`std::nullopt` y no se escriben los `*_exact.xl`. **Es preferible no escribirlos
a escribirlos sin significado**, que es justo el problema que tienen los tests 7
y 8 heredados del Fortran (ver [inconsistencies.md](inconsistencies.md), punto
4).

### Paso 4 — imponer opciones desde el caso

Si tu problema requiere una frontera o una geometría concretas, no confíes en
que el usuario las ponga: impónlas con `forced_boundary` / `forced_geometry` /
`forced_x_max`. `resolveTestCase` las aplica y **avisa por pantalla** si pisan
algo que el usuario había elegido explícitamente.

```cpp
/* forced_boundary */ BoundaryKind::Reflective,
/* forced_geometry */ GeometryKind::Cylindrical,
```

### Paso 5 — el test

```cpp
// en tests/test_config.cpp
TEST(el_test_9_define_sus_estados_y_sus_valores_por_defecto) {
    const TestCaseDefinition& def = testCaseDefinition(9);
    CHECK(def.name == "mi_caso");
    CHECK_NEAR(def.left.rho, 1.0, 0.0);

    SimulationConfig raw;
    raw.test = 9;
    const SimulationConfig resolved = resolveTestCase(raw);
    CHECK_NEAR(resolved.diaphragm_x0, 0.5, 0.0);
    CHECK_NEAR(resolved.final_time, 0.25, 0.0);
}
```

Los tests que comprueban los límites del rango (`un_test_fuera_de_rango_es_un_error`,
`validate_rechaza_rangos_imposibles`) ya están escritos en función de
`kTestCaseCount`, así que no hay que tocarlos.

---

## 6. Un método numérico nuevo

La arquitectura está pensada para esto: cada punto de variación es una interfaz,
y el núcleo de volúmenes finitos no se toca. Ver
[architecture.md](architecture.md).

### Un solver de Riemann (HLLE, Roe, Rusanov…)

1. Crea `include/euler1d/numerical/riemann/hlle.hpp` copiando la estructura de
   `hll.hpp`: una función libre `hlleFlux(gas, qL, qR)` con la fórmula, y una
   clase `HlleRiemannSolver : public RiemannSolver` que implemente `flux`,
   `computeFluxes` y `name`.
2. Añade el valor al enum en `simulation_config.hpp`:
   `enum class RiemannSolverKind { HLL, HLLC, HLLE };`
3. Añade la cadena en los dos sentidos: `toString` en `simulation_config.cpp` y
   `parseSolver` en `namelist_parser.cpp`.
4. Añade la rama en `Simulation::buildComponents()`.

El compilador te ayuda: los `switch` sobre el enum no tienen `default`, así que
con `-Wall` avisa de cada uno que te falte por actualizar.

Los cuatro tests que debería pasar cualquier solver nuevo:

```cpp
// consistencia: W_L = W_R  =>  F = F(W)
// upwind supersonico: S_L >= 0  =>  F = F_L
// barrido por lotes == cara individual (tolerancia exacta)
// el resultado de una simulacion completa es finito y positivo
```

Los tres primeros ya están escritos para HLL y HLLC en `test_numerics.cpp` y se
copian en dos minutos.

### Un integrador temporal (RK3, RK4…)

`TimeIntegrator` es independiente del operador espacial. Declara la clase en
`time_integrator.hpp` con los arrays de etapa como **miembros** (para que no
haya asignación de memoria dentro del ciclo temporal) e impleméntala en
`time_integrator.cpp`. `SspRk2` es la plantilla a seguir; fíjate en que llama a
`op.synchronize(...)` entre etapas, que es obligatorio porque el operador
reconstruye sobre primitivas.

Actualmente el integrador está fijado en `buildComponents()`; si quieres
elegirlo por namelist, añade un enum `TimeIntegratorKind` siguiendo los mismos
cuatro pasos de la sección anterior.

### Un término fuente (fricción, calentamiento…)

Implementa `SourceTerm::accumulate(w, dt, u_out)` y añádelo a `source_storage_`
en `buildComponents()`. Se acumulan en orden sobre la actualización ya
ensamblada, así que puedes tener varios activos a la vez. **No** es el sitio
para la geometría: eso es un `DivergenceOperator`, porque cambia la propia
divergencia y no es un término aditivo.

### Una reconstrucción (WENO, PPM…)

Implementa `Reconstruction::computeInterfaceStates`. El contrato es por
**barrido completo**, no por celda, y debes rellenar `left[k]` y `right[k]` para
`k = 0..N`. Si tu esquema necesita una plantilla más ancha que
$\{k-1,\dots,k+2\}$, tendrás que aumentar `kGhostCells` en
`physics/field.hpp` y revisar las tres condiciones de frontera, que actualmente
rellenan exactamente dos fantasmas por lado.

---

## Antes de dar por bueno un cambio

```bash
cmake --build build && ctest --test-dir build --output-on-failure
cmake --build build-debug && ctest --test-dir build-debug --output-on-failure
```

Y si tocaste algo del esquema numérico, la comprobación que de verdad importa:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\validate_against_fortran.ps1
```

Si tu cambio era una refactorización o una optimización, los 26 casos deben
seguir dentro de tolerancia **y el número de pasos debe seguir coincidiendo**.
Si cambia el recuento de pasos, has alterado el resultado numérico —puede estar
justificado, pero entonces hay que decidirlo y documentarlo, no descubrirlo
después.
