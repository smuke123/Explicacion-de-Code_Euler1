# Plan de implementación del prototipo — FVLab Studio

**Fecha:** 28 de septiembre de 2026
**Entrega:** sábado 3 de octubre de 2026
**Material final del director:** esperado el viernes 2 de octubre
**Naturaleza:** prototipo demostrable, **no** versión final de la tesis

---

# A. Análisis del repositorio actual

## A.1 Lo que existe y está validado

`CONFIRMED` por inspección.

Núcleo numérico C++ de Euler 1D, completo y verificado: **26 de 26**
configuraciones dentro de tolerancia frente al Fortran compilado, con el número
de pasos temporales coincidiendo exactamente; 9 de 13 casos de regresión
idénticos byte a byte; 87 casos de prueba en 6 ejecutables bajo CTest, verdes en
Debug y Release; compilación sin advertencias con el conjunto completo.

| Componente | Estado | Reutilizable tal cual |
|---|---|---|
| `SimulationConfig` + validación | ✅ | Sí |
| Analizador de namelist | ✅ | Sí |
| `TestCaseDefinition` (8 casos) | ✅ | Sí |
| `Mesh1D` | ✅ | Sí |
| `CellField<T>`, estados, `IdealGas` | ✅ | Sí |
| `MusclReconstruction` + 5 limitadores | ✅ | Sí |
| `HllRiemannSolver`, `HllcRiemannSolver` | ✅ | Sí |
| 3 condiciones de frontera | ✅ | Sí |
| `SspRk2`, `ForwardEuler` | ✅ | Sí |
| `CflController` | ✅ | Sí |
| `GravitySource`, divergencia cartesiana y radial | ✅ | Sí |
| `ExactRiemannSolver` | ✅ | Sí — **clave para el prototipo** |
| `ErrorAnalyzer` (max, L1, L2, relativo) | ✅ | Sí — **clave** |
| `OutputManager`, `xl_reader`, `fortran_format` | ✅ | Sí |
| `Simulation` (orquestador) | ✅ | Sí, con una extensión menor |

## A.2 Hallazgo nuevo: los otros dos repositorios

| Repositorio | Contenido | Relevancia para el prototipo |
|---|---|---|
| `Explicacion-de-Code_Euler1` | Este proyecto. Euler 1D C++ + Fortran de referencia | **Es la base** |
| `CFDC-` → `wave1d-cpp/` | **Segunda migración C++ propia**: ecuación de onda 1D, con el mismo patrón arquitectónico (`config/`, `boundary/`, `time_integrators/`, `analysis/`, `io/`, `solver/`) y el mismo conjunto de documentos | Aporta **RK3, RK4 y RK-Fehlberg 4**. Ver F.2 |
| `MinkowskiEcuationSolver` | El Fortran original de la ecuación de onda | Referencia histórica. No se usa |

Dos consecuencias para el plan:

**El patrón arquitectónico está repetido y validado en dos sistemas físicos
distintos.** Eso es un argumento fuerte para la tesis y conviene que el prototipo
lo haga visible, aunque la ecuación de onda quede fuera del alcance.

**`wave1d-cpp` tiene tres integradores temporales que el código de Euler no
tiene.** Su interfaz no es directamente compatible —`step(state, grid, bc,
order)` frente a nuestro `advance(state, dt, op)`— pero los **coeficientes** de
RK3 y RK4 se transcriben en minutos. Ver F.2: es la vía más barata de darle al
usuario un eje de comparación más.

## A.3 Lo que NO existe

| Falta | Impacto en el prototipo |
|---|---|
| Cualquier interfaz gráfica | Es el objeto de este sprint |
| Solver **HLLE** | **Crítico**: es la pregunta de investigación. Y son ~80 líneas |
| Casos **Sod clásico** y **Lax** | Los ejemplos del prototipo los necesitan |
| Serialización a JSON | Necesaria para que el GUI reciba datos |
| Cualquier cosa 2D | **Fuera del prototipo.** Ver N |
| WENO | **Fuera del prototipo.** Ver N |
| `FirstOrderReconstruction` como módulo | Funciona como opción de limitador. Basta para el prototipo |

