# Roadmap de implementación

**Fecha:** 27 de septiembre de 2026

## Criterio de ordenación

Los hitos no están ordenados por tamaño ni por el orden lógico de los
componentes. Están ordenados por **cuánto riesgo eliminan**.

Tres reglas gobiernan el orden:

1. **Primero lo que puede invalidar el diseño.** El *spike* de dimensionalidad va
   antes de escribir 2D, porque si falla hay que cambiar de estrategia y es mucho
   más barato descubrirlo con cero líneas de 2D escritas que con tres mil.
2. **Un resultado publicable lo antes posible.** La matriz de comparación en 1D
   llega en el hito 6, a mitad del camino. A partir de ahí existe algo que
   responde la pregunta de investigación, aunque el 2D no esté.
3. **Lo barato y visible antes que lo caro.** HLLE cuesta poco y cierra dos
   huecos críticos. WENO cuesta mucho y no toca la pregunta de investigación.

---

## Vista general

```
H1  Cierre de decisiones                      🔒 bloqueado por el director
H2  Infraestructura y huecos de rigor
H3  Huecos baratos de alcance
H4  HLLE + medición del acoplamiento          ◄── pregunta de investigación
H5  Adaptadores de configuración y salida
H6  Matriz de comparación 1D                  ◄── primer resultado publicable
H7  Migración a Catch2
H8  Spike de dimensionalidad                  ◄── decide la estrategia 2D
H9  Núcleo 2D
H10 Operador 2D y Lax-Liu
H11 Salida VTK
H12 WENO                                      🔒 condicional a A-2
H13 Validación 2D y matriz 2D
H14 Documentación pedagógica y reproducibilidad
H15 Cierre de tesis
```

---

## H1 — Cierre de decisiones

**Objetivo.** Resolver los doce puntos abiertos de `alcance.md` para que el resto
del trabajo no se rehaga.

**Prerrequisitos.** Enviar `alcance.md` al director y obtener respuesta.

**Trabajo.** Ninguno de implementación. Registrar cada respuesta en
`decision_log.md` y actualizar los documentos afectados en el mismo commit.

**Criterio de cierre.** `A-1`, `A-2`, `B-4` y `C-1` a `C-4` resueltos. Los demás
pueden quedar abiertos sin bloquear.

**Si no hay respuesta.** No bloquea H2 ni H3. Sí bloquea H5 (`C-3`, `C-4`), H7
(`C-2`) y H12 (`A-2`). Se avanza por H2, H3, H4, H6, H8.

---

## H2 — Infraestructura y huecos de rigor

**Objetivo.** Que los requisitos no funcionales pasen de afirmados a verificados.
Es el hito que hace que todo lo demás sea comprobable.

**Prerrequisitos.** Ninguno.

**Trabajo.**

- Integración continua con la matriz de cinco trabajos de `technology_stack.md` §5.
- Bajar a C++17 (`C-1`; verificado que no se usa nada exclusivo de C++20).
- Renombrar proyecto y namespace a `fvlab` (`ADR-001`), y renombrar el
  repositorio.
- `clang-format` configurado, aplicado en un commit de formato aislado.
- Completar `run_info.txt`: hash de commit, compilador y opciones (FR-031).
- Prueba de determinismo: dos ejecuciones idénticas byte a byte (NFR-002).
- Pruebas de los códigos de salida 2 y 3 (NFR-011).
- Separar `tests/` en `unit/`, `integration/` y `regression/`.
- Crear `tests/baseline/` vacío con su documento de política.

**Componentes afectados.** CMake, `main.cpp`, `OutputManager`, estructura de
`tests/`. Ningún componente numérico.

**Validación.** La regresión debe quedar **exactamente** igual: este hito no
cambia numérica. Si cambia algo, el renombrado tocó lo que no debía.

**Criterio de cierre.** Los cinco trabajos de CI en verde. NFR-002, NFR-004,
NFR-011 y NFR-012 con evidencia en `traceability.md`.

---

## H3 — Huecos baratos de alcance

**Objetivo.** Cerrar los huecos frente a los objetivos que cuestan horas, no
semanas.

**Prerrequisitos.** H2 (para que CI proteja los cambios).

**Trabajo.**

- Extraer `FirstOrderReconstruction` como clase propia que implementa la
  interfaz (FR-015). Hoy el orden 1 es una opción de limitador.
- Añadir el **Sod clásico** a la tabla de casos (FR-026).
- Añadir el **problema de Lax** (FR-026).
- Ejemplos de configuración para ambos.
- `--dry-run` y `--print-config` (FR-006).

