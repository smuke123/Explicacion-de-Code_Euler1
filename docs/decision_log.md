# Registro de decisiones arquitectónicas

**Fecha de creación:** 27 de septiembre de 2026

## Para qué existe

Cada decisión técnica de consecuencia se registra aquí una vez, con sus
alternativas y su razón. El objetivo es no volver a discutir lo ya decidido, y
poder responder «por qué hicieron X» en la defensa con una respuesta fechada en
lugar de reconstruida de memoria.

**Regla:** una decisión no registrada no está tomada. Si algo se discute dos
veces, la primera vez no se registró.

Estados posibles: `Aceptada`, `Propuesta`, `Pendiente de aprobación`,
`Sustituida por ADR-xxx`, `Rechazada`.

---

## ADR-001 — Nombre del proyecto y namespace

**Fecha.** 2026-09-27 · **Estado.** `Propuesta` — requiere confirmación del usuario

**Decisión.** Renombrar el proyecto a **FVLab** (*Finite Volume Laboratory*), con
namespace C++ `fvlab`, y renombrar el repositorio de GitHub.

**Alternativas.**

| Alternativa | Por qué no |
|---|---|
| Mantener `Euler1D_Cpp` / `euler1d` | Queda objetivamente incorrecto al entrar el 2D. Un evaluador lo notará |
| `EulerFVM` | Correcto, pero sugiere que el diseño solo admite Euler |
| `CFDLab` | Sobreestima el alcance: no hay flujo viscoso ni turbulencia |

**Razón.** El entregable cubre 1D y 2D. Un namespace `euler1d::` en código
bidimensional es activamente confuso y resta credibilidad al trabajo de
ingeniería.

**Consecuencias.** Renombrado mecánico del namespace en todos los archivos.
Renombrado del repositorio, que GitHub redirige automáticamente. Las rutas de
inclusión cambian de prefijo. Sin efecto numérico: la regresión debe quedar
exactamente igual, y si cambia es señal de que el renombrado tocó lo que no
debía.

---

## ADR-002 — CMake con generador Ninja

**Fecha.** 2026-09-27 · **Estado.** `Aceptada`

**Decisión.** CMake como sistema de construcción; Ninja como generador
preferido.

**Alternativas.** Make simple, como el código de referencia: no es portable a
Windows sin capa de compatibilidad y no gestiona configuraciones múltiples.
Meson: más limpio, pero CMake es el que nombra el anteproyecto (§5.1) y el
estándar de facto en software científico.

**Razón.** Portabilidad real, integración con CTest, y está declarado en el
documento entregado.

**Consecuencias.** Compilación fuera del árbol obligatoria. Dos directorios de
compilación coexistentes para `Debug` y `Release`.

---

## ADR-003 — Aritmética IEEE 754 estricta

**Fecha.** 2026-09-27 · **Estado.** `Aceptada`

**Decisión.** Prohibido `-ffast-math` y cualquier opción que relaje la aritmética
de punto flotante.

**Alternativas.** Activarlo en `Release` por rendimiento.

**Razón.** Rompería el determinismo (NFR-002) y la regresión byte a byte contra el
Fortran, que es el criterio de aceptación central del proyecto. La ganancia de
velocidad es irrelevante para simulaciones de validación.

**Consecuencias.** Se renuncia a vectorización agresiva que dependa de
reasociación. Se registra explícitamente porque es una opción tentadora y su
efecto sería difícil de diagnosticar después.

**Relacionado.** `INV-002`.

---

## ADR-004 — Arnés de pruebas

**Fecha.** 2026-09-27 · **Estado.** `Pendiente de aprobación` — punto abierto `C-2`

**Decisión propuesta.** Catch2 v2 incorporado al repositorio como cabecera única,
con CTest como ejecutor.

**Alternativas.**

| Alternativa | Cumple §5.2 | Cumple NFR-005 |
|---|---|---|
| Arnés propio (actual) | No | Sí |
| GoogleTest vía descarga | Sí | **No**: requiere red |
| Catch2 v3 vía descarga | Sí | **No**: requiere red |
| **Catch2 v2 incorporado** | **Sí** | **Sí** |

**Razón.** Es la única opción que satisface simultáneamente el anteproyecto (que
nombra Catch2 o GoogleTest) y NFR-005 (sin dependencias de red). Catch2 v2 se
distribuye como un único archivo versionable.

