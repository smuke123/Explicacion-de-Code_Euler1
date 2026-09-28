# Arquitectura

> **Cómo está organizado este documento.**
> Las secciones 1 a 8 describen la arquitectura **construida** del solver 1D, que
> está implementada y validada. Las secciones 9 a 13 describen la arquitectura
> **objetivo** del framework completo (1D y 2D), los principios de diseño y los
> patrones, e incluyen las decisiones que todavía no se han ejecutado. Cada
> afirmación de esas secciones lleva etiqueta de estado según la convención de
> `project_definition.md`.

## Punto de partida: qué había en el Fortran

El código original son tres archivos y un módulo de estado global:

- `global_numbers.f90` — módulo con **todas** las variables de configuración y
  de estado (`dx`, `t`, `dt`) como variables de módulo mutables, más la lectura
  del namelist.
- `exact_riemann.f90` — módulo autocontenido con el solver exacto. Es la única
  parte del original ya bien encapsulada.
- `euler1d.f90` — el programa: 555 líneas con 13 subrutinas internas
  (`contains`) que comparten por visibilidad léxica los arrays `U`, `U1`, `U2`,
  `W`, `Fnum`, `x` y todo `global_numbers`.

Las dependencias eran, por tanto, implícitas: cualquier subrutina podía leer o
escribir cualquier cosa. El caso más peligroso era `rhs_fv(Uin, Uout)`, que
recibía `Uin` como argumento pero **reconstruía leyendo el array global `W`**.
Pasarle un `Uin` no sincronizado con `W` evaluaba el operador espacial en un
estado distinto del pedido, sin ningún aviso.

## Diseño en C++

```
Simulation                     orquestador: compone y ejecuta el ciclo temporal
├── SimulationConfig           configuración inmutable, validada
├── TestCaseDefinition         datos declarativos de los casos 1..8
├── Mesh1D                     geometría de la malla
├── SimulationState            conserved + primitive + tiempo + paso
├── IdealGas                   EOS y conversiones (único sitio con E(p,rho,u))
├── CflController              dt adaptativo
├── TimeIntegrator             SspRk2 | ForwardEuler
│   └── SemiDiscreteOperator   contrato: u_out = u_in + dt L(u_in)
│       └── FiniteVolumeOperator
│           ├── Reconstruction        MusclReconstruction<Limiter>
│           ├── RiemannSolver         HllRiemannSolver | HllcRiemannSolver
│           ├── DivergenceOperator    CartesianDivergence | RadialDivergence
│           ├── SourceTerm[]          GravitySource
│           └── BoundaryCondition     Transmissive | Periodic | Reflective
├── ExactRiemannSolver         referencia semianalítica (NO avanza el fluido)
└── OutputManager              escritura en el formato del Fortran
```

Componentes auxiliares fuera del ciclo temporal: `ErrorAnalyzer`
(`compareFields`), `xl_reader` (lectura de `.xl` para regresión) y
`fortran_format` (emulación del descriptor `ESw.dEe`).

### Mapa de dependencias

Las flechas van de dependiente a dependencia. No hay ciclos.

```
main
 └── Simulation
      ├── SimulationConfig ── (nada)
      ├── TestCaseDefinition ── SimulationConfig, Mesh1D, CellField
      ├── Mesh1D ── (nada)
      ├── IdealGas ── state.hpp
      ├── SimulationState ── CellField ── state.hpp
      ├── CflController ── Mesh1D, IdealGas, CellField
      ├── TimeIntegrator ── SemiDiscreteOperator, SimulationState
      ├── FiniteVolumeOperator ── Reconstruction, RiemannSolver,
      │                           DivergenceOperator, SourceTerm,
      │                           BoundaryCondition, Mesh1D, IdealGas
      ├── Reconstruction ── SlopeLimiter, CellField, IdealGas
      ├── RiemannSolver ── IdealGas, state.hpp
      ├── ExactRiemannSolver ── state.hpp
      └── OutputManager ── Mesh1D, CellField, fortran_format
```

`state.hpp` (los tres agregados `PrimitiveState`, `ConservedState`,
`FluxVector`) es la única hoja compartida por casi todo el árbol, y no depende
de nada.

## Responsabilidades

