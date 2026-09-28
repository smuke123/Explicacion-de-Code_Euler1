# Especificación de requisitos

**Fecha:** 27 de septiembre de 2026
**Convención de etiquetas:** ver `project_definition.md` §0

## Cómo leer este documento

Cada requisito tiene fuente trazable. Las fuentes válidas son:

| Fuente | Significado |
|---|---|
| `Obj-1` … `Obj-4` | Objetivo específico de la tesis |
| `§x.y` | Sección del marco teórico del anteproyecto |
| `Derivado` | Consecuencia necesaria de otro requisito. Se indica de cuál |
| `Referencia` | Comportamiento del solver Fortran que hay que reproducir para poder validar |

**No hay requisitos cuya fuente sea «conveniente».** Si algo no se puede trazar a
una de las cuatro fuentes, no está aquí.

Los estados posibles son `Implementado`, `Parcial`, `Pendiente` y
`Bloqueado`. «Implementado» significa que existe en 1D y está cubierto por la
suite de pruebas; se indica entre paréntesis cuando falta la parte 2D.

---

# Requisitos funcionales

## Configuración

### FR-001 — Configuración desde archivo externo
**Descripción.** El sistema lee todos los parámetros de simulación de un archivo
externo, sin recompilar.
**Prioridad.** Crítica · **Fuente.** `Obj-1` · **Estado.** Implementado (namelist)
**Aceptación.** Cambiar reconstructor, limitador, solver, integrador, resolución,
dominio, CFL, caso y tiempo final se hace editando solo el archivo. El binario no
se recompila.
**Depende de.** —

### FR-002 — Validación de la configuración
**Descripción.** Todo parámetro se valida antes de asignar memoria o iniciar el
cálculo. Un valor inválido aborta con mensaje que nombra la clave y el valor.
**Prioridad.** Crítica · **Fuente.** `Referencia` (auditoría del Fortran) · **Estado.** Implementado
**Aceptación.** `gamma <= 1`, `N < 4`, `cfl <= 0` o `cfl > 1`, caso fuera de
rango, solver o limitador desconocido: todos abortan con código 1 y mensaje
identificable. Ninguno llega a asignar memoria.
**Depende de.** FR-001

### FR-003 — Clave desconocida es error
**Descripción.** Una clave no reconocida en el archivo de configuración aborta la
ejecución. No se ignora ni se advierte.
**Prioridad.** Crítica · **Fuente.** `Referencia` · **Estado.** Implementado
**Aceptación.** Un archivo con `resolucion = 400` en lugar de `N = 400` aborta
con código 1 nombrando `resolucion`.
**Justificación.** Es el hallazgo más costoso de la auditoría del proyecto
Fortran: una errata de un carácter ejecutaba otra malla en silencio y alteraba
una tabla de convergencia sin ninguna señal.
**Depende de.** FR-001

### FR-004 — Formato JSON de configuración
**Descripción.** El formato principal de configuración es JSON, con el esquema
especificado en `project_definition.md` §7.
**Prioridad.** Alta · **Fuente.** `§4.4` · **Estado.** Pendiente
**Aceptación.** Un archivo JSON válido produce la misma configuración efectiva
que el namelist equivalente, verificado por test.
**Depende de.** FR-001 · **Punto abierto.** `C-3`

### FR-005 — Lector de namelist de compatibilidad
**Descripción.** Se conserva la capacidad de leer el namelist de Fortran, para
poder ejecutar los mismos archivos de entrada que el solver de referencia.
**Prioridad.** Alta · **Fuente.** `Derivado` de FR-023 (regresión) · **Estado.** Implementado
**Aceptación.** Los `input.par` de `tests/reference/` se leen sin modificación.
**Depende de.** FR-001

### FR-006 — Configuración efectiva inspeccionable
**Descripción.** El sistema puede imprimir la configuración resuelta (después de
que el caso de prueba aplique sus valores por defecto e imposiciones) sin
ejecutar la simulación.
**Prioridad.** Media · **Fuente.** `Derivado` de FR-008 · **Estado.** Pendiente
**Aceptación.** `--dry-run` imprime la configuración resuelta y devuelve 0 sin
escribir archivos de salida ni avanzar el tiempo.
**Depende de.** FR-001, FR-008

## Malla