**Componentes afectados.** `Reconstruction`, `TestCaseDefinition`, CLI.

**Pruebas.** Valores declarados de los dos casos nuevos; que
`FirstOrderReconstruction` produce resultados idénticos al limitador `fo`
anterior.

**Validación.** Los dos casos nuevos contra el solver exacto, con \(L_1\) y
\(L_2\) registrados. La regresión de los casos existentes intacta.

**Criterio de cierre.** El objetivo «al menos dos reconstructores» se cumple con
módulos concretos. Los cuatro casos 1D que nombra el anteproyecto existen.

**Nota.** La prueba de que `FirstOrderReconstruction` reproduce exactamente el
comportamiento del limitador `fo` es la parte que importa. Si difiere en el
último bit, algo se reordenó (`INV-002`).

---

## H4 — HLLE y medición del acoplamiento

**Objetivo.** Implementar el solver que falta y, al hacerlo, **medir** la
afirmación arquitectónica del `Obj-1`.

**Prerrequisitos.** H2. Conviene después de H3 para poder comparar sobre los
casos nuevos.

**Trabajo.**

- `HlleRiemannSolver`: promedios de Roe y estimación de velocidades de Einfeldt
  (FR-018).
- Valor en el enumerado, conversiones de cadena, rama en la fábrica.
- **Contar y registrar los archivos del núcleo modificados** (FR-037).

**Componentes afectados.** Solo `numerical/riemann/`, el enumerado de
configuración y la fábrica. Si se toca algo más, es un hallazgo.

**Pruebas.** Las cuatro propiedades de solver de Riemann de
`validation_strategy.md` §1. Más una que compruebe que las velocidades de onda de
HLLE difieren de las de HLL de forma medible en al menos un caso.

**Validación.** Análisis de convergencia de HLLE **antes** de usarlo en cualquier
comparación. Los casos de Toro contra la solución exacta, con \(L_1\) y \(L_2\).

**Criterio de cierre.** HLLE seleccionable por configuración, con convergencia
verificada, y el número de archivos del núcleo tocados registrado en
`traceability.md`.

**Por qué este hito está tan arriba.** Cierra el hueco de la pregunta de
investigación, es pequeño, y produce la única medición que convierte el `Obj-1`
en un resultado. Si el número no es cero, hay que rediseñar, y es mucho mejor
saberlo ahora.

---

## H5 — Adaptadores de configuración y salida

**Objetivo.** Cumplir los formatos que el anteproyecto declara, sin perder la
capacidad de regresión.

**Prerrequisitos.** H1 resuelto en `C-3` y `C-4`.

**Trabajo.**

- Lector JSON con el esquema de `project_definition.md` §7 (FR-004).
- Escritor CSV para 1D (FR-028).
- Conservar el lector de namelist y el escritor `.xl` (FR-005, FR-030).
- Prueba de equivalencia: el mismo caso en JSON y en namelist produce
  configuraciones efectivas idénticas.

**Componentes afectados.** Solo `config/` e `io/`. Nada numérico.

**Validación.** La regresión sigue usando el camino namelist y `.xl`, y debe
quedar intacta.

**Criterio de cierre.** Los ejemplos están en JSON; los `input.par` archivados
siguen ejecutándose.

**Nota de diseño.** El JSON se escribe sin biblioteca externa (NFR-005). El
esquema es plano y conocido; un analizador de unos cientos de líneas basta, y se
prueba contra entradas mal formadas.

---

## H6 — Matriz de comparación 1D

**Objetivo.** Producir el entregable que responde la pregunta de investigación,
en 1D.

**Prerrequisitos.** H3 y H4. **Convergencia verificada para cada esquema que
entre en la matriz.**

**Trabajo.**

- Script que ejecuta el barrido completo y tabula (FR-035).
- Adaptar `fig_toro.py` y `fig_convergencia.py` de las herramientas del director.
- Figura del detalle del contacto para el caso de contacto casi estacionario.
- Tabla de \(L_1\), \(L_2\) y anchura del contacto en celdas.

**Componentes afectados.** Ninguno del núcleo. Solo `tools/` y `scripts/`.

**Pruebas.** Las herramientas que **calculan métricas** necesitan sus propias
pruebas: producen números que van en la memoria.

**Validación.** Cada celda de la matriz contra la solución exacta.

**Criterio de cierre.** Una tabla y una figura que aíslen el efecto de cambiar el
solver de Riemann manteniendo todo lo demás fijo, con los errores cuantificados.

**Importancia.** A partir de aquí existe un resultado defendible. Todo lo
posterior lo amplía; nada de lo posterior es prerrequisito suyo.