**Consecuencias.** El repositorio crece en un archivo grande. Migrar 87 casos,
en su mayor parte mecánicamente. La compilación de los tests se hace más lenta.
CTest no cambia, lo que acota el riesgo a la forma de escribir aserciones.

---

## ADR-005 — Integración continua en GitHub Actions

**Fecha.** 2026-09-27 · **Estado.** `Propuesta`

**Decisión.** Matriz de cinco trabajos: GCC Release, GCC Debug, Clang Release,
regresión con `gfortran`, y determinismo.

**Razón.** Sin CI, cuatro requisitos no funcionales —portabilidad, ausencia de
advertencias, determinismo y regresión— son promesas en lugar de hechos
verificados.

**Consecuencias.** Advertencias como error en CI, no en local: en local estorban
al depurar, en CI son una puerta. El trabajo de regresión con `gfortran` depende
de que la copia del Fortran siga en el repositorio, lo que refuerza la decisión
de versionarla.

---

## ADR-006 — Estrategia de ramas troncal

**Fecha.** 2026-09-27 · **Estado.** `Propuesta`

**Decisión.** Troncal con ramas de vida corta. `main` siempre compila y tiene la
suite verde. Etiqueta en cada hito y en cada versión que produzca una figura
publicada.

**Alternativas.** `git-flow`: burocracia para dos autores. Commits directos a
`main` sin ramas: pierde la posibilidad de revisión, que es el control de calidad
principal del equipo.

**Razón.** Proporcional al tamaño del equipo. Las etiquetas por figura publicada
es lo que hace operativa la afirmación de reproducibilidad de §5.4.

---

## ADR-007 — Las herramientas de postproceso siguen en Python

**Fecha.** 2026-09-27 · **Estado.** `Propuesta`

**Decisión.** Las herramientas de análisis y figuras se conservan en Python, en
`tools/`, fuera de la compilación. Se incorporan tres de las seis que existen en
los paquetes del director; las otras tres corresponden a física fuera de alcance.

**Alternativas.** Reescribirlas en C++: matplotlib no tiene equivalente
razonable, y §5.3 asigna explícitamente la visualización a Python y ParaView.

**Razón.** El postproceso no es parte del núcleo computacional. Reescribirlo
sería trabajo sin relación con ningún objetivo.

**Consecuencias.** Ninguna herramienta de Python es requisito para compilar ni
para ejecutar la suite, lo que preserva NFR-005. Las que **calculan métricas** —no
solo dibujan— necesitan sus propias pruebas, porque producen números que van en
la memoria.

---

## ADR-008 — Fábrica como función, no como jerarquía

**Fecha.** 2026-09-27 · **Estado.** `Propuesta`

**Decisión.** Extraer la construcción de componentes a un `ComponentFactory` con
métodos estáticos. Sin jerarquía de clases fábrica y sin registro dinámico.

**Contexto.** El anteproyecto (§4.4) dice que el núcleo instancia las estrategias
«mediante una fábrica de objetos (*factory pattern*)». Lo que el código tiene es
una función con un `switch` sobre enumerados que, funcionalmente, **es** una
fábrica: es el único lugar que conoce los tipos concretos.

**Alternativas.** `AbstractFactory` con subclases: complejidad sin beneficio, no
hay familias de productos que deban variar juntas. Dejarlo como función privada:
funciona, pero no se puede pedir un componente suelto desde un test.

**Razón.** Satisface la letra del anteproyecto, mejora la testabilidad, y no
introduce indirección.

**Consecuencias.** Un tipo nuevo con métodos estáticos. Los tests pueden
construir un componente sin leer configuración ni instanciar una simulación.

---

## ADR-009 — Dimensión como parámetro de tiempo de compilación

**Fecha.** 2026-09-27 · **Estado.** `Propuesta` — sujeta al *spike* del hito H8

**Decisión.** Los contenedores y los barridos se parametrizan por dimensión. Los
solvers de Riemann se escriben **una sola vez**, operando sobre un estado en el
marco de la cara con velocidad normal explícita y \(N_T\) velocidades
tangenciales pasivas (\(N_T = 0\) en 1D, \(N_T = 1\) en 2D).