## A.4 Limitaciones conocidas que se preservan

`docs/inconsistencies.md` cataloga 17 peculiaridades del Fortran original. **El
prototipo no corrige ninguna.** Se muestran al usuario como parte del material
educativo, que es precisamente el tipo de transparencia que la tesis defiende.

---

# B. Componentes que se reutilizan sin tocar

**Regla dura de este sprint:** el núcleo numérico no se modifica para acomodar el
GUI. Ni una fórmula, ni un bucle, ni una firma de las existentes.

Todo lo listado en A.1 se reutiliza tal cual. El GUI enlaza contra la biblioteca
`euler1d_core` exactamente igual que lo hace hoy el ejecutable de línea de
comandos.

**Verificación de que esta regla se cumplió:** al final del sprint, la suite de
regresión debe seguir dando 26 de 26 con el número de pasos idéntico. Si cambia
algo, el GUI tocó lo que no debía.

---

# C. Componentes que se modifican

Cambios mínimos y aditivos. Ninguno altera comportamiento numérico existente.

| Componente | Cambio | Riesgo |
|---|---|---|
| `Simulation` | Añadir un método que devuelva los campos **en memoria** en lugar de solo escribirlos a disco. El bucle temporal no cambia | Bajo: es un accesor |
| `SimulationConfig` | Añadir el valor `HLLE` al enumerado y sus conversiones de cadena | Nulo: es aditivo |
| `TestCaseDefinition` | Dos entradas nuevas (Sod clásico, Lax) y subir `kTestCaseCount` a 10 | Nulo: aditivo, y el compilador avisa si se hace a medias |
| `CMakeLists.txt` | Objetivo nuevo `fvlab-studio`. Bajar a C++17 (verificado: no se usa nada exclusivo de C++20) | Bajo |

**`Simulation` es el único cambio con contenido.** Hoy el orquestador escribe a
disco y devuelve un informe. El GUI necesita los arreglos. La extensión es un
accesor `const` a los campos ya existentes; **no** se duplica estado ni se copia
nada en el bucle.

---

# D. Componentes nuevos

Todo lo nuevo está **fuera** del núcleo numérico, salvo HLLE y los dos casos.

| Nuevo | Qué es | Líneas aprox. |
|---|---|---|
| `HlleRiemannSolver` | Promedios de Roe + velocidades de Einfeldt | 80 |
| Casos Sod clásico y Lax | Dos entradas declarativas | 10 |
| `http/server.cpp` | Servidor HTTP mínimo sobre sockets | 300 |
| `http/json.cpp` | Escritor JSON y analizador mínimo para las peticiones | 220 |
| `http/api_schema.cpp` | Genera el esquema de opciones **desde los enumerados de C++** | 130 |
| `http/api_run.cpp` | Ejecuta una o varias configuraciones y serializa | 180 |
| `http/api_source.cpp` | Sirve los archivos fuente **reales** del repositorio | 70 |
| `web/index.html` + `style.css` | Interfaz | 700 |
| `web/app.js` | Controlador: panel, ejecución, estado | 520 |
| `web/plot.js` | Gráficas en Canvas2D, con zoom y desplazamiento | 380 |
| `web/content/*.json` | **16 módulos educativos** | 1500 (prosa) |

Total aproximado: **4100 líneas**, de las que ~1500 son contenido educativo en
prosa y no código.

---

# E. Arquitectura del GUI

## E.1 Decisión de tecnología

`DECIDED` — `ADR-023`

> **Servidor HTTP local escrito en C++ sobre sockets, con interfaz en el
> navegador. Cero dependencias de terceros, ni de C++ ni de JavaScript.**

### Alternativas evaluadas

