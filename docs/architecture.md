# Arquitectura

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
