# Rendimiento y preparación para OpenMP

## Medición actual

Máquina y toolchain: los mismos de [validation.md](validation.md). Fortran a
`-O2` (los flags de su `Makefile`), C++ a `-O3 -DNDEBUG` (Release).

Caso: `test = 3` (blast izquierdo), `hllc`, `vl`, `cfl = 0.9`. Tiempo total del
proceso, tres repeticiones:

| N | pasos | Fortran | C++ | C++ / Fortran |
|---|---|---|---|---|
| 1000 | 713 | 0.310 s | 0.076 s | 0.24 |
| 4000 | 2848 | 3.33 ± 0.01 s | 0.614 ± 0.010 s | 0.18 |

Con `limiter = 'fo'` y N = 4000: Fortran 2.78 s, C++ 0.55 s (razón 0.20).

El C++ resulta **≈ 5 veces más rápido** con los mismos resultados numéricos.

Rendimiento absoluto del C++ en N = 4000: 2848 pasos × 2 etapas RK ×
4000 celdas = 2.28 × 10⁷ actualizaciones celda-etapa en 0.5995 s de ciclo
temporal (medido internamente y registrado en `run_info.txt`), es decir
**≈ 38 millones de actualizaciones celda-etapa por segundo** en un solo hilo.

> **Advertencia sobre la atribución.** No se ejecutó un *profiler*, así que la
> diferencia de 5× no está atribuida a causas concretas con datos. Las
> diferencias estructurales conocidas entre ambas implementaciones son las de
> la sección siguiente; cuál pesa cuánto está sin medir. El nivel de
> optimización tampoco es el mismo (`-O2` contra `-O3`), y eso solo ya explica
> una parte.

### Diferencias estructurales entre las dos implementaciones

1. **Selección del limitador.** El Fortran ejecuta
   `select case (trim(limiter))` en el bucle interno, es decir una comparación
   de cadenas (con `trim`, que en gfortran puede materializar un temporal) por
   celda, por componente y por etapa. En N = 4000 eso son ~7 × 10⁷ comparaciones
   de cadena por corrida. El C++ instancia `sweep<Limiter>` una vez por barrido y
   el cuerpo del bucle no tiene ninguna rama de selección.
2. **Trabajo muerto en geometría radial.** Con `alpha_geo > 0`, el Fortran
   calcula la actualización cartesiana completa y después la **descarta** para
   rehacerla en forma radial. En C++ son dos `DivergenceOperator` distintos y
   solo se ejecuta el que corresponde. (No afecta a la medición de arriba, que
   es plana.)
3. **Selección del solver de Riemann.** El Fortran compara
   `if (trim(solver) == 'hll')` dentro de `riemann_flux`, o sea una vez por
   cara. En C++ el solver concreto se fija en el constructor.
4. **Nivel de optimización**: `-O2` contra `-O3`.

### Decisiones que sí se tomaron por rendimiento

- **Una llamada virtual por barrido, no por celda.** Reconstrucción, Riemann,
  divergencia y frontera son interfaces virtuales cuyo método procesa el array
  completo. Son ~5 llamadas indirectas por paso temporal, no ~5N.
- **Limitadores como tipos-política.** Coste de selección cero dentro del bucle,
  y el cuerpo se puede vectorizar.
- **Reserva única de memoria.** `q_left_`, `q_right_` y `fluxes_` se
  dimensionan en el constructor de `FiniteVolumeOperator`; los arrays de etapa
  (`stage1_`, `stage2_`) en el de `SspRk2`. **El ciclo temporal no hace ninguna
  asignación de memoria.**
- **Almacenamiento contiguo AoS.** `CellField` es un único `std::vector`; las
  tres componentes de una celda son adyacentes, igual que el orden
  column-major de Fortran para `U(3,k)`.
- **Sin copias de estados en los bucles.** Se usan referencias `const`; los
  componentes guardan punteros a la malla y a los colaboradores, no copias.
- **`IdealGas` se copia por valor** dentro de los componentes: son tres `double`,
  y así el compilador puede mantener `gamma` en un registro en lugar de leerlo
  por puntero en cada celda.
- **Exponenciación binaria en lugar de `std::pow(double,int)`** para $r^\alpha$:
  además de ser lo que compila gfortran con `__builtin_powi`, evita una llamada
  a libm por celda.
- **`-ffast-math` NO se activa.** Reasociaría las operaciones en coma flotante y
  rompería la equivalencia numérica, que tiene prioridad sobre la velocidad
  según los requisitos del proyecto.

## Regiones paralelizables

El esquema es explícito: dentro de una etapa de Runge–Kutta, cada celda y cada
cara se calculan de forma independiente. Todos los bucles siguientes están
marcados con un comentario en el código.

