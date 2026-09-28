# Definición del proyecto

**Fase:** planificación previa a la implementación
**Fecha:** 27 de septiembre de 2026
**Estado de este documento:** borrador de trabajo

## Convención de etiquetas

Todos los documentos de planificación usan estas cuatro etiquetas. Aparecen
junto a cada afirmación que no sea un hecho verificable sobre el código actual.

| Etiqueta | Significado |
|---|---|
| `CONFIRMED` | Está en los objetivos de la tesis, o es un hecho comprobado sobre el código |
| `DECIDED` | Decisión tomada y registrada en `decision_log.md` |
| `PROPOSED` | Propuesta técnica razonada, aún sin aprobar |
| `ASSUMED` | Supuesto de trabajo; si es falso, hay que rehacer algo |
| `PENDING INPUT FROM DIRECTOR` | No se puede cerrar sin material o decisión del director |

---

## 1. Identidad del proyecto

### Nombre

`PROPOSED` — **FVLab** (*Finite Volume Laboratory*), con namespace C++ `fvlab`.

El repositorio se llama hoy `Euler1D_Cpp` y el namespace es `euler1d`. Ambos
quedan incorrectos en cuanto entre el 2D, y un evaluador de ingeniería de
sistemas lo va a notar. Alternativas consideradas: `EulerFVM` (correcto pero
sugiere que solo hace Euler, y el diseño admite otros sistemas), `CFDLab`
(sobreestima el alcance: no hay flujo viscoso ni turbulencia), mantener
`Euler1D_Cpp` (incorrecto).

El cambio es un renombrado mecánico de namespace más el renombrado del
repositorio en GitHub, que deja redirección automática. Ver `ADR-001`.

**Requiere confirmación del usuario antes de ejecutarse.**

### Propósito

`CONFIRMED` — Construir un framework de volúmenes finitos, modular y
transparente, que permita **comparar y validar esquemas numéricos
intercambiables** para las ecuaciones de Euler compresibles en 1D y 2D, y que
sirva a la vez como banco de pruebas de investigación y como material didáctico.

El propósito se deriva de la pregunta de investigación de la tesis:

> ¿Cómo diseñar un framework computacional basado en el método de volúmenes
> finitos que permita la comparación y validación de distintos métodos, como los
> solvers de Riemann HLLE y HLLC?

De ahí salen las dos afirmaciones que el proyecto debe demostrar con evidencia, y
conviene mantenerlas separadas porque se prueban de formas distintas:

- **Afirmación de arquitectura.** Un componente numérico se puede sustituir sin
  modificar el núcleo. Se demuestra midiéndolo: al añadir un solver nuevo, se
  cuenta y se reporta cuántos archivos del núcleo hubo que tocar.
- **Afirmación numérica.** El framework calcula correctamente, de modo que las
  diferencias entre esquemas son atribuibles al método y no a un error de
  implementación. Se demuestra con orden de convergencia medido y comparación
  contra solución exacta.

La segunda es prerrequisito de la primera. Comparar dos implementaciones sin
haber verificado antes que ambas son correctas no demuestra nada.

---

## 2. Alcance

### Dentro del alcance

`CONFIRMED` a partir de los objetivos específicos:

- Ecuaciones de Euler compresibles, gas ideal politrópico, no viscoso.
- Dominios estructurados uniformes cartesianos, 1D y 2D.
- Reconstrucción: orden 1 constante, orden 2 MUSCL, orden 3+ WENO.
- Limitadores de pendiente: minmod, van Leer, superbee como mínimo.
- Solvers de Riemann: HLL, HLLE, HLLC.
- Integración temporal explícita, Runge-Kutta de orden ≥ 2, con paso adaptativo
  por condición CFL.
- Condiciones de frontera: transmisiva, reflectiva, periódica.
- Solución exacta del problema de Riemann como referencia de verificación.
- Métricas de error \(L_1\) y \(L_2\), y análisis de orden de convergencia.
- Configuración externa que permita cambiar esquemas sin recompilar.
- Suite de pruebas automatizada.
- Documentación técnica y documentación pedagógica.

### Fuera del alcance

Declarado explícitamente para que no reaparezca como entrega tardía.