| Opción | A favor | En contra | Veredicto |
|---|---|---|---|
| **Dear ImGui + ImPlot** | Nativo C++, enlace directo, ImPlot es excelente para gráficas científicas, un solo ejecutable | Renderizar ecuaciones matemáticas es muy costoso (haría falta pre-renderizarlas como imágenes); resaltado de sintaxis manual; **no** encaja con «entorno de servidor»; hay que vendorizar ~30 000 líneas | Descartada por el requisito educativo |
| **Qt + QtCharts** | Maduro, aspecto nativo | Dependencia enorme, instalación pesada en Windows, dudas de licencia, demasiado para 5 días | Descartada |
| **Python (PySide / Streamlit) + enlace al C++** | Desarrollo rapidísimo, matplotlib ya se usa | Obliga al usuario final a tener Python en Windows; añade una frontera de lenguaje en el camino caliente | Descartada por fricción de instalación |
| **Servidor HTTP en C++ + navegador** | El navegador ya está en toda máquina Windows; ecuaciones y código con resaltado son triviales en HTML; **es literalmente un servidor**, lo que resuelve el requisito de migración; el núcleo queda intacto detrás de una API | Hay que escribir el servidor (~300 líneas); dos lenguajes | **Elegida** |

### Por qué cero dependencias

Tres razones concretas, en orden de peso:

1. **La demo tiene que funcionar sin red.** Cualquier CDN convierte una
   presentación en una apuesta. Y vendorizar KaTeX o Plotly son megabytes de
   blobs minificados en el repositorio de una tesis.
2. **Coherencia con `ADR-010`**, que ya está registrada: compilar y ejecutar no
   requiere descargar nada. Es uno de los cuatro problemas que la justificación
   de la tesis identifica.
3. **El coste real es bajo.** Las ecuaciones se renderizan con HTML, CSS y
   Unicode (ρ, ∂, √, ∇, subíndices), que para las fórmulas de este dominio da un
   resultado limpio. Las gráficas de perfiles 1D en Canvas2D son ~380 líneas y
   dan control total.

**Consecuencia que se acepta:** sin KaTeX, las ecuaciones más complejas se ven
algo menos elegantes. Se compensa con CSS cuidado. Está en la lista de mejoras
futuras (N).

## E.2 Estructura

```
┌─────────────────────────────────────────────────┐
│  NAVEGADOR  (HTML + CSS + JS, sin dependencias) │
│  ┌──────────┬───────────────┬────────────────┐  │
│  │ Panel de │  Gráficas     │  Panel         │  │
│  │ control  │  (Canvas2D)   │  educativo     │  │
│  │          │               │  + código real │  │
│  └──────────┴───────────────┴────────────────┘  │
└────────────────────┬────────────────────────────┘
                     │  HTTP  localhost:8080
                     │  JSON
┌────────────────────▼────────────────────────────┐
│  fvlab-studio.exe                               │
│  ┌───────────────────────────────────────────┐  │
│  │  Capa HTTP  (servidor + rutas + JSON)     │  │
│  │  NO CONTIENE NINGÚN ALGORITMO NUMÉRICO    │  │
│  └────────────────────┬──────────────────────┘  │
│  ┌────────────────────▼──────────────────────┐  │
│  │  euler1d_core   (biblioteca, SIN TOCAR)   │  │
│  │  Simulation · Config · Mesh · Recon ·     │  │
│  │  Riemann · Time · BC · Source · Exact     │  │
│  └───────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
          ▲
          │ la misma biblioteca
┌─────────┴──────────┐
│  xEuler1D.exe      │  línea de comandos, sigue funcionando igual
└────────────────────┘
```

Esto satisface literalmente las secciones 13 y 18 del encargo: el GUI no contiene
algoritmos numéricos, y el núcleo sigue siendo utilizable sin GUI.

## E.3 API

| Ruta | Qué hace |
|---|---|
| `GET /` | La aplicación |
| `GET /api/schema` | Opciones disponibles y rangos válidos, **generados desde los enumerados de C++** |
| `POST /api/run` | Ejecuta **una o varias** configuraciones y devuelve campos, referencia, métricas de error e informe |
| `GET /api/source?id=hllc` | Devuelve el **archivo fuente real** del repositorio |
| `GET /api/content?id=hllc` | Módulo educativo |

Dos de estas rutas son decisiones de diseño, no fontanería:

**`/api/schema` se genera desde los enumerados de C++.** El GUI no puede ofrecer
una opción que el núcleo no tenga, ni quedarse desactualizado. Una fuente de
verdad única.