| Componente | Responsabilidad única | Lo que deliberadamente NO hace |
|---|---|---|
| `SimulationConfig` | Guardar y **validar** los parámetros | No elige algoritmos ni construye nada |
| `TestCaseDefinition` | Describir un caso de prueba como dato | No muta configuración global |
| `Mesh1D` | Centros, caras, `dx` | No guarda campos |
| `IdealGas` | EOS, conversiones, sonido, flujo físico, pisos | No conoce la malla |
| `CellField<T>` | Almacenamiento contiguo con índices Fortran | No conoce física |
| `SimulationState` | Estado evolutivo | No sabe cómo avanzar |
| `Reconstruction` | Estados de cara a partir de primitivas | No calcula flujos |
| `RiemannSolver` | Flujo numérico dados dos estados | No reconstruye ni limita |
| `DivergenceOperator` | Ensamblar la actualización conservativa | No conoce fuentes físicas |
| `SourceTerm` | Acumular un término fuente aditivo | No toca flujos |
| `BoundaryCondition` | Rellenar celdas fantasma | No aparece en el núcleo del esquema |
| `FiniteVolumeOperator` | Componer los cinco pasos anteriores | No contiene fórmulas propias |
| `TimeIntegrator` | Combinar etapas en el tiempo | No sabe qué es un flujo |
| `CflController` | Calcular `dt` y detectar estados no finitos | No avanza nada |
| `ExactRiemannSolver` | Solución semianalítica de referencia | **No participa en el avance temporal** |
| `OutputManager` | Escribir archivos | No calcula nada |
| `Simulation` | Composición y control del bucle | No contiene fórmulas |

## Flujo de datos

### Arranque

```
main
 ├── readConfigFile(input.par)   → SimulationConfig     (aborta si algo falla)
 ├── config.validate()
 ├── resolveTestCase(config)      → config efectiva + informe de sobreescrituras
 ├── config.validate()            (otra vez: el test pudo cambiar el dominio)
 └── Simulation(config, salida)
      ├── buildComponents()       ← ÚNICO punto donde se eligen algoritmos
      └── initialize()
           ├── applyInitialCondition(...)   → primitivas físicas
           ├── toConserved celda a celda    → conservadas físicas
           └── boundary.apply(u, w)         → celdas fantasma
```

Validar antes **y** después de resolver el caso es intencional: así un valor
inválido del usuario se detecta aunque el test fuera a sobreescribirlo, y a la
vez se comprueba que lo que el test impone sigue siendo coherente.

### Un paso temporal

```
CflController.computeTimeStep(w)          → dt   (aborta si hay NaN/Inf)
recorte: si t + dt > tf entonces dt = tf - t
TimeIntegrator.advance(state, dt, op)
 ├── op.applyUpdate(U,  W, dt, U1)        etapa 1
 ├── op.synchronize(U1, W)                W ← EOS(U1) + fantasmas
 ├── op.applyUpdate(U1, W, dt, U2)        etapa 2 (destino SEPARADO)
 ├── U ← (U + U2) / 2
 └── op.synchronize(U,  W)
```

Y dentro de `applyUpdate` (que corresponde a `rhs_fv`):

```
1. Reconstruction.computeInterfaceStates(W, hidrostático) → qL[0..N], qR[0..N]
2. pisos de positividad sobre qL, qR
3. RiemannSolver.computeFluxes(qL, qR)                    → F[0..N]
4. DivergenceOperator.apply(Uin, W, F, dt)                → Uout[1..N]
5. para cada SourceTerm: accumulate(W, dt, Uout)
```

### Cierre

```
volcado final (si el último paso no se volcó ya)
referenceProfile(t)  → ExactRiemannSolver o sinusoide advectada
OutputManager.writeExact(...)
OutputManager.writeRunInfo(...)   ← reproducibilidad
```

## Decisiones de diseño y por qué

### El estado global desaparece, pero el contrato se hace explícito

`SemiDiscreteOperator::applyUpdate` recibe `w` como parámetro:

```cpp
virtual void applyUpdate(const ConservedField& u_in,
                         const PrimitiveField& w,
                         double dt,
                         ConservedField& u_out) = 0;
```

La dependencia que en el Fortran era invisible (reconstruir sobre el `W`
global) ahora está en la firma. No se puede olvidar sincronizar porque hay que
pasar explícitamente el campo con el que se reconstruye.

### Strategy con granularidad de barrido, no de celda

Los cuatro puntos de variación (reconstrucción, Riemann, divergencia, frontera)
son interfaces virtuales, pero **el método virtual procesa un barrido completo**,
no una celda:

```cpp
virtual void computeFluxes(const std::vector<PrimitiveState>& q_left,
                           const std::vector<PrimitiveState>& q_right,
                           std::vector<FluxVector>& out) const = 0;
```

Así hay una sola llamada virtual por etapa de Runge-Kutta (unas 5 por paso
temporal) en lugar de una por celda y componente. Dentro, el bucle es plano y
vectorizable. Cada solver expone además `flux(qL, qR)` para una sola cara, que
es lo que usan los tests y lo que hace legible la fórmula.

El limitador va un paso más allá: es un **tipo-política** con métodos estáticos,
instanciado por plantilla en `sweep<Limiter>`. El Fortran hacía
`select case (trim(limiter))` dentro del bucle sobre celdas y componentes, es
decir una comparación de cadenas por celda; aquí el coste es cero.

Esto cumple el objetivo arquitectónico del proyecto: añadir HLLE, Roe o WENO
significa escribir una clase nueva y una rama en `buildComponents()`. El núcleo
de volúmenes finitos no se toca.

### Geometría como divergencia, no como fuente