| Fuera | Razón |
|---|---|
| Magnetohidrodinámica (MHD 1D y 2D) | Los objetivos hablan solo de Euler. Existen en el proyecto del director, no en esta tesis |
| Aguas someras | Igual que arriba |
| Paralelización con OpenMP | La presentación del tema la sitúa en «trabajos futuros»; el anteproyecto en «una fase posterior». El compromiso es que la arquitectura no lo impida, no entregarlo |
| Flujo viscoso, conducción de calor, turbulencia, química, multifase | Fuera del modelo físico declarado |
| Mallas no estructuradas, refinamiento adaptativo, geometría arbitraria | Los objetivos dicen «dominios estructurados» |
| Esquemas implícitos | Los objetivos dicen «integración temporal explícito» |
| Interfaz gráfica propia | El postprocesamiento se hace con matplotlib y ParaView |
| Gravedad bien balanceada, geometría radial, Sedov-Taylor, Kelvin-Helmholtz, Rayleigh-Taylor | No aparecen en ningún objetivo. Los ya migrados se conservan como regresión contra el Fortran, pero **no se invierte tiempo adicional** en ellos |

### Límites actuales del proyecto (estado real, no aspiración)

`CONFIRMED` por inspección del código:

- Solo existe implementación **1D**. No hay una línea de 2D.
- Solvers implementados: **HLL y HLLC**. No hay HLLE.
- Reconstructores implementados: **uno** (`MusclReconstruction`), con el orden 1
  como opción de limitador interna, no como módulo separado. No hay WENO.
- Configuración: namelist de Fortran. No hay JSON ni YAML.
- Salida: formato `.xl` heredado del Fortran. No hay CSV ni VTK.
- Pruebas: arnés propio de 87 casos. No se usa Catch2 ni GoogleTest.
- El estándar declarado en CMake es C++20; el anteproyecto dice C++17.

**No se debe afirmar en la memoria de tesis ninguna capacidad de esta lista
hasta que esté implementada y validada.**

---

## 3. Usuarios previstos

| Usuario | Qué necesita | Qué no necesita |
|---|---|---|
| **Estudiante de CFD** | Correr un caso estándar, cambiar un esquema, ver el efecto en una gráfica | Leer C++, compilar con opciones, entender la arquitectura |
| **Investigador / tesista** | Añadir un esquema nuevo y compararlo con los existentes bajo condiciones idénticas | Reescribir el núcleo |
| **Desarrollador del framework** | Extender, depurar, mantener la suite verde | — |
| **Evaluador de la tesis** | Trazar cada requisito hasta su implementación y su evidencia de verificación | Ejecutar nada |

El último es un usuario real y con frecuencia se olvida. Es el destinatario de
`traceability.md`.

---

## 4. Casos de uso previstos

Detallados en `use_cases.md`. Resumen:

1. Configurar y ejecutar una simulación desde un archivo externo.
2. Ejecutar un caso de prueba estándar y comparar con la solución exacta.
3. Cambiar reconstructor, limitador o solver de Riemann sin recompilar.
4. Medir error \(L_1\) y \(L_2\) frente a una referencia.
5. Ejecutar un barrido de resoluciones y estimar el orden de convergencia.
6. Producir la matriz de comparación reconstructor × solver.
7. Añadir un caso de prueba nuevo.
8. Añadir un método numérico nuevo.
9. Verificar una implementación nueva contra el solver Fortran de referencia.

---

## 5. Qué ES y qué NO ES el framework

**Es** un banco de pruebas numérico: un entorno donde se aísla una sola decisión
algorítmica y se mide su efecto bajo condiciones por lo demás idénticas.

**Es** un artefacto pedagógico: el código está escrito para poder trazar la
ecuación del pizarrón hasta la línea que la evalúa.

**Es** un artefacto de software con ciclo de vida: versionado, documentado,
probado y reproducible.

**No es** un código de producción industrial. No compite con OpenFOAM ni SU2 en
capacidades, geometría ni rendimiento.

**No es** un código de alto rendimiento. Prioriza claridad sobre velocidad
cuando hay conflicto, dentro de lo razonable.