### FR-007 — Malla estructurada uniforme 1D y 2D
**Descripción.** Malla cartesiana uniforme, con celdas fantasma, en una y dos
dimensiones.
**Prioridad.** Crítica · **Fuente.** `Obj-2` · **Estado.** Parcial (solo 1D)
**Aceptación.** Centros de celda en \(x_i = x_{min} + (i - 1/2)\Delta x\); el
número de celdas fantasma cubre el stencil más ancho en uso.
**Depende de.** FR-001

### FR-008 — Casos de prueba con valores por defecto declarativos
**Descripción.** Cada caso de prueba declara su dominio, tiempo final, frontera y
estados iniciales. Los valores que el usuario haya especificado explícitamente
**no** se sobreescriben; los que el caso imponga por necesidad física se aplican
y se reportan.
**Prioridad.** Alta · **Fuente.** `Referencia` · **Estado.** Implementado
**Aceptación.** Si el usuario da `final_time`, el caso no lo pisa. Si el caso
requiere frontera reflectiva y el usuario pidió transmisiva, se aplica la
reflectiva y se emite aviso nombrando el conflicto.
**Depende de.** FR-002

### FR-009 — Celdas fantasma de anchura configurable
**Descripción.** La anchura de la zona fantasma es un parámetro del sistema, no
una constante fija.
**Prioridad.** Alta · **Fuente.** `Derivado` de FR-015 (WENO) · **Estado.** Pendiente
**Aceptación.** Con MUSCL la anchura es 2; con WENO5 es 3; las tres condiciones
de frontera rellenan correctamente la anchura activa.
**Justificación.** Hoy la anchura es la constante `kGhostCells = 2`. WENO5
necesita 3, y las fronteras están escritas suponiendo exactamente 2.
**Depende de.** FR-007, FR-011

## Estado y variables

### FR-010 — Variables conservadas y primitivas
**Descripción.** El estado se mantiene en forma conservada \((\rho, \rho u, E)\)
en 1D y \((\rho, \rho u, \rho v, E)\) en 2D, con la representación primitiva
\((\rho, u, p)\) / \((\rho, u, v, p)\) sincronizada mediante la ecuación de
estado.
**Prioridad.** Crítica · **Fuente.** `Obj-2`, `§1.1` · **Estado.** Parcial (solo 1D)
**Aceptación.** La conversión ida y vuelta reproduce el estado original dentro de
error de redondeo. La sincronización se hace en un único punto del código.
**Depende de.** FR-007

### FR-011 — Pisos de positividad con contador
**Descripción.** Cuando la densidad o la presión calculadas caen por debajo de un
piso, se recortan al piso y **se cuenta el evento**. Al final de la ejecución se
informa el total si es distinto de cero.
**Prioridad.** Alta · **Fuente.** `Referencia`, `§6.1` · **Estado.** Implementado
**Aceptación.** Con el piso en su valor real, los casos estándar reportan cero
activaciones. Elevando el piso artificialmente, el contador crece.
**Justificación.** Cuando el piso dispara, la representación primitiva deja de
ser la imagen de la conservada, y sin contador nada lo indica.
**Depende de.** FR-010

### FR-012 — Detección de estados no finitos
**Descripción.** La aparición de `NaN` o `Inf` en el campo se detecta y aborta la
ejecución con código 2, en lugar de propagarse.
**Prioridad.** Crítica · **Fuente.** `Referencia` · **Estado.** Implementado
**Aceptación.** Un campo con un `NaN` inyectado aborta en el cálculo de `dt` con
mensaje explícito.
**Justificación.** `std::max` y `max()` de Fortran absorben `NaN` silenciosamente
según el orden de los argumentos.
**Depende de.** FR-010

## Condiciones de frontera

### FR-013 — Tres condiciones de frontera
**Descripción.** Transmisiva, reflectiva y periódica, en 1D y 2D.
**Prioridad.** Crítica · **Fuente.** `Obj-2`, `§2.3` · **Estado.** Parcial (solo 1D)
**Aceptación.** Transmisiva copia el interior; reflectiva copia escalares e
invierte la velocidad normal; periódica copia del extremo opuesto. Verificado
celda a celda con igualdad exacta.
**Depende de.** FR-007, FR-009