El Fortran mezclaba ambas cosas en `rhs_fv` y, cuando `alpha_geo > 0`, calculaba
la actualización cartesiana para luego **descartarla** y rehacerla en forma
radial. Aquí son dos abstracciones distintas:

- `DivergenceOperator` — la geometría cambia la propia divergencia (áreas
  $A = r^\alpha$ y volúmenes exactos), no añade un término.
- `SourceTerm` — lo que sí es aditivo (gravedad).

El término geométrico de presión $p(A_+ - A_-)/V$ vive en `RadialDivergence`
porque es inseparable de esa discretización: es lo que hace que una presión
uniforme no acelere el fluido.

### `conserved` y `primitive` coexisten

Son redundantes: `primitive` es la imagen de `conserved` bajo la EOS. Se
mantienen las dos porque el esquema reconstruye sobre primitivas y recalcularlas
en cada acceso costaría una raíz y una división por celda. La invariante
(«`primitive` está sincronizado con `conserved`») la restablece un único método,
`synchronize`.

### Índices idénticos a los del Fortran

`CellField` acepta `k` de `-1` a `N+2`, con `1..N` las celdas físicas, tal como
`U(3,-1:N+2)`. Internamente el slot es `k + 1`. Se conservó la convención
Fortran (base 1) en lugar de pasar a base 0 por una razón concreta: permite
auditar la migración comparando línea a línea con el original, que es el
criterio de aceptación del proyecto. La correspondencia está documentada en
`field.hpp` y en `docs/numerical_method.md`.

El layout es AoS: las tres componentes de una celda son contiguas, igual que el
orden column-major de Fortran para `U(3,k)`. Así se preserva el patrón de acceso
a memoria del original.

### Patrones que NO se usaron

- **Factory / Abstract Factory** para los componentes: `buildComponents()` es un
  `switch` de veinte líneas sobre un enum. Una jerarquía de fábricas añadiría
  indirección sin resolver ningún problema real.
- **Observer** para la salida: hay un único consumidor (`OutputManager`) y el
  ciclo temporal sabe cuándo volcar. Un sistema de eventos sería ceremonia.
- **Herencia para los limitadores**: son funciones puras sin estado; una interfaz
  virtual costaría una llamada indirecta por celda a cambio de nada.
- **`std::variant` en lugar de interfaces virtuales**: con una llamada virtual
  por barrido, el coste es indetectable, y las interfaces se leen mejor para un
  estudiante.

## Correspondencia Fortran → C++

| Fortran | C++ |
|---|---|
| `global_numbers` (variables de módulo) | `SimulationConfig` (por valor, sin estado global) |
| `read_input` | `readConfigFile` / `parseNamelist` |
| `validate_input` | `SimulationConfig::validate` + enums del parser |
| `set_test` | `TestCaseDefinition` + `resolveTestCase` |
| condición inicial en el cuerpo del programa | `applyInitialCondition` |
| `prim2cons_all` | `IdealGas::toConserved` (por celda) |
| `cons2prim_all` | `IdealGas::toPrimitive` + `FloorCounters` |
| `phys_flux` | `IdealGas::physicalFlux` |
| `boundary`, `boundary_of` | `BoundaryCondition` y sus tres implementaciones |
| `compute_dt` | `CflController` |
| `reconstructor` (función con `select case`) | `limiters::*` + `limitedSlope` |
| bucle MUSCL dentro de `rhs_fv` | `MusclReconstruction::computeInterfaceStates` |
| `riemann_flux` (con `if solver == 'hll'`) | `HllRiemannSolver` / `HllcRiemannSolver` |
| ensamblado y fuentes en `rhs_fv` | `DivergenceOperator` + `SourceTerm` |
| bucle `do while (t < tf)` | `Simulation::run` |
| SSP-RK2 en línea (con aliasing) | `SspRk2` (destinos separados) |
| `save_output` | `OutputManager::writeInitial` / `appendSnapshot` |
| `save_exact`, `save_exact_suave` | `Simulation::referenceProfile` + `writeExact` |
| formato `es20.10e3` | `formatEs`, `formatDataRow`, `formatTimeHeader` |
| `exact_riemann` módulo | `ExactRiemannSolver` |
| `fK` | `ExactRiemannSolver::pressureFunction` |
| `exact_star` | `ExactRiemannSolver::solveStar` |
| `exact_sample` | `ExactRiemannSolver::sample` |
| (no existía) | `run_info.txt`, `euler1d_compare`, suite de tests |

## Estructura de archivos