**No es** un solver de propósito general. Resuelve Euler compresible en mallas
estructuradas uniformes, y nada más.

**No es** una traducción del Fortran. El Fortran es la **referencia de
verificación**, no la especificación del diseño. Esta distinción es la más
importante de todo el documento: el entregable es un framework modular, mientras
que el material de referencia son dos programas monolíticos.

---

## 6. Interacción: interfaz de línea de comandos

`PROPOSED`. No se implementa en esta fase; se especifica para que la
implementación no tenga que inventarla.

### Ejecutable del solver

```
fvlab [opciones]

  -i, --input <archivo>     Archivo de configuración. Por defecto: ./input.json
  -o, --output <directorio> Directorio de salida. Por defecto: directorio actual
  -q, --quiet               Sin salida por pantalla (solo código de retorno)
  -v, --verbose             Informe por paso temporal
      --dry-run             Valida la configuración y la imprime resuelta, sin simular
      --print-config        Imprime la configuración efectiva tras resolver el caso
  -h, --help                Ayuda
      --version             Versión y hash de commit
```

`--dry-run` y `--print-config` no existen hoy y se añaden por una razón
concreta: el caso de prueba puede sobreescribir valores del usuario (dominio,
frontera, tiempo final), y hoy eso solo se ve en un aviso por pantalla. Poder
imprimir la configuración resuelta sin simular evita la clase de error que la
auditoría del Fortran encontró dos veces.

Códigos de retorno:

| Código | Significado |
|---|---|
| 0 | La simulación terminó correctamente |
| 1 | Error de configuración (archivo ausente, clave desconocida, valor fuera de rango) |
| 2 | Error numérico durante la ejecución (estado no finito detectado) |
| 3 | Error de entrada/salida (no se puede escribir el directorio de salida) |

Distinguir 1 de 2 importa: el primero es culpa del usuario, el segundo es un
resultado del experimento y puede ser justo lo que se quería observar.

### Ejecutable de comparación

```
fvlab-compare <directorio_A> <directorio_B> [--tolerance <valor>] [--csv <salida>]
```

Compara dos corridas campo a campo y reporta \(\max|diff|\), \(L_1\), \(L_2\) y
error relativo. Devuelve 0 si todo está dentro de tolerancia, 1 si no.

### Salida por pantalla

`PROPOSED`. Formato estable y parseable, porque los scripts de validación lo
leen.

```
fvlab 0.2.0 (commit a1b2c3d)
config: caso=sod  dim=1  N=400  reconstruccion=muscl  limitador=vl  solver=hllc  cfl=0.900
referencia: p* = 3.031301e-01   u* = 9.274526e-01
pasos: 245   t final: 0.200000   tiempo de pared: 0.031 s
pisos de positividad: 0
salida: out/sod/
```

---

## 7. Interacción: archivo de configuración

`PROPOSED`, sujeto al punto abierto `C-3` de `alcance.md`.

El anteproyecto (§4.4) exige JSON o YAML. El código actual lee namelist de
Fortran. La propuesta es soportar **ambos**, con JSON como formato principal de
usuario y el namelist como entrada de compatibilidad para poder seguir
validando contra el Fortran del director indefinidamente. Los dos producen el
mismo objeto de configuración validado.

Eso no es duplicación gratuita: es la demostración práctica del desacoplamiento
que defiende el Objetivo 1, y es lo que preserva la capacidad de regresión.

### Esquema propuesto

```json
{
  "case": "sod",
  "mesh": {
    "dimension": 1,
    "cells": [400],
    "domain": { "min": [0.0], "max": [1.0] }
  },
  "physics": {
    "gamma": 1.4
  },
  "numerics": {
    "reconstruction": "muscl",
    "limiter": "vanleer",
    "riemann_solver": "hllc",
    "time_integrator": "ssprk2",
    "cfl": 0.9
  },
  "boundary": {
    "x_min": "transmissive",
    "x_max": "transmissive"
  },
  "time": {
    "final_time": 0.2,
    "output_every_steps": 0
  },
  "output": {
    "directory": "out/sod",
    "format": ["csv"],
    "variables": ["density", "velocity", "pressure"],
    "write_reference": true
  }
}
```