### FR-014 — Condición de frontera por borde
**Descripción.** En 2D, cada borde del dominio admite una condición distinta.
**Prioridad.** Alta · **Fuente.** `Referencia` (el modo `rt` del Fortran 2D
combina periódica en x con pared en y) · **Estado.** Pendiente
**Aceptación.** Una configuración con periódica en x y reflectiva en y produce
celdas fantasma correctas, incluidas las esquinas.
**Depende de.** FR-013

## Reconstrucción y limitadores

### FR-015 — Tres esquemas de reconstrucción
**Descripción.** Orden 1 constante, orden 2 MUSCL, orden 3+ WENO. Cada uno es un
módulo concreto independiente que implementa la interfaz de reconstrucción.
**Prioridad.** Crítica (orden 1, MUSCL) / Por confirmar (WENO) · **Fuente.** `Obj-2`, `§2.4` · **Estado.** Parcial
**Aceptación.** Cada esquema es seleccionable por configuración. El orden de
convergencia observado se aproxima al teórico: ≈1, ≈2 y ≈3 respectivamente.
**Estado detallado.** MUSCL implementado. El orden 1 existe hoy como *opción de
limitador* dentro de MUSCL, no como módulo separado; el objetivo pide módulos
concretos. WENO pendiente.
**Depende de.** FR-010 · **Punto abierto.** `A-2`, `C-5`

### FR-016 — Limitadores de pendiente
**Descripción.** minmod, van Leer y superbee como mínimo, seleccionables por
configuración, aplicables a la reconstrucción MUSCL.
**Prioridad.** Alta · **Fuente.** `Obj-2`, `§2.5` · **Estado.** Implementado
**Aceptación.** Los cinco del solver de referencia (`fo`, `mm`, `vl`, `mc`, `sb`)
producen resultados idénticos a los del Fortran para la misma configuración.
**Depende de.** FR-015

## Solvers de Riemann

### FR-017 — Solver HLL
**Descripción.** Flujo numérico de dos ondas con estimación de velocidades de
Davis.
**Prioridad.** Alta · **Fuente.** `§3.1`, `Referencia` · **Estado.** Implementado
**Aceptación.** Consistencia: con \(W_L = W_R\) devuelve el flujo físico.
Upwind supersónico: con \(S_L \ge 0\) devuelve \(F_L\) exactamente.
**Depende de.** FR-010

### FR-018 — Solver HLLE
**Descripción.** Flujo numérico de dos ondas con estimación de velocidades de
Einfeldt, basada en promedios de Roe de las variables primitivas.
**Prioridad.** Crítica · **Fuente.** `Obj-2`, pregunta de investigación, `§3.2` · **Estado.** Pendiente
**Aceptación.** Las mismas dos propiedades de FR-017, más: sus velocidades de
onda difieren de las de HLL en al menos un caso de prueba de forma medible.
**Justificación.** El solver de referencia implementa HLL con velocidades de
Davis, **no HLLE**. No existe en ningún material entregado y debe implementarse
desde cero. Es el centro de la pregunta de investigación.
**Depende de.** FR-010

### FR-019 — Solver HLLC
**Descripción.** Flujo numérico de tres ondas, con la velocidad del contacto
\(S^*\) y dos subestados intermedios.
**Prioridad.** Crítica · **Fuente.** `Obj-2`, `§3.3` · **Estado.** Implementado
**Aceptación.** Las dos propiedades de FR-017, más: resuelve la discontinuidad de
contacto en menos celdas que HLL sobre el mismo problema y malla.
**Depende de.** FR-010

### FR-020 — Solver direccional único
**Descripción.** El mismo código de cada solver de Riemann atiende 1D y 2D,
operando sobre un estado con velocidad normal y velocidades tangenciales
pasivas.
**Prioridad.** Crítica · **Fuente.** `Derivado` de `Obj-1` · **Estado.** Pendiente
**Aceptación.** No existe una segunda implementación de HLL, HLLE ni HLLC para
2D. Añadir un solver nuevo lo habilita en ambas dimensiones a la vez.
**Justificación.** Si cada solver existiera dos veces, «añadir HLLE» sería
escribirlo dos veces, lo que contradice la afirmación de arquitectura que la
tesis debe demostrar.
**Depende de.** FR-017, FR-018, FR-019 · **Ver.** `architecture.md` §Dimensionalidad