```
Euler1D_Cpp/
├── CMakeLists.txt
├── README.md
├── input.par
├── examples/                 configuraciones listas para usar
├── include/euler1d/
│   ├── config/               simulation_config.hpp
│   ├── initial/              test_case.hpp
│   ├── mesh/                 mesh1d.hpp
│   ├── physics/              state.hpp, field.hpp, ideal_gas.hpp
│   ├── numerical/
│   │   ├── reconstruction/   reconstruction.hpp, slope_limiter.hpp
│   │   ├── riemann/          riemann_solver.hpp, hll.hpp, hllc.hpp,
│   │   │                     exact_riemann_solver.hpp
│   │   ├── source/           source_term.hpp
│   │   ├── time/             time_integrator.hpp
│   │   ├── cfl_controller.hpp
│   │   ├── semi_discrete_operator.hpp
│   │   └── finite_volume_operator.hpp
│   ├── boundary/             boundary_condition.hpp
│   ├── io/                   output_manager.hpp, xl_reader.hpp, fortran_format.hpp
│   ├── simulation/           simulation.hpp, simulation_state.hpp
│   └── validation/           error_analyzer.hpp
├── src/                      implementaciones (misma jerarquía) + main.cpp
│   └── tools/                compare_runs.cpp
├── scripts/                  validate_against_fortran.ps1
├── reference_fortran/        copia del proyecto Fortran original (solo lectura)
├── tests/                    suite + reference/ (salidas archivadas del Fortran)
└── docs/
```

`reference_fortran/` no participa en la compilación de CMake: está para que la
validación se pueda ejecutar teniendo solo este repositorio, y para poder
consultar el original al leer los comentarios que lo citan.

Las cabeceras que solo contienen tipos pequeños o funciones `inline` (`state.hpp`,
`field.hpp`, `ideal_gas.hpp`, `mesh1d.hpp`, `hll.hpp`, `hllc.hpp`,
`slope_limiter.hpp`) no tienen `.cpp`: partirlas artificialmente impediría el
`inline` justo en los puntos más calientes.

---

# 9. Arquitectura objetivo del framework (1D y 2D)

Todo lo anterior describe el solver 1D construido. Esta sección describe a dónde
tiene que llegar el diseño, y es donde están las decisiones abiertas.

## 9.1 La decisión que condiciona todo: dimensionalidad

`PROPOSED` — requiere el *spike* de la sección 9.3 antes de comprometerse.

### El problema

El framework debe cubrir 1D y 2D. La pregunta es cómo, y no es una cuestión
estética: **si cada solver de Riemann existe dos veces, entonces «añadir HLLE»
significa escribirlo dos veces, y eso contradice directamente la afirmación que
la tesis tiene que demostrar** (`Obj-1`: intercambiar un componente sin
reingeniería). Los solvers de Riemann tienen que existir una sola vez.

### Opciones consideradas

**Opción A — dos implementaciones paralelas.** Un árbol 1D y un árbol 2D
independientes, compartiendo solo lo trivialmente común (limitadores, ecuación
de estado, análisis de error, configuración).

- A favor: cada archivo es más simple; el 1D validado no se toca en absoluto.
- En contra: duplica el álgebra de HLL, HLLE y HLLC (unas 180 líneas). Las dos
  copias divergen con el tiempo. **Y debilita el resultado central de la tesis.**

**Opción B — la dimensión como parámetro de tiempo de compilación.** Los
contenedores y los barridos se parametrizan; los solvers de Riemann se escriben
una vez sobre un estado con velocidad normal explícita y tangenciales pasivas.

- A favor: una sola fuente de verdad para la numérica; coste en ejecución nulo;
  el 1D no arrastra memoria ni operaciones del 2D.
- En contra: plantillas en las cabeceras, compilación más lenta, y un salto de
  dificultad de lectura que hay que compensar con documentación.

**Opción C — dimensión en tiempo de ejecución.** Arreglos dinámicos y ramas por
dimensión dentro del código.

- En contra: ramas en los bucles calientes y el 1D pagando la indexación del 2D.
  Viola NFR-007. **Descartada.**

### Opción recomendada: B

El estado que ve el solver de Riemann está en el marco de la cara: una velocidad
normal y \(N_T\) velocidades tangenciales, con \(N_T = 0\) en 1D y \(N_T = 1\) en
2D. Es exactamente la abstracción que usa el solver 2D de referencia en su
`riemann_dir`, sobre \(q = (\rho, u_n, u_t, p)\).

```cpp
/// Estado en el marco de la cara. NT = número de velocidades tangenciales:
/// 0 en 1D, 1 en 2D. Las tangenciales son pasivas: se advectan con el contacto.
template <int NT>
struct FaceState {
    double rho;
    double un;                      // velocidad normal a la cara
    std::array<double, NT> ut;      // tangenciales; vacío y sin coste si NT == 0
    double p;
};
```

El álgebra se escribe una vez. Con `NT == 0` el bucle sobre tangenciales no tiene
iteraciones y el arreglo no ocupa espacio:

```cpp
template <int NT>
FaceFlux<NT> hllcFlux(const IdealGas& gas,
                      const FaceState<NT>& qL, const FaceState<NT>& qR);
```

Tres consecuencias de diseño que se derivan de esto:

1. **La rotación vive en el barrido, no en el solver.** Para una cara en y, el
   barrido empaqueta \((\rho, v, u, p)\), llama al solver y desempaqueta el flujo
   invirtiendo la permutación. El solver nunca sabe qué dirección está
   resolviendo. Así la rotación queda localizada en un sitio y se puede probar
   por separado — que es justo lo que hizo la auditoría del código de referencia,
   verificando simetría por transposición a \(10^{-16}\).
2. **La interfaz virtual se instancia dos veces**, como `RiemannSolver<0>` y
   `RiemannSolver<1>`. Son tipos distintos, pero **la implementación es una
   sola**: `HllcRiemannSolver<NT>` se instancia para los dos valores.
3. **El 1D no paga nada.** Ni memoria ni operaciones, porque `std::array<double,
   0>` está vacío y los bucles de longitud cero desaparecen en compilación.

### 9.2 Coste de la generalización sobre el 1D ya validado

Este es el riesgo principal del proyecto y hay que dimensionarlo antes de
empezar. Componentes afectados:

| Componente | Cambio | Riesgo de romper la regresión |
|---|---|---|
| `PrimitiveState`, `ConservedState`, `FluxVector` | Pasan a `FaceState<NT>` en la frontera del solver | **Alto**: tocan todo |
| `hllFlux`, `hllcFlux` | Se plantillan | Alto |
| `CellField<T>` | Se añade `CellField2D<T>`; anchura de fantasmas configurable | Medio |
| `MusclReconstruction` | El barrido pasa a ser direccional | Medio |
| `CflController` | Segunda fórmula para 2D sin separación (`INV-006`) | Bajo: es un método nuevo, no una modificación |
| `BoundaryCondition` | Por borde en lugar de por dominio | Medio |
| `Mesh1D` | Sin cambios; se añade `Mesh2D` | Ninguno |
| `IdealGas` | La energía cinética pasa a sumar sobre componentes | Bajo |
| `ExactRiemannSolver` | Sin cambios: es 1D por naturaleza | Ninguno |
| `OutputManager` | Escritor 2D nuevo | Ninguno sobre 1D |
| `TimeIntegrator` | Sin cambios: opera sobre el operador, no sobre el campo | Ninguno |

Que el integrador temporal y el solver exacto no se toquen es una señal de que la
separación de responsabilidades del 1D era correcta.

### 9.3 Spike de confirmación

`PROPOSED`. Antes de comprometer la opción B, y **antes de escribir una línea de
2D**:

- **Duración:** un día, acotado.
- **Trabajo:** plantillar `FaceState`, `FaceFlux`, `hllFlux` y `hllcFlux`, e
  instanciar solo con `NT = 0`. No se escribe nada de 2D.
- **Criterio de éxito:** la suite completa sigue verde **y** la regresión contra
  el Fortran sigue dando 26 de 26 con el número de pasos idéntico y los 9 casos
  byte a byte que ya coincidían.
- **Criterio de aborto:** si la regresión cambia en cualquier grado, o si la
  revisión de legibilidad por parte de los dos autores es negativa, se pasa a la
  opción A y se acepta la duplicación, **documentándola** como deuda técnica
  consciente con su justificación.

Hacer el spike con `NT = 0` únicamente es lo que permite separar «la plantilla
rompió algo» de «el 2D está mal».

## 9.4 La cuestión del `Mesh` abstracto

`PROPOSED` · `PENDING INPUT FROM DIRECTOR`

El anteproyecto (§4.2, Etapa 1) enumera `RiemannSolver`, `Reconstructor`,
`TimeIntegrator` y **`Mesh`** como «interfaces abstractas fundamentales» que
quedarán «completamente definidas» antes de implementar.

Para tres de las cuatro, una clase base virtual es correcta: son puntos de
variación reales y se invocan una vez por barrido. Para `Mesh` **no lo es**. La
malla se consulta dentro de los bucles celda a celda (`dx`, centro de celda,
áreas de cara), de modo que una llamada virtual ahí violaría NFR-007 por una
abstracción que no resuelve ningún problema: no hay ni habrá despacho dinámico
sobre el tipo de malla, porque la dimensión se conoce en tiempo de compilación.

Propuesta: `Mesh` es un **contrato de tiempo de compilación** —un *concept*—
satisfecho por `Mesh1D` y `Mesh2D`. Aparece como interfaz en el diagrama de
componentes y su contrato se documenta igual de formalmente que los otros tres,
pero no genera tabla virtual.

Es exactamente el tipo de caso que la sección 11.2 llama «dónde no
sobre-aplicar», y conviene tenerlo acordado por escrito antes de la defensa, no
durante.

---

# 10. Inventario de componentes y estado