Un caso 2D cambia `dimension` a 2, `cells` a `[400, 400]`, los dominios a pares,
añade `y_min`/`y_max` en `boundary`, y `format` a `["vtk"]`.

Decisiones de esquema, con su razón:

- **`cells` y `domain` son arreglos**, no `nx`/`ny`. Así el mismo esquema sirve
  para 1D y 2D sin claves opcionales que signifiquen cosas distintas según la
  dimensión.
- **`boundary` es por borde, no global.** El código 2D del director necesita
  periódica en x y pared en y simultáneamente (su modo `rt`). Un único campo
  `boundary` no lo puede expresar.
- **`reconstruction` y `limiter` están separados.** Hoy el orden 1 se pide con
  `limiter = 'fo'`, que confunde dos conceptos distintos. Con WENO en juego, el
  limitador deja de aplicar y la separación se vuelve necesaria.
- **Claves desconocidas son error, no aviso.** Es la lección más caras de la
  auditoría del Fortran: una errata de un carácter corría otra malla en silencio.
- **Sin valores por defecto ocultos en el parser.** Los defaults son del caso de
  prueba y se reportan cuando se aplican.

`PENDING INPUT FROM DIRECTOR` — si prefiere YAML sobre JSON. JSON no necesita
dependencias externas si se escribe un lector mínimo; YAML sí.

---

## 8. Estructura de salida

`PROPOSED`.

```
out/sod/
├── run_info.txt          configuración efectiva, versión, commit, tiempo de pared
├── density.csv           columnas: x, valor  (un bloque por instante volcado)
├── velocity.csv
├── pressure.csv
└── reference/
    ├── density.csv       solución exacta muestreada en los centros de celda
    ├── velocity.csv
    └── pressure.csv
```

En 2D, los `.csv` se sustituyen por `.vti` o `.vtr` legibles por ParaView.

`run_info.txt` es el artefacto de reproducibilidad: sin él, un resultado no se
puede volver a producir. Debe contener la configuración **resuelta** (después de
que el caso de prueba aplique sus valores), la versión, el hash del commit, el
compilador y sus opciones.

**Nota de política:** `run_info.txt` incluye el tiempo de pared, que cambia en
cada ejecución. Por eso **no** se usa nunca como archivo de referencia de
regresión. Ver `validation_strategy.md`.

---

## 9. Flujo de trabajo típico de un usuario

`PROPOSED`. Esto es el guion del tutorial pedagógico del Objetivo 4.

```
1. Copiar un ejemplo:        cp examples/sod.json mi_caso.json
2. Editar lo que interese:   "riemann_solver": "hll"
3. Correr:                   fvlab -i mi_caso.json -o out/hll
4. Repetir con otro solver:  "riemann_solver": "hllc"  →  out/hllc
5. Comparar:                 fvlab-compare out/hll out/hllc
6. Graficar:                 python tools/plot_comparison.py out/hll out/hllc
```

Los pasos 4 y 5 son la experiencia que justifica todo el proyecto: cambiar una
cadena en un archivo y medir el efecto. Si esa secuencia no resulta trivial, la
arquitectura ha fallado en su objetivo, independientemente de lo limpia que sea
por dentro.

---

## 10. Riesgos del proyecto

| Riesgo | Impacto | Mitigación |
|---|---|---|
| El alcance crece con material nuevo del director | Alto | `alcance.md` firmado; MHD y aguas someras declarados fuera |
| La generalización a 2D obliga a rediseñar el 1D ya validado | Alto | *Spike* acotado antes de comprometerse (ver `roadmap.md`, Hito 6) |
| WENO consume el tiempo que necesitan los entregables centrales | Medio | Punto abierto `A-2`: confirmar si es entregable o trabajo futuro |
| La regresión contra el Fortran se rompe al refactorizar | Medio | La suite de regresión corre en cada commit; el número de pasos debe coincidir |
| Se compara HLLE con HLLC sin haber verificado ninguno | Alto | El estudio de convergencia es prerrequisito de la matriz de comparación |
| El C++ acaba validándose contra su propia salida | Alto | Política explícita en `reference_data`, ver `validation_strategy.md` |