## Integración temporal

### FR-021 — Integrador explícito de orden ≥ 2
**Descripción.** Runge-Kutta explícito, con al menos un esquema de orden 2.
**Prioridad.** Crítica · **Fuente.** `Obj-2`, `§2.6` · **Estado.** Implementado
**Aceptación.** SSP-RK2 reproduce el resultado del Fortran paso a paso. Los
arreglos de etapa están separados: ninguna etapa lee y escribe el mismo arreglo.
**Depende de.** FR-010

### FR-022 — Paso de tiempo adaptativo por CFL
**Descripción.** El paso se calcula en cada iteración a partir de la velocidad
máxima de propagación.
**Prioridad.** Crítica · **Fuente.** `Obj-2`, `§2.6` · **Estado.** Parcial (solo 1D)
**Aceptación.** En 1D, \(\Delta t = C\,\Delta x / \max(|u| + a)\). En 2D sin
separación dimensional, \(\Delta t = C / (s_x/\Delta x + s_y/\Delta y)\) con
máximos **por dirección**.
**Justificación crítica.** Las dos fórmulas son distintas y **no** se pueden
unificar. La auditoría del solver 2D de referencia demostró que aplicar el
criterio 1D en 2D sin separación permite CFL efectivos superiores a 0.5 y produce
corrupción en patrón de tablero de ajedrez de forma silenciosa a partir de
CFL ≈ 0.75, porque los pisos de positividad enmascaran el `NaN`.
**Depende de.** FR-010, FR-012 · **Ver.** `numerical_invariants.md` `INV-006`

## Términos fuente

### FR-023 — Términos fuente aditivos separados del flujo
**Descripción.** Los términos fuente se acumulan sobre la actualización ya
ensamblada, sin modificar el cálculo de flujos.
**Prioridad.** Media · **Fuente.** `Referencia` · **Estado.** Implementado
**Aceptación.** Activar o desactivar un término fuente no cambia ni una línea del
solver de Riemann ni de la reconstrucción.
**Nota de alcance.** La gravedad bien balanceada y la geometría radial están
implementadas por herencia de la migración, pero **no aparecen en ningún
objetivo**. Se conservan como regresión, no se amplían.
**Depende de.** FR-010

## Solución de referencia

### FR-024 — Solver exacto del problema de Riemann
**Descripción.** Solución semianalítica del problema de Riemann, usada
exclusivamente como referencia de verificación.
**Prioridad.** Crítica · **Fuente.** `Obj-3`, `§1.3`, `§6` · **Estado.** Implementado
**Aceptación.** Los valores \(p^*\) y \(u^*\) coinciden con la Tabla 4.3 de Toro
en las cinco configuraciones publicadas, con error relativo por debajo de
\(10^{-5}\).
**Restricción.** No participa nunca en el avance temporal.
**Depende de.** FR-010

### FR-025 — Solución de referencia muestreada en la malla
**Descripción.** La solución de referencia se evalúa en los centros de celda de la
malla de la simulación, para permitir comparación punto a punto.
**Prioridad.** Crítica · **Fuente.** `Derivado` de FR-028 · **Estado.** Implementado
**Aceptación.** El archivo de referencia tiene exactamente el mismo número de
puntos y las mismas coordenadas que el de la simulación.
**Depende de.** FR-024, FR-007

## Casos de prueba

### FR-026 — Casos de Riemann estándar 1D
**Descripción.** El framework incluye los casos de verificación 1D con solución
analítica conocida.
**Prioridad.** Crítica · **Fuente.** `Obj-3`, `§6.1` · **Estado.** Parcial
**Aceptación.** Están disponibles y documentados: Sod clásico, Lax, problema 123
(dos rarefacciones) y doble choque.
**Estado detallado.** El problema 123 y el doble choque existen (casos 2 y 4 de
Toro). **Faltan el Sod clásico** —el caso 1 del solver de referencia es el Sod
*modificado* de Toro, con \(u_L = 0.75\)— **y el de Lax**.
**Depende de.** FR-008, FR-024