| Componente | Responsabilidad | 1D | 2D |
|---|---|---|---|
| `SimulationConfig` | Guardar y validar parámetros | ✅ | Ampliar a dominio y fronteras por eje |
| Lector de namelist | Adaptar `input.par` a configuración | ✅ | Ampliar |
| Lector JSON | Adaptar JSON a configuración | ⬚ | ⬚ |
| `TestCaseDefinition` | Casos como dato declarativo | ✅ | ⬚ |
| `Mesh1D` / `Mesh2D` | Geometría de la malla | ✅ | ⬚ |
| `CellField<T>` / `CellField2D<T>` | Almacenamiento con fantasmas | ✅ | ⬚ |
| `IdealGas` | Ecuación de estado, conversiones, flujo físico, pisos | ✅ | Energía cinética multicomponente |
| `SimulationState` | Estado evolutivo | ✅ | ⬚ |
| `FirstOrderReconstruction` | Reconstrucción de orden 1 | ⚠ interna a MUSCL | ⬚ |
| `MusclReconstruction` | Reconstrucción de orden 2 | ✅ | ⬚ |
| `WenoReconstruction` | Reconstrucción de orden 3+ | ⬚ | ⬚ |
| Limitadores | minmod, van Leer, monotonizado central, superbee | ✅ | Reutilizables sin cambio |
| `HllRiemannSolver` | Flujo de dos ondas, Davis | ✅ | Vía plantilla |
| `HlleRiemannSolver` | Flujo de dos ondas, Einfeldt | ⬚ | ⬚ |
| `HllcRiemannSolver` | Flujo de tres ondas | ✅ | Vía plantilla |
| `BoundaryCondition` | Rellenar fantasmas | ✅ | Por borde |
| `DivergenceOperator` | Ensamblar la actualización conservativa | ✅ | Versión 2D sin separación |
| `SourceTerm` | Términos aditivos | ✅ | — (fuera de alcance) |
| `CflController` | `dt` adaptativo y detección de no finitos | ✅ | Fórmula distinta (`INV-006`) |
| `TimeIntegrator` | Etapas de Runge-Kutta | ✅ | Sin cambios |
| `FiniteVolumeOperator` | Composición del operador espacial | ✅ | Barridos x e y |
| `ExactRiemannSolver` | Referencia semianalítica | ✅ | — (no existe en 2D) |
| `ErrorAnalyzer` | Métricas de error | ✅ | Reutilizable |
| `OutputManager` | Escritura | ✅ | Escritor VTK |
| `Simulation` | Orquestación | ✅ | ⬚ |

✅ implementado y probado · ⚠ existe pero no como módulo separado · ⬚ pendiente

---

# 11. Principios de diseño

## 11.1 Principios que se aplican, y por qué

**Separación de responsabilidades.** Cada componente tiene una razón para
cambiar. El criterio operativo: si al cambiar de solver de Riemann hay que tocar
la reconstrucción, la separación está mal hecha.

**Responsabilidad única.** La tabla de la sección 4 incluye una columna «lo que
deliberadamente NO hace». Esa columna es la parte útil: enunciar qué queda fuera
es lo que impide que un componente crezca sin control.

**Inversión de dependencias, pero solo donde hay variación real.** El operador de
volúmenes finitos depende de las interfaces de reconstrucción, Riemann,
divergencia, fuente y frontera. Son cinco puntos de variación, y son cinco porque
los objetivos exigen alternativas en cada uno. **No** se invierte la dependencia
sobre `IdealGas`, `Mesh` ni `CellField`: no hay requisito de alternativas y una
interfaz con una sola implementación es coste sin beneficio.

**Composición sobre herencia.** La única herencia del diseño es la
implementación de interfaces. No hay jerarquías de tipos de datos ni clases base
con estado.

**RAII y propiedad explícita.** Los componentes se poseen por
`std::unique_ptr` en `Simulation`, que es su único dueño. Todo lo demás se pasa
por referencia `const` y nadie almacena punteros a lo que no posee.

**Corrección de `const`.** Los componentes numéricos son `const` tras su
construcción; sus métodos son `const`. Un solver de Riemann con estado mutable
sería un error de diseño y la firma lo impide.

**Determinismo.** Ninguna dependencia de orden de iteración no determinista, de
generadores aleatorios ni de direcciones de memoria. Es prerrequisito de NFR-002
y, por tanto, de toda la estrategia de regresión.

**Sin estado global mutable.** Es el defecto estructural del código de referencia
y la causa directa de su fallo más grave. Ver NFR-006.

**Configuración explícita.** Ningún valor por defecto oculto en el código. Los
defaults pertenecen al caso de prueba y se reportan cuando se aplican.

## 11.2 Dónde NO sobre-aplicar

Esta sección es tan importante como la anterior y suele faltar.

**No hay despacho dinámico dentro de bucles celda a celda.** El método virtual
recibe un barrido completo. Una llamada virtual por etapa de Runge-Kutta, no una
por celda y componente. Los limitadores van más allá: son tipos-política
resueltos en compilación, porque son funciones puras sin estado y una interfaz
virtual costaría una indirección por celda a cambio de nada.