**`/api/source` sirve el archivo real, leído del disco.** Es la respuesta directa
al requisito 7: *«Avoid creating educational pseudo-code that differs
substantially from the real implementation»*. El código que el usuario ve **es**
el código que se ejecutó, no una transcripción.

---

# F. Flujo del usuario

## F.1 El recorrido principal

```
1. Ejecuta fvlab-studio.exe  →  se abre el navegador
2. Elige un ejemplo (Toro 1, Sod, Lax, 123, doble choque…)
   →  lee qué problema es, qué esperar y qué observar
3. Lee el panel educativo del método seleccionado
   →  explicación · ecuaciones · algoritmo · CÓDIGO REAL · parámetros
4. Ajusta parámetros (N, CFL, γ, estados, solver, limitador…)
5. Ejecutar  →  ~30 ms
6. Ve densidad, velocidad, presión y energía, con la solución exacta superpuesta
7. Lee las métricas: max|diff|, L1, L2
8. Cambia el solver de HLLC a HLL
9. Ejecutar de nuevo
10. Modo comparación: las dos curvas superpuestas, diferencia cuantificada
11. Entiende por qué el contacto se difumina con dos ondas y no con tres
```

Los pasos 8 a 11 son la demostración central de la tesis y es donde hay que
poner el esfuerzo de pulido.

## F.2 Ejes de comparación que se ofrecen

| Eje | Opciones | Origen |
|---|---|---|
| Solver de Riemann | HLL · **HLLE** · HLLC | HLLE es nuevo (D) |
| Limitador | fo · minmod · van Leer · MC · superbee | Ya existe |
| Resolución | libre, 50 a 3200 | Ya existe |
| CFL | libre, 0 a 1 | Ya existe |
| Integrador temporal | Euler · SSP-RK2 | Ya existe |
| Caso de prueba | 10 casos | 8 existen, 2 nuevos |
| Frontera | transmisiva · periódica · reflectiva | Ya existe |

`SHOULD HAVE` — añadir **RK3 y RK4** transcribiendo los coeficientes de
`wave1d-cpp` (A.2). Da un eje de comparación más por muy poco trabajo, y hace
visible que el patrón arquitectónico se repite entre dos proyectos.

---

# G. Sistema educativo

## G.1 Estructura de un módulo

Cada concepto es un archivo de datos con seis partes fijas:

```
{
  "id": "hllc",
  "titulo": "Solver HLLC",
  "que_es":          "…qué es y qué problema resuelve",
  "por_que":         "…por qué se usa, qué gana frente a la alternativa",
  "ecuaciones":      [ { "latex_unicode": "…", "descripcion": "…" } ],
  "interpretacion":  "…significado físico",
  "supuestos":       [ "…" ],
  "algoritmo":       [ "paso 1", "paso 2", … ],
  "parametros":      [ "gamma", "cfl" ],
  "donde_encaja":    "flujo numérico, etapa 3 de 5 del operador espacial",
  "fuente":          { "archivo": "include/euler1d/numerical/riemann/hllc.hpp",
                       "funcion": "hllcFlux" },
  "experimento":     { "descripcion": "…", "config_a": {…}, "config_b": {…},
                       "que_observar": "…" }
}
```

El campo `fuente` es el que da la transparencia: el panel llama a
`/api/source` y muestra el archivo real. El campo `experimento` es el que
convierte la explicación en acción: un botón que carga las dos configuraciones y
las ejecuta.

## G.2 Los 16 módulos

| # | Módulo | Prioridad |
|---|---|---|
| 1 | Ecuaciones de Euler | MUST |
| 2 | Variables conservadas y primitivas | MUST |
| 3 | Método de volúmenes finitos | MUST |
| 4 | Flujo numérico en la cara | MUST |
| 5 | El problema de Riemann | MUST |
| 6 | Solución exacta de Riemann | MUST |
| 7 | Solver HLL | MUST |
| 8 | Solver HLLE | MUST |
| 9 | Solver HLLC | MUST |
| 10 | Reconstrucción MUSCL | MUST |
| 11 | Limitadores de pendiente | MUST |
| 12 | Condición CFL y paso adaptativo | MUST |
| 13 | Integración temporal SSP-RK2 | MUST |
| 14 | Condiciones de frontera y celdas fantasma | MUST |
| 15 | Términos fuente y geometría | SHOULD |
| 16 | Órdenes de convergencia | SHOULD |