### FR-027 — Caso de referencia 2D
**Descripción.** Al menos un problema de referencia bidimensional.
**Prioridad.** Crítica · **Fuente.** `Obj-3`, `§6.2` · **Estado.** Pendiente
**Aceptación.** El problema de Riemann 2D de Lax-Liu se ejecuta y se compara con
la referencia documentada en la literatura.
**Punto abierto.** `B-2` (¿reflexión de Mach comprometida u opcional?), `B-3`
(¿cuántas configuraciones de Lax-Liu?)
**Depende de.** FR-007, FR-013, FR-020

## Salida

### FR-028 — Salida en formato de texto para 1D
**Descripción.** Los resultados 1D se exportan en CSV o texto plano procesable
con Python y matplotlib.
**Prioridad.** Alta · **Fuente.** `§5.3` · **Estado.** Parcial (formato `.xl` heredado)
**Aceptación.** Un script de Python de menos de 20 líneas lee el archivo y lo
grafica sin lógica de parseo especial.
**Depende de.** FR-010 · **Punto abierto.** `C-4`

### FR-029 — Salida en formato VTK para 2D
**Descripción.** Los resultados 2D se exportan en un formato legible por
ParaView.
**Prioridad.** Alta · **Fuente.** `§5.3` · **Estado.** Pendiente
**Aceptación.** ParaView abre el archivo y muestra el campo sin conversión
intermedia.
**Depende de.** FR-007 · **Punto abierto.** `C-4`

### FR-030 — Salida de compatibilidad con el solver de referencia
**Descripción.** Se conserva la escritura en el formato del Fortran (`.xl` en 1D,
`.xyl` en 2D), con semántica byte a byte idéntica.
**Prioridad.** Alta · **Fuente.** `Derivado` de FR-032 · **Estado.** Implementado (1D)
**Aceptación.** El archivo producido es idéntico byte a byte al del Fortran para
la misma configuración, incluidos el descriptor de formato con exponente de tres
dígitos y los terminadores de línea.
**Depende de.** FR-028

### FR-031 — Registro de ejecución reproducible
**Descripción.** Cada corrida escribe un registro con la configuración efectiva,
la versión, el hash de commit, el compilador y sus opciones.
**Prioridad.** Alta · **Fuente.** `§5.4`, `Obj-4` · **Estado.** Parcial
**Aceptación.** A partir del registro y del repositorio, un tercero reproduce la
corrida sin información adicional.
**Depende de.** FR-001

## Validación

### FR-032 — Regresión contra el solver de referencia
**Descripción.** Comparación automatizada de la salida del framework contra
salidas archivadas del solver Fortran.
**Prioridad.** Crítica · **Fuente.** `Derivado` de `Obj-3` · **Estado.** Implementado (1D)
**Aceptación.** Para cada configuración archivada, todas las variables dentro de
tolerancia **y el número de pasos temporales idéntico**.
**Estado actual medido.** 26 de 26 configuraciones dentro de tolerancia con los
pasos coincidiendo exactamente; 9 de 13 casos archivados idénticos byte a byte.
**Depende de.** FR-030 · **Punto abierto.** `B-5` (tolerancia aceptable)

### FR-033 — Métricas de error L1 y L2
**Descripción.** Cálculo de \(\max|diff|\), \(L_1\), \(L_2\) y error relativo
entre dos campos.
**Prioridad.** Crítica · **Fuente.** `Obj-3`, `§6.3` · **Estado.** Implementado
**Aceptación.** Sobre campos construidos con diferencia conocida, las cuatro
métricas coinciden con el valor analítico dentro de error de redondeo.
**Depende de.** FR-025

### FR-034 — Análisis de orden de convergencia
**Descripción.** Barrido de resoluciones y estimación del orden observado
\(p_{obs} = \log_2(\|e(\Delta x)\| / \|e(\Delta x/2)\|)\).
**Prioridad.** Crítica · **Fuente.** `Obj-3`, `§6.4` · **Estado.** Parcial
**Aceptación.** Sobre una solución suave, \(p_{obs} \ge 0.9\,p_{teórico}\) para
cada esquema de reconstrucción, con al menos cuatro resoluciones.
**Depende de.** FR-033, FR-015 · **Punto abierto.** `B-4`