**No se abstrae la ecuación de estado.** Hay un modelo de gas —ideal
politrópico— y ningún objetivo pide otro. Una interfaz `EquationOfState` con una
sola implementación añadiría indirección en el punto más caliente del código sin
resolver ningún problema presente.

**No se abstrae la malla como clase virtual.** Ver sección 9.4.

**No hay interfaces con una sola implementación**, salvo que un objetivo exija
explícitamente la alternativa. `SourceTerm` tiene hoy una sola implementación y
sí es interfaz, porque el requisito de activar y desactivar términos
independientemente es real.

**No hay accesores triviales sobre tipos valor.** `PrimitiveState` expone
`rho`, `u`, `p` como miembros públicos. Envolverlos en métodos haría el código
numérico ilegible sin encapsular nada: no hay invariante que proteger.

**No se optimiza antes de medir.** El orden es: correcto, validado, medido, y
solo entonces optimizado. Cualquier optimización debe ir acompañada de la medida
que la justifica y de la regresión que demuestra que no cambió el resultado.

**La legibilidad del código numérico gana a la pureza arquitectónica.** Cada
solver expone una función de una sola cara, legible y directamente comparable con
la fórmula del texto, además del barrido optimizado. La duplicación aparente está
justificada: una es para leer y verificar, la otra para ejecutar, y un test
comprueba que coinciden exactamente.

---

# 12. Patrones de diseño

Solo los que resuelven un problema presente. El objetivo no es maximizar el
número de patrones.

## 12.1 Strategy — **sí**, es el patrón central

**Dónde.** Los cinco puntos de variación: reconstrucción, solver de Riemann,
operador de divergencia, término fuente y condición de frontera.

**Qué problema resuelve.** Es literalmente el objetivo del proyecto:
intercambiar un algoritmo sin modificar quien lo usa. Sin Strategy, cambiar de
solver sería un `if` dentro del bucle de flujos, que es lo que hace el código de
referencia y lo que la justificación de la tesis identifica como el problema.

**Complejidad que introduce.** Una tabla virtual por interfaz y una indirección
por barrido. Medible y despreciable.

**Variante usada.** Granularidad de barrido, no de celda. Es la decisión que hace
que el patrón sea gratis. Ver sección 11.2.

## 12.2 Policy / plantillas — **sí**, en dos sitios

**Dónde.** Los limitadores de pendiente, como tipos con métodos estáticos
instanciados en `sweep<Limiter>`. Y la dimensión, como parámetro `NT` (sección
9.1).

**Qué problema resuelve.** Variación sin coste en ejecución, donde el punto de
variación está dentro de un bucle caliente. El código de referencia hacía
`select case (trim(limiter))` por celda y por componente: una comparación de
cadenas por celda.

**Complejidad que introduce.** Código en cabeceras, mensajes de error de
plantilla más difíciles, compilación más lenta. Aceptable en los limitadores;
**en la dimensión es el coste que el spike de 9.3 tiene que validar.**

## 12.3 Adapter — **sí**

**Dónde.** Los lectores de configuración. El lector de namelist y el lector JSON
adaptan dos formatos externos al mismo `SimulationConfig` validado.

**Qué problema resuelve.** Permite cumplir el requisito del anteproyecto (JSON)
sin perder la capacidad de ejecutar los `input.par` del código de referencia, que
es lo que sostiene la regresión. Los dos formatos convergen en un único punto de
validación.

**Complejidad que introduce.** Un segundo lector. Trivial, y es la demostración
práctica del desacoplamiento que defiende el `Obj-1`.

## 12.4 Facade — **sí**, de facto

**Dónde.** `Simulation`. Expone `run()` sobre un subsistema de una docena de
componentes.

**Qué problema resuelve.** `main` no necesita conocer el operador semidiscreto ni
el controlador de CFL. Y el usuario de la biblioteca —por ejemplo, un test de
integración— construye una simulación con una configuración y la ejecuta.

**Complejidad que introduce.** Ninguna añadida: el orquestador hace falta de
todas formas. Se nombra el patrón porque describe con precisión lo que es.

## 12.5 Inyección de dependencias — **sí**, por constructor y manual

**Dónde.** Los componentes se construyen en un único punto y se inyectan al
operador de volúmenes finitos por constructor.

**Qué problema resuelve.** Testabilidad. Un test puede construir el operador con
un solver concreto y sin leer un archivo de configuración.

**Complejidad que introduce.** Ninguna. **No** se usa contenedor de inyección:
sería infraestructura empresarial para resolver un grafo de doce nodos conocido
en tiempo de compilación.

## 12.6 Factory — **parcialmente**, y hay un conflicto que resolver

El anteproyecto (§4.4) dice que el núcleo «instancia las estrategias
correspondientes mediante una **fábrica de objetos (factory pattern)**».

Lo que el diseño tiene hoy es una función única, `buildComponents()`, con un
`switch` de veinte líneas sobre enumerados. Funcionalmente **es** una fábrica: es
el único lugar que conoce los tipos concretos y traduce configuración en objetos.
Lo que no es, es una jerarquía de clases fábrica.