## G.3 «Qué está haciendo el solver»

`MUST HAVE`. Un diagrama del operador espacial en el que **se resalta la etapa
correspondiente al módulo abierto**:

```
W  →  [1 Reconstrucción]  →  qL,qR  →  [2 Pisos]  →
      [3 Solver de Riemann]  →  F  →  [4 Divergencia]  →
      [5 Términos fuente]  →  Uout
```

Es la pieza que convierte un conjunto de explicaciones en una respuesta a la
pregunta «¿qué está haciendo este solver?». Cuesta poco y comunica mucho.

---

# H. Visualización

`MUST HAVE`:

- Densidad, velocidad, presión y energía específica interna, seleccionables.
- Solución numérica (puntos) contra solución exacta (línea) superpuestas.
- Métricas de error: max|diff|, L1, L2 y error relativo.
- Modo comparación: dos configuraciones superpuestas, con leyenda.
- Zoom y desplazamiento con rueda y arrastre.
- Cursor con lectura de valores.

`SHOULD HAVE`:

- Curva de diferencia numérica menos exacta.
- Deslizador temporal cuando hay varios volcados.
- Exportar la gráfica a PNG (`canvas.toDataURL`, tres líneas).

`NICE TO HAVE`:

- Gráfica log-log de convergencia con el orden observado ajustado.
- Flujos numéricos en las caras.

**Separación explícita entre simulación y visualización** (requisito 12): el
núcleo calcula y devuelve arreglos; el navegador dibuja. No hay ninguna operación
de interfaz dentro del bucle del solver.

---

# I. Estrategia de pruebas

**Puerta no negociable en cada etapa:** la suite existente sigue verde y la
regresión sigue dando 26 de 26 con los pasos idénticos. Es lo que demuestra que
el GUI no tocó el núcleo.

| Etapa | Qué se prueba, y cómo |
|---|---|
| 0 | La suite existente sigue verde tras el cambio de CMake y a C++17 |
| 1 | Pruebas unitarias del escritor y analizador JSON; ida y vuelta de configuración |
| 1 | Las rutas responden: comprobación manual con `curl` documentada en el README |
| 2 | Ninguna nueva: es interfaz. Verificación visual |
| 3 | **HLLE: las cuatro propiedades de solver de Riemann** de `validation_strategy.md`, más convergencia medida antes de usarlo en cualquier comparación |
| 3 | Sod y Lax: valores declarados, y error contra la solución exacta registrado |
| 4 | Que todo `fuente.archivo` de los 16 módulos exista y sea legible. Automatizable, y evita enlaces muertos en la demo |
| 5 | Que todo `experimento` de los módulos produzca configuraciones válidas |

Sobre HLLE hay una regla que **no se relaja por ser un prototipo**: se verifica su
orden de convergencia antes de mostrarlo en una comparación. Enseñar una
comparación entre un método verificado y uno sin verificar sería exactamente el
error que la estrategia de validación previene.

---

# J. Compilación y ejecución en Windows

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cd <repo>
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
.\build\fvlab-studio.exe
```

El ejecutable abre el navegador por defecto en `http://localhost:8080` y sirve
`web/` desde el disco.

Decisiones para que la demo no falle:

| Decisión | Razón |
|---|---|
| Puerto configurable con `--port`, con reintento automático | El 8080 puede estar ocupado en la máquina de la demo |
| Escucha solo en `127.0.0.1` | No expone nada a la red. Sin autenticación, que no hace falta |
| `web/` se sirve desde disco, no se empotra | Permite editar la interfaz sin recompilar durante el sprint |
| Sin TLS, sin sesiones, sin dependencias | Es una herramienta local |
| **Ensayo completo el sábado por la mañana** | Una demo no ensayada es una demo que falla |

