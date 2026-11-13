# Plan de implementacion del prototipo MVP

**Fecha:** 28 de septiembre de 2026
**Horizonte:** demostrable este sabado
**Estado:** aprobado para ejecucion autonoma (este documento es la hoja de ruta)

Este prototipo **no** es el software final de tesis. Es un laboratorio
interactivo que envuelve el nucleo C++ ya validado, para demostrar la idea
central:

> Aprender → modificar → ejecutar → ver → comparar → entender

---

## A. Analisis del repositorio actual

Ya existe y esta validado:

| Pieza | Donde | Estado |
|---|---|---|
| Nucleo Euler 1D (MUSCL, 5 limitadores, HLL, HLLC, RK2, CFL, fronteras, fuentes) | `include/`, `src/` | Validado vs Fortran (26/26) |
| CLI `xEuler1D` | `src/main.cpp` | Funciona: `-i`, `-o`, `-q` |
| Namelist `&euler` | `src/config/namelist_parser.cpp` | Estricto; claves desconocidas abortan |
| Salida `.xl` + referencia exacta | `OutputManager` | `rho_1.xl`, `vel_1.xl`, `press_1.xl` y `*_exact.xl` |
| Lector `.xl` | `src/io/xl_reader.cpp` | Bloques `#Time` |
| 8 casos de prueba | `src/initial/test_case.cpp` | Toro 1-5, entropia, atmosfera, Sedov |
| Metricas L1/L2 | `ErrorAnalyzer` | Usado por tests y `euler1d_compare` |
| Binarios Release/Debug | `build/xEuler1D.exe`, `build-debug/` | Presentes en esta maquina |
| Suite CTest | `tests/` | 87 casos; no se toca |

No existe hoy: GUI, catalogo pedagogico integrado, comparacion visual, flujo
Learn→Run→See.

Los repositorios `CFDC-` y `MinkowskiEcuationSolver` se citaron como contexto
previo del autor. El prototipo **reutiliza el nucleo local ya validado**, no
reescribe solvers a partir de esos repos.

---

## B. Componentes a reutilizar (sin reescribir)

- `euler1d_core` + `xEuler1D.exe` como motor numerico unico.
- Formato namelist como contrato GUI ↔ nucleo (misma via que el Fortran).
- Formato `.xl` como contrato de visualizacion.
- Definiciones de casos 1–8 (estados, x0, tf, fronteras forzadas).
- Comentarios y formulas ya presentes en `hllc.hpp`, `hll.hpp`,
  `slope_limiter.hpp`, `exact_riemann_solver.hpp`.
- `docs/numerical_method.md` como fuente de texto pedagogico.

La GUI **no** implementa HLL, HLLC, MUSCL, CFL ni RK2.

---

## C. Componentes a adaptar (minimo)

Ningun algoritmo numerico. Solo, si hace falta mas adelante:

- `main.cpp`: flags extra (`--dry-run`) — **FUTURE**, no este sabado.
- Namelist: estados L/R editables — el nucleo **no** los lee del namelist;
  los fija el caso. El prototipo los **muestra** y explica; editar L/R de
  forma arbitraria queda como FUTURE (exigiria un caso 9 o un modo custom).

Shortcut documentado: los estados se ven y se usan para ensenar; cambiar el
caso cambia los estados. No se inventa un segundo inicializador.

---

## D. Componentes a crear

```
tools/fvlab_lab/          laboratorio interactivo (Python)
  app.py                  entrada
  controller.py           escribe namelist, lanza xEuler1D, lee .xl
  xl_io.py                parser .xl (espejo de xl_reader.cpp, no un solver)
  catalog.py              manual interactivo + rutas al codigo real
  examples.py             experimentos guiados
  ui.py                   Tkinter + matplotlib
run_lab.ps1 / run_lab.bat lanzadores Windows
tools/requirements-lab.txt
docs/mvp_plan.md          este plan
```

---

## E. Decision de GUI

**Elegido:** Python 3 + Tkinter (stdlib) + matplotlib.

| Opcion | Por que no / si |
|---|---|
| Qt/C++ nativo | Demasiado para el plazo; acoplaria GUI al nucleo |
| Dear ImGui | Bueno para tools, malo para texto pedagogico largo |
| Electron | Pesado, otra cadena de build |
| Canvas de Cursor | No es una app que el director pueda abrir fuera del chat |
| **Tkinter + matplotlib** | Windows ya tiene Tk; portable a Linux/macOS; matplotlib es el postproceso 1D del anteproyecto |

El nucleo C++ permanece independiente. La GUI es un cliente.

Comunicacion:

```
UI  →  namelist temporal  →  xEuler1D.exe -i … -o … -q
     ←  stdout + .xl      ←  OutputManager (sin cambios)
```

---

## F. Flujo de usuario (MUST HAVE)

1. Abrir el laboratorio.
2. Cargar un ejemplo guiado (p. ej. Toro 1 / contacto Toro 5).
3. Leer que problema es y que observar.
4. Elegir solver / limitador / N / CFL / tf.
5. Abrir la ficha del metodo seleccionado (que es, ecuaciones, archivo real).
6. Ejecutar.
7. Ver densidad / velocidad / presion, numerica vs exacta.
8. Guardar la corrida, cambiar un metodo, ejecutar de nuevo, comparar.