Propuesta: extraerla a un `ComponentFactory` con métodos estáticos
(`makeRiemannSolver(config)`, `makeReconstruction(config)`, …). Satisface la
letra del anteproyecto, mejora la testabilidad —se puede pedir un componente
suelto sin construir una simulación— y **no** introduce jerarquía ni registro
dinámico. Una `AbstractFactory` con subclases sí sería complejidad sin
beneficio, y se descarta explícitamente.

Registrado como `ADR-008`.

## 12.7 Patrones descartados explícitamente

| Patrón | Por qué no |
|---|---|
| **Abstract Factory** | No hay familias de productos que deban variar juntas |
| **Observer** para la salida | Un único consumidor, y el ciclo temporal sabe cuándo volcar. Sería ceremonia |
| **Singleton** para la configuración | Reintroduciría el estado global que el diseño elimina (NFR-006) |
| **Visitor** sobre los estados | Los tipos de estado son estables y pocos; un visitante añadiría indirección sin resolver nada |
| **Template Method** en el operador | La composición explícita se lee mejor que una clase base con ganchos |
| **Command** para las etapas temporales | Las etapas son llamadas a función, no objetos que haya que encolar o deshacer |
| **CRTP** en los solvers | Con una llamada virtual por barrido el coste es indetectable, y las interfaces virtuales se leen mucho mejor para un estudiante |
| **Pimpl** | No hay ABI que estabilizar: es un ejecutable, no una biblioteca distribuida |
| **`std::variant`** en lugar de interfaces | Igual que CRTP: el coste evitado es nulo y la legibilidad empeora |

---

# 13. Diagrama de componentes

Éste es el diagrama que el `Obj-1` pide como entregable. Las flechas van de
dependiente a dependencia; no hay ciclos.

```
                         ┌──────────────┐
                         │     main     │
                         └──────┬───────┘
                                │
              ┌─────────────────▼──────────────────┐
              │            Simulation              │  «Facade»
              │  orquesta; no contiene fórmulas    │
              └──┬──────┬──────┬──────┬──────┬─────┘
                 │      │      │      │      │
   ┌─────────────▼┐ ┌───▼───┐ ┌▼─────┐│ ┌────▼────────┐
   │ Component    │ │ Cfl   │ │ Time ││ │  Output     │
   │ Factory      │ │ Ctrl  │ │ Integ││ │  Manager    │
   └─────┬────────┘ └───────┘ └──┬───┘│ └─────────────┘
         │ construye              │    │
         │                        │    └──► ExactRiemannSolver
         │            ┌───────────▼──────────┐   (solo referencia;
         │            │ SemiDiscreteOperator │    NO avanza el fluido)
         │            │      «interfaz»      │
         │            └───────────┬──────────┘
         │                        │
         │            ┌───────────▼──────────┐
         └───────────►│ FiniteVolumeOperator │
                      └──┬───┬───┬───┬───┬───┘
                         │   │   │   │   │
      ┌──────────────────┘   │   │   │   └──────────────────┐
      │        ┌─────────────┘   │   └─────────┐            │
      ▼        ▼                 ▼             ▼            ▼
┌───────────┐┌──────────────┐┌──────────┐┌──────────┐┌─────────────┐
│Reconstruc-││ RiemannSolver││Divergence││SourceTerm││  Boundary   │
│  tion     ││              ││ Operator ││          ││  Condition  │
│«interfaz» ││ «interfaz»   ││«interfaz»││«interfaz»││ «interfaz»  │
├───────────┤├──────────────┤├──────────┤├──────────┤├─────────────┤
│FirstOrder ││ Hll          ││Cartesian ││Gravity   ││Transmissive │
│Muscl<Lim> ││ Hlle         ││Radial    ││          ││Periodic     │
│Weno       ││ Hllc         ││Unsplit2D ││          ││Reflective   │
└───────────┘└──────────────┘└──────────┘└──────────┘└─────────────┘

Tipos de soporte, sin dependencias salientes:
  FaceState<NT>, FaceFlux<NT>   estados y flujos en el marco de la cara
  CellField<T>, CellField2D<T>  almacenamiento con celdas fantasma
  Mesh1D, Mesh2D                «contrato Mesh», satisfecho en compilación
  IdealGas                      ecuación de estado
  SimulationConfig              configuración validada, inmutable
```

Lectura del diagrama en una frase: **hay exactamente cinco interfaces con
implementaciones alternativas, y todas se invocan desde un único componente.** Esa
concentración es lo que hace que añadir un esquema no toque el núcleo.

Los dos elementos que se salen del flujo principal están ahí a propósito.
`ExactRiemannSolver` cuelga de `Simulation` y no del operador, porque no
participa en el avance temporal. `ComponentFactory` es el único que conoce los
tipos concretos, de modo que el resto del sistema solo ve interfaces.