`SHOULD HAVE` — opción `--embed` que empotre `web/` en el binario, para poder
repartir un único `.exe`. Es un guion de conversión a cabecera, no un cambio de
arquitectura.

---

# K. Migración futura al entorno de servidor

La elección de E.1 hace que esto sea casi gratis, y era la razón principal para
elegirla.

| Escenario | Qué hay que cambiar |
|---|---|
| Servidor Linux | Recompilar. El código de sockets tiene ~30 líneas entre `#ifdef` |
| Multiusuario | Cambiar `127.0.0.1` por `0.0.0.0`, añadir un proxy inverso delante y una cola de trabajos. La API no cambia |
| Contenedor | Un `Dockerfile` de diez líneas. Sin dependencias que resolver |
| Otra interfaz (escritorio, cuaderno) | Consume la misma API HTTP. El núcleo y la capa HTTP no se tocan |
| Línea de comandos | **Ya funciona.** `xEuler1D` sigue siendo un ejecutable independiente |

El riesgo real que queda es que las simulaciones 2D del futuro tarden demasiado
para una petición sincrónica. La solución —una cola de trabajos con consulta de
estado— es un cambio en la capa HTTP, no en el núcleo ni en la interfaz.

---

# L. Priorización

## MUST HAVE — sin esto no hay demo

1. Servidor HTTP con las cinco rutas.
2. Panel de parámetros generado desde el esquema del núcleo.
3. Ejecutar y graficar las cuatro variables con la solución exacta superpuesta.
4. Métricas de error en pantalla.
5. **Solver HLLE**, con su convergencia verificada.
6. Modo comparación de dos configuraciones.
7. Los 14 módulos educativos marcados MUST, con **código fuente real**.
8. Diagrama de «qué está haciendo el solver».
9. Cinco ejemplos predefinidos con descripción y experimento sugerido.
10. Casos Sod clásico y Lax.
11. Compilación y ejecución verificadas en Windows.
12. **La suite y la regresión siguen verdes.**

## SHOULD HAVE

13. RK3 y RK4 transcritos de `wave1d-cpp`.
14. Módulos educativos 15 y 16.
15. Modo de estudio de convergencia con orden observado.
16. Curva de diferencia, deslizador temporal, exportar PNG.
17. Lector de configuración JSON (cierra parte de `C-3`).
18. Salida CSV (cierra parte de `C-4`).

## NICE TO HAVE

19. Opción `--embed` para un único ejecutable.
20. Resaltado de sintaxis de C++ en el visor de código.
21. Guardar y cargar sesiones de experimento.
22. Gráfica log-log de convergencia.

## FUTURE — explícitamente fuera del sprint

23. Todo lo 2D.
24. WENO.
25. Migración a Catch2.
26. Renombrado a `fvlab` (`ADR-001`): **se aplaza a propósito.** Renombrar el
    namespace durante un sprint de cinco días consume tiempo y pone en riesgo la
    regresión, a cambio de cero valor para la demo. El nombre del producto en la
    interfaz será «FVLab Studio»; el namespace sigue siendo `euler1d`. Es una
    incoherencia consciente y documentada.
27. Sistema de colas para el servidor multiusuario.
28. Ecuación de onda como segundo sistema físico (de `CFDC-`).

---

# M. Etapas y calendario

Viernes queda deliberadamente libre como colchón, porque es cuando llega el
material del director.

| Etapa | Cuándo | Contenido | Se puede demostrar al final |
|---|---|---|---|
| **0** | lunes noche | Objetivo CMake `fvlab-studio`; C++17; servidor sirviendo una página estática. Suite verde | El servidor arranca |
| **1** | martes | JSON, `/api/schema`, `/api/run`, `/api/source`. Verificado con `curl` | El núcleo responde por HTTP |
| **2** | martes–miércoles | Interfaz: panel, ejecución, gráficas, selector de variable, métricas | **Primera demo completa** |
| **3** | miércoles | HLLE + convergencia; Sod y Lax; modo comparación | **La demo central de la tesis** |
| **4** | jueves | Sistema educativo: 14 módulos, visor de código real, diagrama del operador | La transparencia funciona |
| **5** | jueves–viernes | Ejemplos con descripción; SHOULD HAVE según tiempo | Aplicación completa |
| **6** | viernes | Colchón: material del director, ajustes, README de la demo | — |
| **7** | sábado mañana | **Ensayo completo de la demo** y corrección de lo que falle | Entrega |