### FR-035 — Matriz de comparación de esquemas
**Descripción.** Ejecución sistemática del mismo problema variando **una sola**
elección algorítmica, con resultados tabulados.
**Prioridad.** Crítica · **Fuente.** `Obj-3`, `§3.4`, `§6.4` · **Estado.** Pendiente
**Aceptación.** Una tabla que, para un caso fijo y una malla fija, reporte
\(L_1\) y \(L_2\) para cada combinación de reconstructor y solver, y una figura
que muestre la diferencia visible en la discontinuidad de contacto.
**Justificación.** Es el entregable que responde la pregunta de investigación.
**Depende de.** FR-033, FR-017, FR-018, FR-019, FR-015 · **Punto abierto.** `B-4`

### FR-036 — Verificación de propiedades físicas
**Descripción.** Comprobación automatizada de conservación global y positividad.
**Prioridad.** Alta · **Fuente.** `§1.1`, `§2.1` · **Estado.** Implementado (1D)
**Aceptación.** Con fronteras periódicas, la masa y la energía totales se
conservan con error relativo por debajo de \(10^{-12}\). La densidad y la presión
son estrictamente positivas en todos los casos.
**Depende de.** FR-013, FR-021

## Extensibilidad

### FR-037 — Añadir un solver de Riemann sin modificar el núcleo
**Descripción.** Incorporar un solver nuevo requiere una clase nueva y su
registro en la construcción; no modifica el operador de volúmenes finitos.
**Prioridad.** Crítica · **Fuente.** `Obj-1` · **Estado.** Implementado, **sin medir**
**Aceptación.** Al añadir HLLE (FR-018) se cuenta y se reporta el número de
archivos del núcleo modificados. El criterio de éxito es **cero**.
**Justificación.** Es la forma de convertir la afirmación arquitectónica del
Objetivo 1 en un resultado medible en lugar de argumentado.
**Depende de.** FR-018

### FR-038 — Añadir un caso de prueba sin modificar el núcleo
**Descripción.** Un caso nuevo se declara como dato, sin tocar el solver.
**Prioridad.** Alta · **Fuente.** `Obj-1`, `Obj-4` · **Estado.** Implementado
**Aceptación.** Documentado en `extending.md`; verificado añadiendo un caso de
prueba de ejemplo.
**Depende de.** FR-008

---

# Requisitos no funcionales

### NFR-001 — Corrección numérica frente a la referencia
**Descripción.** El framework reproduce el comportamiento numérico del solver
Fortran de referencia para las configuraciones que ambos admiten.
**Razón.** Es la base de credibilidad del trabajo: sin ella, cualquier
comparación entre esquemas es indistinguible de un error de implementación.
**Aceptación.** \(\max|diff| \le 10^{-12} + 10^{-9}\max|ref|\) en todas las
variables, y número de pasos temporales idéntico.
**Verificación.** Suite de regresión ejecutada en integración continua.
**Prioridad.** Crítica · **Punto abierto.** `B-5`

### NFR-002 — Determinismo
**Descripción.** Dos ejecuciones con la misma configuración y el mismo binario
producen salidas idénticas byte a byte.
**Razón.** Sin determinismo, la regresión no distingue un cambio real de ruido.
**Aceptación.** Ejecutar dos veces y comparar los archivos de salida, excluyendo
el tiempo de pared del registro de ejecución.
**Verificación.** Test automatizado.
**Prioridad.** Crítica

### NFR-003 — Reproducibilidad por terceros
**Descripción.** Un tercero reproduce cualquier resultado reportado partiendo del
repositorio público y el archivo de configuración.
**Razón.** `§5.4` lo declara «componente no negociable», y responde al problema
de irreproducibilidad de la justificación.
**Aceptación.** Instalación, configuración y ejecución desde cero siguiendo solo
la documentación de usuario, sin intervención de los autores. Se ejecuta y se
documenta el resultado.
**Verificación.** Actividad de cierre del `Obj-4`.
**Prioridad.** Crítica

### NFR-004 — Reproducibilidad de la compilación
**Descripción.** La compilación produce el mismo binario funcional en Linux,
macOS y Windows con los compiladores soportados.
**Razón.** `§5.1` declara multiplataforma.
**Aceptación.** La suite completa pasa en GCC y Clang. El entorno primario
declarado se verifica en cada commit.
**Verificación.** Integración continua.
**Prioridad.** Alta