---

## G. Sistema educativo

Fichas en `catalog.py`, no una web aparte. Cada ficha:

- que es, para que sirve, hipotesis;
- ecuaciones (texto Unicode, sin KaTeX en el MVP);
- algoritmo simplificado alineado con el codigo real;
- ruta y fragmento del `.hpp` / `.cpp` verdadero;
- experimento sugerido.

Conceptos MUST HAVE: Euler, FVM, conservadas/primitivas, flujo numerico,
ghost cells, MUSCL, cada limitador, HLL, HLLC, CFL, RK2, fronteras, Riemann
exacto.

Al cambiar solver/limitador, la ficha visible sigue la seleccion.

---

## H. Visualizacion

- Un eje 1D, variable seleccionable: ρ, u, p.
- Curva numerica + referencia exacta si existe.
- Segunda corrida en el mismo eje (comparar HLL vs HLLC).
- Zoom/pan de matplotlib.
- Panel de metricas: pasos, t final, max|diff|, L1 si hay exacta.

No VTK, no 2D, no animacion cuadro a cuadro (NICE TO HAVE).

---

## I. Pruebas de esta fase

- No se toca `euler1d_core`. La suite CTest existente sigue siendo la
  garantia numerica.
- Prueba manual del laboratorio: Toro 1 HLLC vs HLL, N=100, debe plotear
  y mostrar contacto mas difuso en HLL.
- Si se anade un test Python, solo del parser `.xl` y del escritor de
  namelist (no del solver).

---

## J. Como se corre en Windows

```
# 1. Nucleo (ya compilado en esta maquina)
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cmake --build build

# 2. Dependencias del laboratorio
python -m pip install -r tools/requirements-lab.txt

# 3. Arranque
.\run_lab.ps1
# o: python tools/fvlab_lab/app.py
```

Busqueda del binario: `EULER1D_BIN`, luego `build/xEuler1D.exe`,
`build-debug/xEuler1D.exe`.

---

## K. Migracion futura

El controlador (`controller.py`) es la unica puerta al nucleo. Un servidor
Linux o una GUI Qt posterior solo reemplaza `ui.py`. El namelist y los `.xl`
siguen siendo el contrato.

`PENDING DIRECTOR INPUT`: entorno servidor final, 2D, HLLE, WENO, JSON.

---

## L. Prioridad

### MUST HAVE (sabado)

- Lanzar el C++ real desde la GUI.
- Configurar: caso, solver, limitador, N, CFL, γ, tf, x0, xmin/xmax, bc.
- Plot ρ/u/p vs exacta.
- Comparar dos corridas.
- Manual interactivo ligado a la seleccion.
- Ejemplos guiados (Toro 1, Toro 2, Toro 5, entropia).
- Mensajes de error del nucleo visibles.

### SHOULD HAVE

- Ver codigo fuente real del modulo seleccionado.
- Metricas L1/L2 en el panel.
- Gravedad / geometria / every_out (avanzado).
- Log de la corrida.

### NICE TO HAVE

- Animacion temporal si hay varios bloques `#Time`.
- Edicion libre de estados L/R (caso custom).
- Tema oscuro, empaquetado .exe.

### FUTURE / PENDING DIRECTOR

- 2D, VTK, HLLE, WENO, JSON, Catch2, servidor.
- Estados L/R como claves de namelist (cambio de nucleo, no del prototipo).

---

## Clasificacion honesta

```
PROTOTYPE IMPLEMENTED   laboratorio Python + nucleo C++ intacto
PROTOTYPE SIMPLIFIED    estados L/R no editables en el namelist;
                        ecuaciones en texto, no LaTeX renderizado;
                        una sola ventana, no IDE
FUTURE IMPLEMENTATION   2D, HLLE, WENO, JSON, web/servidor, bindings C-API
PENDING DIRECTOR INPUT  alcance A-1/A-2, material 2D, servidor
```

---

## Decisiones tomadas aqui (sin repreguntar)

| ID | Decision |
|---|---|
| MVP-01 | GUI = Tkinter + matplotlib, no Qt |
| MVP-02 | El nucleo se invoca por proceso, no por pybind11 |
| MVP-03 | Contrato = namelist + `.xl` |
| MVP-04 | Cero cambios en formulas numericas |
| MVP-05 | Catalogo pedagogico en Python, fragmentos leidos del repo |
| MVP-06 | Comparacion = dos ultimas corridas en memoria |

Siguiente accion: implementar MUST HAVE en `tools/fvlab_lab/`.

---

## Estado de ejecucion (28 sep 2026, noche)

MUST HAVE del laboratorio implementado:

- `tools/fvlab_lab/` (controlador, catalogo, ejemplos, UI, parser `.xl`)
- `run_lab.ps1` / `run_lab.bat`
- `python tools/fvlab_lab/test_lab.py` (parser + namelist + humo)

Como demostrar el sabado:

1. `.\run_lab.ps1`
2. Cargar ejemplo **2. Toro 5 — HLL vs HLLC**
3. Ejecutar (HLLC), guardar para comparar
4. Cambiar solver a `hll`, ejecutar de nuevo
5. Variable: densidad. El contacto de HLL es mas ancho.
6. Abrir la ficha HLLC / HLL: ecuaciones + archivo real `hllc.hpp` / `hll.hpp`.