**Alternativas.**

| Alternativa | Por qué no |
|---|---|
| **A.** Dos árboles independientes, 1D y 2D | Duplica el álgebra de los tres solvers. **Y debilita la afirmación central de la tesis**: «añadir HLLE» pasaría a ser escribirlo dos veces |
| **C.** Dimensión en tiempo de ejecución | Ramas en los bucles calientes; el 1D pagaría la indexación del 2D. Viola NFR-007. Descartada sin más análisis |

**Razón.** La afirmación que la tesis debe demostrar es que un componente se
intercambia sin reingeniería. Si cada solver existe dos veces, la afirmación no se
sostiene. Una fuente de verdad única para la numérica es, por tanto, un requisito
del argumento, no una preferencia estética. El coste en ejecución es cero:
`std::array<double, 0>` está vacío y los bucles de longitud cero desaparecen en
compilación.

**Consecuencias.** Plantillas en las cabeceras, compilación más lenta, y un salto
de dificultad de lectura que hay que compensar con documentación. La rotación de
la dirección y vive en el barrido y no en el solver, lo que la localiza en un
sitio y permite probarla por separado.

**Mitigación del riesgo.** El *spike* de H8 plantilla el código instanciando solo
con \(N_T = 0\), sin escribir nada de 2D, y exige que la regresión quede
inalterada. Si falla, se adopta la alternativa A y se documenta como deuda
técnica consciente.

---

## ADR-010 — Sin dependencias externas obligatorias

**Fecha.** 2026-09-27 · **Estado.** `Aceptada`

**Decisión.** Compilar y ejecutar el solver y su suite no requiere descargar
nada más que un compilador y CMake. Se incluye el analizador de JSON, que se
escribe a mano en lugar de usar una biblioteca.

**Alternativas.** Usar `nlohmann/json`, que es excelente: añadiría una
dependencia de red o un submódulo para analizar un esquema plano y conocido.

**Razón.** Elimina una clase entera de problemas de reproducibilidad, que es uno
de los cuatro problemas que la justificación de la tesis identifica
explícitamente. Y reduce la barrera de adopción, que es un objetivo pedagógico.

**Consecuencias.** Hay que escribir y probar un analizador de JSON de unos
cientos de líneas, incluidas entradas mal formadas. El coste es acotado porque el
esquema es fijo.

---

## ADR-011 — Dos formatos de configuración y dos de salida

**Fecha.** 2026-09-27 · **Estado.** `Pendiente de aprobación` — puntos abiertos `C-3`, `C-4`

**Decisión.** JSON como formato principal de configuración y CSV/VTK como salida
de usuario, **conservando** el lector de namelist y el escritor `.xl`/`.xyl` de
compatibilidad. Ambos caminos convergen en el mismo objeto de configuración
validado.

**Alternativas.** Solo JSON y CSV: cumpliría el anteproyecto pero **perdería la
capacidad de ejecutar los `input.par` del director y de comparar byte a byte con
sus salidas**, que es lo que sostiene toda la estrategia de validación. Solo
namelist y `.xl`: contradice §4.4 y §5.3.

**Razón.** No es duplicación gratuita. Es la demostración práctica del
desacoplamiento que defiende el `Obj-1`, aplicada al caso que lo necesita, y
preserva indefinidamente la capacidad de regresión.

**Consecuencias.** Dos lectores y dos escritores que hay que mantener. Una prueba
de equivalencia entre formatos. El camino de compatibilidad es el que usa la
suite de regresión, así que se ejercita constantemente y no se puede podrir sin
que se note.

---

## ADR-012 — `Mesh` como contrato de compilación, no clase virtual

**Fecha.** 2026-09-27 · **Estado.** `Propuesta` · `PENDING INPUT FROM DIRECTOR`

**Decisión.** De las cuatro «interfaces abstractas fundamentales» que enumera el
anteproyecto, tres se implementan como clases base virtuales
(`RiemannSolver`, `Reconstruction`, `TimeIntegrator`) y **`Mesh` se implementa
como contrato verificado en tiempo de compilación**, satisfecho por `Mesh1D` y
`Mesh2D`.