**Punto de control del miércoles por la noche:** si la etapa 3 no está cerrada,
se recortan los módulos educativos de 14 a 8 (los del 1 al 9 sin HLL/HLLE
separados) antes de tocar cualquier MUST HAVE.

---

# N. Prototipo frente a producto final

## PROTOTYPE IMPLEMENTED

Todo lo de MUST HAVE. Núcleo numérico intacto y validado. HLLE verificado. GUI
funcional con las tres áreas. 14 módulos educativos apuntando al código real.

## PROTOTYPE SIMPLIFIED — atajos conscientes

| Atajo | Por qué | Coste de deshacerlo |
|---|---|---|
| Ecuaciones en HTML/Unicode, no LaTeX | Sin dependencias, funciona sin red | Bajo: incorporar KaTeX |
| Gráficas propias en Canvas2D | Sin dependencias, control total | Bajo: cambiar a una biblioteca si hace falta |
| Servidor de un solo hilo, una petición a la vez | Basta para un usuario; una corrida 1D son ~30 ms | Medio: cola de trabajos |
| `web/` servido desde disco | Permite iterar sin recompilar | Trivial: `--embed` |
| Namespace sigue siendo `euler1d` | No gastar el sprint en un renombrado | Bajo, pero hay que hacerlo |
| Orden 1 como opción de limitador | Funciona numéricamente | Bajo: extraer la clase |
| Sin persistencia de sesiones | No hace falta para demostrar la idea | Bajo |
| Analizador JSON mínimo, no conforme al estándar completo | Solo consume nuestro propio esquema | Medio si se abre a terceros |

## FUTURE IMPLEMENTATION

2D completo · WENO · Catch2 · renombrado · CI · colas · empotrado · segundo
sistema físico · matriz de comparación formal para la memoria.

## PENDING DIRECTOR INPUT

`BLOCKED — REQUIRES EXTERNAL INFORMATION`, y **ninguno bloquea el prototipo**:

| Pendiente | Qué bloquea |
|---|---|
| Material del viernes | Nada del prototipo. Colchón previsto en la etapa 6 |
| `A-2` — ¿WENO es entregable? | Solo trabajo posterior al sprint |
| `B-4` — alcance de la matriz de comparación | La versión formal para la memoria, no la demo |
| `C-2` — Catch2 | Fuera del sprint |
| `C-3`, `C-4` — JSON y CSV | Son SHOULD HAVE. El GUI habla JSON por HTTP de todos modos |
| Estilo de figuras | Cosmético |
| NFR-015 — objetivo de rendimiento | Se mide lo que salga |

Los doce puntos abiertos de `alcance.md` siguen abiertos y **no detienen nada de
este sprint**.

---

# O. Decisión nueva registrada

## ADR-023 — Tecnología del GUI del prototipo

**Fecha.** 2026-09-28 · **Estado.** `Aceptada` para el prototipo

**Decisión.** Servidor HTTP local en C++ sobre sockets, interfaz en el
navegador, cero dependencias de terceros en ambos lados.

**Alternativas.** Ver E.1.

**Razón.** Es la única opción que satisface simultáneamente los tres requisitos
que más pesan: el sistema educativo con ecuaciones y código fuente (trivial en
HTML, costoso en cualquier interfaz nativa), la portabilidad futura al entorno de
servidor (la arquitectura ya **es** cliente-servidor), y el plazo de cinco días.

**Consecuencias.** Hay que escribir ~300 líneas de servidor y ~380 de graficado
que una biblioteca daría hechas. En cambio: la demo funciona sin red, el
repositorio no engorda con blobs minificados, el núcleo queda intacto detrás de
una API, y la migración al servidor es recompilar.

**Lo que esta decisión NO es.** No es un compromiso de que la versión final use
navegador. Si más adelante se prefiere una aplicación nativa, consume la misma
API y ni el núcleo ni la capa HTTP cambian.
