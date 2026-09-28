# PENDING INPUT FROM DIRECTOR

**Fecha:** 27 de septiembre de 2026
**Documento hermano:** `alcance.md` (los doce puntos abiertos `A-1` a `C-5`)

## Cómo leer este documento

Todo lo que aparece aquí es algo que **no se puede cerrar responsablemente** sin
material o decisión del director. Para cada punto:

- qué falta exactamente,
- por qué importa,
- qué parte del diseño depende de ello,
- **si el desarrollo puede avanzar sin ello**.

La última columna es la importante. Nada de esta lista bloquea todo el proyecto;
la mayoría bloquea una parte concreta y acotada.

**Regla de trabajo:** no se detiene trabajo que no dependa de un punto pendiente.
Los hitos H2, H3, H4, H6 y H8 del roadmap se pueden ejecutar íntegramente con lo
que ya hay.

---

# Parte I — Material que falta

## P-01 — `correr_pruebas.py`, la suite de regresión del proyecto

**Qué falta.** El script que el `Makefile` de `Code_Euler2D` invoca:

```
test: euler2d
	cd .. && python3 correr_pruebas.py Code_Euler2D
```

Se llama con el nombre del módulo como argumento y admite `--rapido`. Vive en el
directorio raíz del proyecto del director y no venía en ninguno de los dos
paquetes recibidos.

**Por qué importa.** Es la definición operativa de «pasa la regresión» según su
criterio. Sin él no sabemos qué casos considera obligatorios, qué tolerancias
aplica, ni qué métricas reporta. Nuestra suite puede estar exigiendo más o menos
que la suya, y en cualquiera de los dos casos conviene saberlo.

**De qué depende.** `validation_strategy.md` Parte III (la matriz de validación) y
el punto abierto `B-5` (tolerancia aceptable).

**¿Bloquea?** **No.** Tenemos una suite propia de 87 casos que ya valida 26 de 26
configuraciones. Este script mejoraría la alineación con su criterio, no la
sustituye.

## P-02 — `Code_Euler1D` actualizado

**Qué falta.** La versión actual de su `Code_Euler1D`.

**Evidencia concreta.** El paquete recibido corresponde al estado del 18 de
agosto a las 07:36. Ese mismo día, a las 18:21, existe un commit —«El piso de
presión deja de dispararse en silencio»— cuyo texto declara explícitamente:
«Ahora los cuatro códigos con ecuación de energía (Euler1D, Euler2D, MHD1D,
MHD2D) cuentan los disparos y avisan al final de la corrida». Se verificó que el
`euler1d.f90` recibido **no contiene** ese contador, y que su `Makefile` tampoco
tiene el objetivo `test` que el 2D sí recibió a las 18:32.

**Por qué importa.** La migración del 1D se validó contra una versión que él ya
no tiene. No sabemos si ese commit tocó algo más del 1D.

**De qué depende.** La afirmación de equivalencia numérica (NFR-001) y la
procedencia de los datos de `tests/reference/` (Clase C de
`validation_strategy.md`).

**¿Bloquea?** **No, pero hay que resolverlo antes de la memoria.** La validación
actual es real y medida; simplemente está referida a una versión anterior, y eso
hay que poder declararlo con precisión. Nota: nuestro C++ ya implementa el
contador de pisos por decisión propia, así que en ese punto concreto puede que ya
esté alineado.

## P-03 — Confirmación de que no hay material posterior

**Qué falta.** Confirmación de que no existen entregas de Euler posteriores al 18
de agosto de 2026.

**Por qué importa.** Han pasado cinco semanas desde el último commit visible. El
proyecto avanzó seis códigos en tres días, así que el ritmo permite que exista
material nuevo.

**¿Bloquea?** **No.** Pero un cambio de alcance a mitad de implementación es el
riesgo de proyecto mejor identificado, y esta pregunta lo reduce.

## P-04 — Un ejemplo del estándar esperado en resultados

**Qué falta.** Una figura o tabla de un trabajo similar que sirva de referencia
de formato y nivel de detalle para el capítulo de resultados.

**Por qué importa.** El entregable central (FR-035, la matriz de comparación) es
una tabla y una figura. Saber qué formato espera evita rehacerlas.

**De qué depende.** El hito H6, y las herramientas de `tools/`.

**¿Bloquea?** **No.** Se puede producir con criterio propio y ajustar después. Un
ejemplo ahorraría una iteración.

---

# Parte II — Decisiones de alcance

## P-05 — Qué documento de objetivos rige (`A-1`)

**El problema.** Existen dos redacciones de los objetivos específicos y **no
exigen lo mismo**:

| | Presentación del tema | Anteproyecto |
|---|---|---|
| Reconstructores | «al menos **dos**» | «**tres**, incluido WENO» |
| Solvers | «**dos** solvers» (sin nombrar) | «**HLLE y HLLC**» |
| Casos 2D | «en 1D **y 2D**» | «**al menos un caso** en 2D» |

**Por qué importa.** Contra la presentación del tema, el único hueco real es el
2D: «al menos dos reconstructores» ya se cumple (orden 1 y MUSCL) y «dos solvers»
ya se cumple (HLL y HLLC). Contra el anteproyecto faltan además HLLE y WENO.

**De qué depende.** Todo el alcance, y por tanto todo el roadmap.

**¿Bloquea?** **Parcialmente.** Se asume que rige el anteproyecto, por ser
posterior y más detallado, y se trabaja en consecuencia. El único hito realmente
condicionado es H12 (WENO).

**Es la pregunta de mayor impacto de toda la lista.**

## P-06 — Si WENO es entregable o trabajo futuro (`A-2`)

**Por qué importa.** Es la adición técnica más grande del proyecto: indicadores
de suavidad, pesos no lineales, y —si es WENO5— ampliar las celdas fantasma de 2
a 3, lo que obliga a revisar las tres condiciones de frontera y convierte el
cambio en una modificación del núcleo.

Y es el único entregable grande que **no toca la pregunta de investigación**, que
es sobre solvers de Riemann y no sobre reconstructores.

**De qué depende.** El hito H12 completo, y la anchura de la zona fantasma
(FR-009) que se diseña en H9.

**¿Bloquea?** **Solo H12.** Pero conviene resolverlo antes de H9, porque la
anchura configurable de celdas fantasma se diseña ahí y es más barato hacerlo bien
la primera vez que readaptarlo.

**Nota de planificación honesta.** Si el tiempo aprieta, es el primer candidato a
pasar a trabajo futuro. `A-2` existe para decidirlo por escrito y a tiempo, no
para descubrirlo tarde.

## P-07 — Reflexión de Mach: comprometida u opcional (`B-2`)

**El problema.** El Objetivo 3 del anteproyecto pide «al menos un caso en 2D»,
pero la metodología (Etapa 3) nombra **dos**: Lax-Liu y la reflexión de Mach.

**Por qué importa.** La reflexión de Mach requiere una condición de frontera
**dependiente del tiempo** que no existe en el código de referencia, además del
tratamiento de la rampa. Lax-Liu por sí solo satisface el objetivo tal como está
redactado.

**De qué depende.** El hito H13.

**¿Bloquea?** **No.** Lax-Liu se implementa en cualquier caso.

## P-08 — Alcance de la matriz de comparación (`B-4`)

**El problema.** El §6.4 dice que el análisis de convergencia se hará «para cada
combinación de reconstructor y solver». Tres por tres son nueve combinaciones,
por cada malla y cada caso.

**Por qué importa.** Nueve combinaciones × cuatro resoluciones × varios casos es
sensiblemente más trabajo —y más superficie de figura— que aislar HLLE contra
HLLC con el reconstructor fijo.

**De qué depende.** Los hitos H6 y H13, y el diseño de las herramientas de
`tools/`.

**¿Bloquea?** **No.** Se puede construir la infraestructura para la matriz
completa y rellenar solo lo que se decida presentar.

## P-09 — Tolerancia aceptable frente al Fortran (`B-5`)

**Qué falta.** Confirmación del criterio de aceptación numérica.

**Propuesta a confirmar.** \(\max|diff| \le 10^{-12} + 10^{-9}\max|ref|\) en
todas las variables, **y número de pasos temporales idéntico**.

**Estado actual.** Ese criterio ya se cumple: 26 de 26 configuraciones, con los
pasos coincidiendo exactamente. Además 9 de 13 casos archivados son idénticos
byte a byte.

**¿Bloquea?** **No.** Se está cumpliendo un criterio más estricto de lo que
probablemente exija. Confirmarlo sirve para poder afirmarlo en la memoria sin
tener que defender el número.

---

# Parte III — Decisiones técnicas declaradas en el anteproyecto

Los cuatro puntos donde el código actual se aparta de un documento ya entregado.
Ninguno es un error; todos conviene resolverlos por escrito.

| Punto | El anteproyecto dice | El código hace | Propuesta | ¿Bloquea? |
|---|---|---|---|---|
| `C-1` | C++17 (§5.1) | C++20 | Bajar a C++17. Verificado: no se usa nada exclusivo de C++20, es una línea | No |
| `C-2` | Google Test o Catch2 (§5.2) | Arnés propio | Catch2 v2 incorporado, sin red (`ADR-004`) | Solo H7 |
| `C-3` | JSON o YAML con *factory* (§4.4) | Namelist de Fortran | Ambos: JSON principal, namelist de compatibilidad (`ADR-011`) | Solo H5 |
| `C-4` | CSV en 1D, VTK en 2D (§5.3) | Formato `.xl` | Ambos, por la misma razón (`ADR-011`) | Solo H5, H11 |