| Región | Archivo | Iteraciones | Patrón | Dependencias |
|---|---|---|---|---|
| Reconstrucción MUSCL | `reconstruction.cpp`, `sweep()` | N+1 caras | `parallel for` | Solo lectura de `w[k-1..k+2]`; escribe `left[k]`, `right[k]` |
| Pisos en estados de cara | `finite_volume_operator.cpp` | N+1 | `parallel for` | Elemento a elemento |
| Flujos de Riemann | `hll.hpp`, `hllc.hpp`, `computeFluxes()` | N+1 | `parallel for` | Cada cara es independiente |
| Divergencia cartesiana | `source_term.cpp`, `CartesianDivergence::apply` | N | `parallel for` | Lee `fluxes[k-1]`, `fluxes[k]`; escribe `u_out[k]` |
| Divergencia radial | `source_term.cpp`, `RadialDivergence::apply` | N | `parallel for` | Igual, más geometría local |
| Fuente de gravedad | `source_term.cpp`, `GravitySource::accumulate` | N | `parallel for` | Elemento a elemento |
| Conversión U→W | `finite_volume_operator.cpp`, `synchronize` | N | `parallel for` | Elemento a elemento, salvo `FloorCounters` |
| Velocidad de señal | `cfl_controller.cpp`, `scan` | N | `reduction(max:) reduction(+:)` | Reducción doble |
| Combinación RK2 | `time_integrator.cpp` | N | `parallel for` | Elemento a elemento |

Estimación: en un paso temporal, ~95 % de las operaciones en coma flotante caen
en estas regiones. La ley de Amdahl no es el límite; el ancho de banda de
memoria sí lo será.

### Lo que NO es paralelizable tal cual

- **El bucle temporal.** Hay dependencia serial estricta entre pasos.
- **Las condiciones de frontera.** Son O(1): ocho celdas por llamada. Paralelizar
  costaría más que ejecutarlas.
- **La construcción hidrostática de la presión inicial** (test 7): es una
  recurrencia $p_k = p_{k-1} - g\Delta x(\rho_{k-1}+\rho_k)/2$. Se ejecuta una
  sola vez, así que no importa.
- **La escritura de archivos.** E/S serial por naturaleza.

### Puntos que hay que resolver antes de paralelizar

1. **`FloorCounters` es estado compartido mutable.** `IdealGas::toPrimitive`
   incrementa `counters->density`, `counters->pressure` y actualiza
   `min_internal_energy` dentro del bucle de `synchronize`. Con varios hilos hay
   carrera. Solución: contadores por hilo combinados al final
   (`reduction(+:...)` para los conteos y `reduction(min:...)` para la energía),
   o `atomic` —pero los pisos se disparan raras veces, así que la reducción es
   preferible a pagar un atómico por celda.
2. **La reducción del CFL es doble.** `scan()` calcula a la vez el máximo y la
   suma de comprobación de finitud. Necesita
   `reduction(max:max_speed) reduction(+:checksum)`. La suma en paralelo cambia
   el orden de acumulación y por tanto su último bit; como solo se usa para
   `isfinite`, es irrelevante, pero conviene tenerlo presente.
3. **El resultado dejaría de ser bit a bit reproducible** si alguna reducción en
   coma flotante cambia de orden. El máximo es asociativo y seguro; la suma de
   comprobación no lo es. Ninguna reducción en suma afecta al resultado físico
   en la implementación actual, pero cualquier reducción nueva (energía total,
   normas) rompería la reproducibilidad y hay que decidirlo explícitamente.
4. **Falsa compartición.** `CellField` es AoS con `PrimitiveState` de 24 bytes.
   En un reparto estático por bloques, los hilos vecinos escriben en la misma
   línea de caché en la frontera del bloque. Con N grande el efecto es marginal;
   conviene `schedule(static)` con bloques grandes, no `schedule(dynamic)`.

### Cómo activarlo

```bash
cmake -S . -B build -DEULER1D_ENABLE_OPENMP=ON
```

La opción existe y enlaza `OpenMP::OpenMP_CXX`, pero **no hay ninguna directiva
`#pragma omp` en el código todavía**. Está desactivada por defecto a propósito:
el orden de prioridades del proyecto pone la validación por delante de la
paralelización, y ya hay una versión secuencial validada contra el Fortran que
sirve de referencia para comprobar que la paralela da lo mismo.

## Optimizaciones consideradas y descartadas

| Idea | Por qué no |
|---|---|
| `-ffast-math` / `-Ofast` | Rompe la equivalencia numérica, que tiene prioridad |
| SoA en lugar de AoS | Mejoraría la vectorización, pero rompe la correspondencia directa con `U(3,k)` del Fortran, que es lo que permite auditar la migración. Reconsiderable después, con medición |
| Fusionar reconstrucción y Riemann en un bucle | Ahorraría un recorrido de los arrays de cara, a costa de acoplar dos componentes que el diseño separa deliberadamente. Sin medir el beneficio, no compensa |
| `std::variant` en vez de interfaces virtuales | ~5 llamadas indirectas por paso temporal: indetectable |
| Precalcular $A_\pm$ y $V_k$ de la malla radial | Son 4 multiplicaciones y una división por celda, solo en geometría no plana. Cabría en `Mesh1D`, pero primero habría que medir que importa |

## Metodología pendiente

Lo honesto es decir qué falta:

- **No se ha ejecutado un profiler.** No hay atribución de tiempo por función,
  ni conteo de fallos de caché, ni comprobación de si los bucles se están
  vectorizando de verdad (`-fopt-info-vec`).
- **No hay un banco de pruebas de rendimiento en CI.** Las cifras de arriba son
  una medición puntual, no una regresión vigilada.
- **No se ha medido el escalado con hilos**, porque no hay versión paralela.
- **Una sola máquina y un solo compilador.**

El orden de trabajo que fija el proyecto es: corrección → validación → *profiling*
→ optimización. Los dos primeros están hechos y documentados; el tercero es el
siguiente paso, y solo después tiene sentido tocar nada por velocidad.