**Alternativas.** Clase base virtual `Mesh`: la malla se consulta dentro de los
bucles celda a celda, de modo que una llamada virtual ahí violaría NFR-007 por
una abstracción que no resuelve nada. No hay ni habrá despacho dinámico sobre el
tipo de malla, porque la dimensión se conoce en compilación.

**Razón.** El propósito de una interfaz es permitir sustitución en tiempo de
ejecución. Para la malla no existe ese requisito, y el coste caería justo en el
punto más caliente del código.

**Consecuencias.** `Mesh` aparece como interfaz en el diagrama de componentes y
su contrato se documenta con el mismo rigor que los otros tres, pero no genera
tabla virtual. En C++17 la verificación se hace con `static_assert` en lugar de
con un *concept*.

**Por qué requiere consulta.** El anteproyecto dice «clases abstractas». Es una
divergencia justificada, pero conviene tenerla acordada por escrito antes de la
defensa y no durante.

---

## ADR-013 — Separación física de datos de referencia y líneas base

**Fecha.** 2026-09-27 · **Estado.** `Aceptada`

**Decisión.** `tests/reference/` contiene **exclusivamente** salidas del solver
Fortran. Las líneas base producidas por el C++ van en `tests/baseline/`, y nunca
se llaman «referencia».

**Alternativas.** Un único directorio con convención de nombres: es
precisamente cómo ocurre el fallo que esto previene.

**Razón.** Evitar que el C++ acabe validándose contra su propia salida. El fallo
es fácil de cometer, silencioso, e invalidaría la estrategia de validación
completa: alguien regenera «la referencia» con el código que quería validar, y a
partir de ahí todo pasa siempre.

**Consecuencias.** Dos directorios con políticas distintas, documentadas en
`validation_strategy.md` Parte II. Salvaguarda automática propuesta: el test de
regresión aborta si encuentra en `tests/reference/` un archivo que solo el C++
produce.

---

## Decisiones ya tomadas en la fase de migración

Estas se tomaron durante la migración del 1D y se registran aquí para que queden
en un solo lugar. Todas están en estado `Aceptada` y verificadas por la
regresión.

| ID | Decisión | Razón resumida |
|---|---|---|
| ADR-014 | Índices de celda de \(-1\) a \(N+2\), convención Fortran | Permite auditar la migración línea a línea, que es el criterio de aceptación |
| ADR-015 | Disposición de memoria por celda (componentes contiguas) | Preserva el patrón de acceso del original, que es *column-major* sobre `U(3,k)` |
| ADR-016 | Strategy con granularidad de barrido, no de celda | Una llamada virtual por etapa en lugar de una por celda y componente |
| ADR-017 | Limitadores como tipos-política, no como interfaz | Son funciones puras sin estado; una interfaz costaría una indirección por celda |
| ADR-018 | Geometría como operador de divergencia, no como término fuente | La geometría cambia la propia divergencia; no es un término aditivo |
| ADR-019 | Estados conservado y primitivo coexisten | El esquema reconstruye sobre primitivas; recalcularlas costaría una raíz y una división por celda |
| ADR-020 | Sin `Factory` con jerarquía, sin `Observer`, sin `Singleton` | Ver `architecture.md` §12.7 |
| ADR-021 | Datos de referencia marcados como binarios en `.gitattributes` | Se comparan byte a byte; cualquier conversión de fin de línea invalidaría la validación |
| ADR-022 | El solver exacto no participa en el avance temporal | Si participara, la comparación contra la solución exacta sería circular |

---

## Decisiones del prototipo GUI (2026-09-28)

Registradas para no reabrirlas durante la semana del MVP. Detalle en `docs/mvp_plan.md`.

| ID | Decisión | Estado |
|---|---|---|
| MVP-01 | GUI = Tkinter + matplotlib, no Qt/Electron | Aceptada |
| MVP-02 | El núcleo se invoca por proceso (`xEuler1D`), no por pybind11 | Aceptada |
| MVP-03 | Contrato GUI ↔ núcleo = namelist `&euler` + archivos `.xl` | Aceptada |
| MVP-04 | Cero cambios en fórmulas numéricas para integrar la GUI | Aceptada |
| MVP-05 | Catálogo pedagógico en Python; fragmentos leídos del `.hpp` real | Aceptada |
| MVP-06 | Comparación = dos corridas en memoria, no un segundo solver | Aceptada |