### NFR-005 — Sin dependencias externas obligatorias para el núcleo
**Descripción.** Compilar y ejecutar el solver y su suite de pruebas no requiere
descargar nada más que un compilador y CMake.
**Razón.** Reduce la barrera de adopción, que es un objetivo pedagógico
explícito, y elimina una clase entera de fallos de reproducibilidad.
**Aceptación.** `cmake` y `cmake --build` funcionan sin red.
**Verificación.** Compilación en un entorno sin red.
**Prioridad.** Alta · **Nota.** En tensión con el punto abierto `C-2` (Catch2).
Ver `ADR-004`.

### NFR-006 — Ausencia de estado global mutable
**Descripción.** Ninguna variable de configuración o de estado físico es global.
**Razón.** El acoplamiento implícito por estado global es el defecto estructural
del código de referencia y la causa directa de su fallo más grave: una subrutina
que recibía un argumento pero leía un arreglo global distinto.
**Aceptación.** Inspección: no existen variables no `const` a nivel de namespace.
**Verificación.** Revisión de código y búsqueda automatizada.
**Prioridad.** Crítica

### NFR-007 — Coste de abstracción acotado en los bucles numéricos
**Descripción.** No hay llamadas virtuales ni comparaciones de cadenas dentro de
bucles celda a celda.
**Razón.** El código de referencia hacía `select case (trim(limiter))` por celda
y por componente. La abstracción debe pagarse una vez por barrido, no por celda.
**Aceptación.** Como máximo una llamada virtual por barrido y por etapa de
Runge-Kutta. Los limitadores se resuelven en tiempo de compilación.
**Verificación.** Inspección de las firmas de las interfaces: el método virtual
recibe un barrido completo, no una celda.
**Prioridad.** Alta

### NFR-008 — Legibilidad del código numérico
**Descripción.** Cada fórmula del documento de métodos numéricos se localiza en
el código, y la correspondencia es directa.
**Razón.** Es el objetivo pedagógico: cerrar la brecha entre la ecuación del
pizarrón y su ejecución. Es también el diagnóstico que justifica el proyecto.
**Aceptación.** Cada solver de Riemann expone una función de una sola cara,
legible, además del barrido optimizado. Las variables de las fórmulas usan la
notación del texto de referencia.
**Verificación.** Revisión cruzada entre `numerical_methods.md` y el código.
**Prioridad.** Alta

### NFR-009 — Testabilidad de componentes en aislamiento
**Descripción.** Cada componente numérico se puede verificar sin construir una
simulación completa.
**Razón.** Localiza los fallos en el módulo que los origina, que es la estrategia
declarada en la metodología.
**Aceptación.** Los solvers de Riemann, los limitadores, la ecuación de estado,
las fronteras y el solver exacto tienen pruebas que no instancian `Simulation`.
**Verificación.** Inventario en `testing.md`.
**Prioridad.** Crítica

### NFR-010 — Cobertura de la suite de pruebas
**Descripción.** Todo componente numérico y todo requisito funcional crítico
tiene al menos una prueba automatizada asociada.
**Razón.** Es la evidencia de verificación que el evaluador de ingeniería de
sistemas va a buscar.
**Aceptación.** La tabla de trazabilidad no tiene ninguna fila crítica sin
prueba.
**Verificación.** `traceability.md`, revisado en cada hito.
**Prioridad.** Crítica

### NFR-011 — Manejo explícito de errores
**Descripción.** Ningún error se ignora en silencio. Los errores de configuración
y los numéricos se distinguen entre sí.
**Razón.** Los cuatro hallazgos más graves de la auditoría del código de
referencia eran fallos silenciosos.
**Aceptación.** Códigos de retorno distintos para error de configuración (1),
error numérico (2) y error de entrada/salida (3). Ningún `catch` vacío.
**Verificación.** Pruebas que provocan cada clase de error y comprueban el
código de retorno.
**Prioridad.** Crítica

### NFR-012 — Compilación sin advertencias
**Descripción.** El código compila limpio con el conjunto de advertencias
declarado.
**Razón.** Varias de las advertencias activadas detectan exactamente los errores
que más importan aquí: conversiones con pérdida y comparaciones de signo mixto
en indexación de mallas.
**Aceptación.** Cero advertencias con `-Wall -Wextra -Wpedantic -Wshadow
-Wconversion -Wsign-conversion`.
**Verificación.** Integración continua con advertencias como error.
**Prioridad.** Alta · **Estado.** Cumplido hoy.