---

## H7 — Migración a Catch2

**Objetivo.** Cumplir §5.2 sin introducir dependencias de red.

**Prerrequisitos.** H1 resuelto en `C-2`.

**Trabajo.** Incorporar Catch2 v2 como cabecera única en `third_party/`. Migrar
los 87 casos con el mapeo de `technology_stack.md` §4. Retirar el arnés propio.

**Componentes afectados.** Solo `tests/`.

**Criterio de cierre.** Los 87 casos pasan bajo Catch2, con el mismo recuento y
el mismo resultado. **Un único arnés en el repositorio.**

**Nota.** Se hace en un solo hito, no gradualmente. Dos arneses conviviendo es
peor que cualquiera de los dos.

---

## H8 — Spike de dimensionalidad

**Objetivo.** Confirmar o refutar la estrategia de generalización **antes** de
escribir 2D.

**Prerrequisitos.** H4 (para que el spike cubra los tres solvers).

**Trabajo.** Lo especificado en `architecture.md` §9.3. Plantillar `FaceState`,
`FaceFlux` y los tres solvers, e instanciar **solo** con `NT = 0`. No se escribe
nada de 2D.

**Duración acotada:** un día.

**Criterio de éxito.** Suite verde **y** regresión sin cambios: 26 de 26 con
pasos idénticos y los 9 casos byte a byte que ya coincidían.

**Criterio de aborto.** Cualquier cambio en la regresión, o revisión de
legibilidad negativa por parte de los dos autores. En ese caso se pasa a la
opción A —implementaciones separadas por dimensión— y se documenta como deuda
técnica consciente con su justificación.

**Por qué instanciar solo con `NT = 0`.** Separa «la plantilla rompió algo» de
«el 2D está mal». Sin esa separación, un fallo posterior tiene dos causas
posibles y cuesta el doble diagnosticarlo.

---

## H9 — Núcleo 2D

**Objetivo.** Malla, campos, fronteras y paso de tiempo en dos dimensiones.

**Prerrequisitos.** H8 cerrado con éxito, o con la decisión de aborto tomada.

**Trabajo.**

- `Mesh2D` (FR-007).
- `CellField2D` con anchura de fantasmas configurable (FR-009).
- Fronteras por borde, con tratamiento correcto de esquinas (FR-013, FR-014).
- Energía cinética multicomponente en la ecuación de estado (FR-010).
- **Segunda fórmula de CFL** para el esquema sin separación (FR-022, `INV-006`).

**Componentes afectados.** `mesh/`, `physics/`, `boundary/`,
`numerical/cfl_controller`.

**Pruebas.** Índices y coordenadas de la malla; las tres fronteras celda a celda
**incluidas las esquinas**; `dt` verificado contra la fórmula sin separación.

**Criterio de cierre.** Se puede construir un estado 2D, rellenar sus fantasmas y
calcular un paso de tiempo. **Todavía no avanza nada.**

**Riesgo principal.** Las esquinas. Son la parte que el código de referencia
resuelve por el orden de llenado —primero periódica en x, después pared en y— y
es fácil equivocarse. Merece una prueba propia y explícita.

---

## H10 — Operador 2D y Lax-Liu

**Objetivo.** Avanzar el fluido en 2D y validar contra el caso de referencia.

**Prerrequisitos.** H9.

**Trabajo.**

- Reconstrucción direccional.
- Empaquetado y desempaquetado de la rotación en el barrido en y.
- Operador de divergencia **sin separación dimensional**: flujos en x e y
  calculados sobre el mismo estado y una sola actualización.
- Caso Lax-Liu configuración 3 (FR-027).
- Caso de explosión cilíndrica, para la prueba de simetría radial.

**Componentes afectados.** `numerical/reconstruction/`,
`numerical/finite_volume_operator`, `numerical/source/` (divergencia 2D),
`initial/`.

**Pruebas.** **Transposición x↔y con u↔v**: el criterio más sensible para la
rotación, verificable a nivel de redondeo. Sod a lo largo de x contra Sod a lo
largo de y. Conservación con fronteras periódicas.

**Validación.** Lax-Liu contra la referencia de la literatura. Simetría radial de
la explosión cilíndrica. Barrido de CFL de 0.45 a 0.95 midiendo el indicador de
tablero de ajedrez, para confirmar `INV-006`.

**Criterio de cierre.** Lax-Liu se ejecuta y se compara. La transposición se
verifica al nivel del redondeo. El barrido de CFL es estable hasta el CFL
declarado.