## P-10 — JSON o YAML (`C-3`, matiz)

**Qué falta.** Si prefiere YAML sobre JSON.

**Por qué importa.** JSON se puede analizar sin dependencias externas, lo que
preserva NFR-005. YAML no: requeriría una biblioteca, y con ella una dependencia
de red o un submódulo.

**Recomendación.** JSON, por esa razón concreta. Si prefiere YAML, hay que decidir
si se acepta la dependencia o se soporta un subconjunto de YAML escrito a mano,
que es una mala idea por lo sutil que resulta el formato.

## P-11 — Orden de WENO (`C-5`)

**Qué falta.** WENO3 o WENO5.

**Por qué importa.** WENO3 cabe en las dos celdas fantasma actuales. WENO5 exige
tres y obliga a revisar las tres condiciones de frontera. Ambos cumplen «orden
3+» tal como está redactado el objetivo.

**¿Bloquea?** Condicionado a `A-2`. Si WENO entra, conviene decidirlo antes de H9.

## P-12 — Formato del diagrama de componentes (`B-1`)

**Qué falta.** Si espera UML formal, notación C4, o un diagrama libre.

**Por qué importa.** Es un entregable explícito del Objetivo 1.

**Estado actual.** Hay un diagrama en `architecture.md` §13, en notación libre,
que muestra los componentes, las cinco interfaces con sus implementaciones y las
dependencias. Puede servir tal cual o rehacerse en notación formal.

**¿Bloquea?** **No.** Solo el hito H15.

---

# Parte IV — Decisiones que requieren su criterio, no el nuestro

## P-13 — Cualquier desviación del comportamiento numérico de referencia

**Qué es.** El procedimiento de `numerical_invariants.md` Parte III establece que
una desviación deliberada del comportamiento del solver Fortran requiere decisión
registrada.

**Por qué le corresponde a él.** Si la desviación afecta a un resultado
publicable, no es una decisión de ingeniería de software. Puede que el
comportamiento que a nosotros parece un defecto sea intencional por una razón
física que no conocemos.

**Procedimiento propuesto.** Se documenta el hallazgo, se mide su efecto, y se
consulta **antes** de cambiarlo. Mientras no haya respuesta, se preserva el
comportamiento original y se deja anotado.

**Estado actual.** Hay 17 peculiaridades catalogadas en `inconsistencies.md`,
todas clasificadas como preservadas o reestructuradas. Ninguna desviación
deliberada pendiente de consulta.

## P-14 — Estilo de las figuras

**Qué falta.** Si el estilo de sus herramientas —mapas en `inferno`, perfiles en
blanco y negro, tipografía serif— debe adoptarse como estándar del proyecto.

**Por qué importa poco y aun así conviene.** Solo afecta a la presentación, pero
decidirlo una vez ahorra rehacer figuras al final.

**¿Bloquea?** **No.**

## P-15 — Objetivo de rendimiento (NFR-015)

**Qué falta.** Un número para «tiempo de ejecución razonable para fines de
validación» (§4.3).

**Propuesta a confirmar.** Lax-Liu en malla \(400^2\) hasta su tiempo final en
menos de 10 minutos en un solo núcleo.

**Por qué importa.** Sin un número, NFR-015 no es verificable y no se puede
afirmar que se cumple.

**¿Bloquea?** **No.** Se mide lo que salga y se registra.

---

# Resumen: qué se puede hacer ahora mismo

| Hito | ¿Bloqueado? | Por qué |
|---|---|---|
| H2 Infraestructura | **No** | Solo CI, renombrado y pruebas de rigor |
| H3 Huecos baratos | **No** | Sod, Lax y extraer el orden 1 |
| H4 **HLLE y medición** | **No** | Nada pendiente lo condiciona |
| H5 Adaptadores | Sí | `C-3`, `C-4` |
| H6 **Matriz 1D** | **No** | Se puede construir para la matriz completa y presentar un subconjunto |
| H7 Catch2 | Sí | `C-2` |
| H8 **Spike de dimensión** | **No** | Decisión interna de arquitectura |
| H9 Núcleo 2D | Parcial | Conviene `A-2` para la anchura de fantasmas |
| H10 Operador 2D | **No** | — |
| H11 VTK | Sí | `C-4` |
| H12 WENO | Sí | `A-2`, `C-5` |
| H13 Validación 2D | Parcial | `B-2`, `B-4` |
| H14 Documentación | **No** | — |

**Cinco de los seis hitos de la ruta crítica no están bloqueados por nada.** El
trabajo de mayor valor —HLLE, su medición del acoplamiento, y la matriz de
comparación en 1D— se puede empezar hoy.