### NFR-013 — Aserciones de rango en compilación de depuración
**Descripción.** El acceso a los campos verifica los índices en configuración de
depuración y no paga nada en configuración optimizada.
**Razón.** El error más probable al generalizar a 2D es indexar mal las celdas
fantasma.
**Aceptación.** La suite pasa en `Debug` con las aserciones activas y en
`Release` sin ellas.
**Verificación.** Ambas configuraciones en integración continua.
**Prioridad.** Alta · **Estado.** Cumplido hoy.

### NFR-014 — Uso de memoria acotado y sin asignaciones en el bucle
**Descripción.** Toda la memoria de trabajo se asigna una vez al iniciar. El
bucle temporal no asigna ni libera.
**Razón.** Es un requisito de previsibilidad más que de velocidad: las
asignaciones en el bucle introducen variabilidad y dificultan razonar sobre el
coste.
**Aceptación.** Los arreglos de etapa y los de flujo son miembros de sus
componentes, no variables locales del bucle. Verificable por inspección.
**Verificación.** Revisión de código; opcionalmente contador de asignaciones en
una compilación instrumentada.
**Prioridad.** Media

### NFR-015 — Rendimiento suficiente para 2D de validación
**Descripción.** Una simulación 2D de validación termina en un tiempo que permita
iterar.
**Razón.** `§4.3` pide «tiempo de ejecución razonable para fines de validación y
experimentación». No es un requisito de alto rendimiento.
**Aceptación.** `PENDING INPUT FROM DIRECTOR` — hace falta fijar un número.
Propuesta: Lax-Liu en malla \(400^2\) hasta \(t_f\) en menos de 10 minutos en un
solo núcleo del equipo de desarrollo.
**Verificación.** Medición registrada en `performance.md`.
**Prioridad.** Media

### NFR-016 — Arquitectura preparada para paralelización
**Descripción.** Los bucles sobre celdas están aislados, sin dependencias entre
iteraciones ni acumuladores compartidos sin reducción, y documentados como
paralelizables.
**Razón.** Los dos documentos de objetivos sitúan la paralelización en trabajo
futuro, pero exigen que la arquitectura no obligue a reestructurar.
**Aceptación.** `performance.md` enumera cada bucle candidato, su patrón de
acceso y las reducciones que necesitaría. **No se entrega código paralelo.**
**Verificación.** Revisión del documento contra el código.
**Prioridad.** Media
**Nota.** El solver 2D de referencia contiene una carrera de datos real en el
contador de pisos de positividad, incrementado dentro de una región paralela sin
reducción. Está documentada en `inconsistencies.md` y **no debe reproducirse**.

### NFR-017 — Documentación dual
**Descripción.** Documentación técnica para desarrolladores y documentación
pedagógica para usuarios, ambas mantenidas.
**Razón.** `Obj-4`.
**Aceptación.** Un desarrollador externo añade un solver siguiendo solo la guía
técnica. Un estudiante instala y reproduce un caso siguiendo solo el manual de
usuario.
**Verificación.** Prueba de reproducibilidad de NFR-003, y revisión cruzada entre
los dos autores.
**Prioridad.** Crítica

---

## Resumen de estado

| Categoría | Implementado | Parcial | Pendiente |
|---|---|---|---|
| Configuración | FR-001, 002, 003, 005 | — | FR-004, 006 |
| Malla y estado | FR-008, 011, 012 | FR-007, 010 | FR-009 |
| Fronteras | — | FR-013 | FR-014 |
| Reconstrucción | FR-016 | FR-015 | — |
| Riemann | FR-017, 019 | — | FR-018, 020 |
| Tiempo | FR-021 | FR-022 | — |
| Fuentes | FR-023 | — | — |
| Referencia | FR-024, 025 | — | — |
| Casos de prueba | — | FR-026 | FR-027 |
| Salida | FR-030 | FR-028, 031 | FR-029 |
| Validación | FR-032, 033, 036 | FR-034 | FR-035 |
| Extensibilidad | FR-037, 038 | — | — |

Los requisitos **críticos pendientes** son FR-018 (HLLE), FR-020 (solver
direccional único), FR-027 (caso 2D) y FR-035 (matriz de comparación). Los cuatro
están en la ruta crítica del `roadmap.md`.