**Nota.** El esquema **no** usa separación dimensional. Es la estructura del
código de referencia y hay que preservarla: con separación, el criterio de CFL
sería otro y la comparación con su código dejaría de ser posible.

---

## H11 — Salida VTK

**Objetivo.** Visualización 2D en ParaView.

**Prerrequisitos.** H10, `C-4` resuelto.

**Trabajo.** Escritor VTK para malla estructurada (FR-029). Conservar el escritor
`.xyl` de compatibilidad (FR-030).

**Componentes afectados.** Solo `io/`.

**Criterio de cierre.** ParaView abre el archivo sin conversión intermedia.

---

## H12 — WENO

**Objetivo.** Tercer esquema de reconstrucción.

**Prerrequisitos.** H9 (anchura de fantasmas configurable). **Condicional a que
`A-2` confirme que es entregable.**

**Trabajo.** `WenoReconstruction` con indicadores de suavidad y pesos no
lineales. Orden según `C-5`: WENO3 cabe en dos celdas fantasma; WENO5 exige tres
y obliga a revisar las tres fronteras.

**Componentes afectados.** `numerical/reconstruction/`, y si es WENO5, también
`physics/field` y `boundary/` — lo que lo convierte en un cambio del núcleo que
hay que documentar como tal.

**Pruebas.** Orden de convergencia ≥ 3 sobre solución suave. Ausencia de
oscilaciones cerca de discontinuidades.

**Criterio de cierre.** \(p_{obs} \ge 0.9 \times 3\) medido, y el esquema
seleccionable por configuración.

**Nota de planificación.** Es el hito más caro y el único que no toca la pregunta
de investigación, que es sobre solvers de Riemann y no sobre reconstructores. De
ahí su posición. Si el tiempo aprieta, es el primer candidato a pasar a trabajo
futuro, y `A-2` existe precisamente para decidirlo por escrito y a tiempo.

---

## H13 — Validación 2D y matriz 2D

**Objetivo.** Extender el banco de pruebas a dos dimensiones.

**Prerrequisitos.** H10, y H12 si WENO entra.

**Trabajo.** Matriz de comparación en 2D. Adaptar `fig_euler2d.py`, incluida su
métrica de simetría radial. Si `B-2` lo confirma, reflexión de Mach con su
frontera dependiente del tiempo.

**Criterio de cierre.** Tabla y figuras 2D con los errores cuantificados.

---

## H14 — Documentación pedagógica y reproducibilidad

**Objetivo.** Cerrar el `Obj-4`, incluida la actividad de cierre que el
anteproyecto declara no negociable.

**Prerrequisitos.** H6 como mínimo; idealmente H13.

**Trabajo.**

- Manual de instalación.
- Tutoriales progresivos de 1D a 2D, siguiendo el guion de
  `project_definition.md` §9.
- Un archivo de configuración de ejemplo por cada caso de prueba.
- Completar la referencia de interfaces en las cabeceras públicas.
- **Prueba de reproducibilidad desde cero** (NFR-003): instalar, configurar y
  simular siguiendo únicamente la documentación, sin intervención de los autores.

**Criterio de cierre.** La prueba desde cero se ejecuta y su resultado se
documenta, incluidos los puntos donde la documentación resultó insuficiente.

**Nota.** Los tutoriales salen de los casos ya validados. Por eso este hito se
solapa con el final de la validación, como establece la metodología del
anteproyecto (Etapa 4), y no se empieza de cero.

---

## H15 — Cierre de tesis

**Objetivo.** Producir los artefactos de la memoria.

**Trabajo.** Diagrama de componentes en su formato final (`B-1`). Revisión de
`traceability.md` sin filas críticas huérfanas. Figuras y tablas finales, cada
una etiquetada con el commit que la produjo. Revisión cruzada de toda la
documentación.

**Criterio de cierre.** Cada requisito crítico trazable hasta su implementación y
su evidencia. Cada figura publicada asociada a una etiqueta de versión.

---

## Ruta crítica

```
H2 → H3 → H4 → H6        primer resultado publicable
          H4 → H8 → H9 → H10 → H13    dos dimensiones
```

H5, H7, H11 y H12 están fuera de la ruta crítica: se pueden reordenar o diferir
sin retrasar el resultado central.

**El hito de mayor riesgo es H8.** Es el único que puede obligar a cambiar de
estrategia, y está deliberadamente acotado a un día y colocado antes de que
exista código 2D que perder.

**El hito de mayor valor por unidad de esfuerzo es H4.** Cierra el hueco de la
pregunta de investigación y produce la medición del `Obj-1` en, probablemente,
menos de un día de trabajo.
