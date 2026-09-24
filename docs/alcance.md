# Especificación de alcance

**Estado:** borrador para revisión del director
**Fecha:** 23 de septiembre de 2026
**Propósito:** fijar por escrito qué se entrega, con qué criterios se considera
terminado, y qué queda explícitamente fuera.

Este documento no propone cambiar los objetivos. Los traduce a criterios
verificables y señala los puntos donde los documentos existentes no coinciden o
no alcanzan a determinar una decisión de implementación. **Lo que se pide del
director es una revisión: confirmar, corregir o descartar.** Cada punto abierto
está numerado para poder responderlo en una línea.

---

## 1. El ancla: la pregunta de investigación

> ¿Cómo diseñar un framework computacional basado en el método de volúmenes
> finitos que permita la comparación y validación de distintos métodos, como los
> solvers de Riemann HLLE y HLLC?

Todo lo demás en este documento existe para poder responder esa pregunta con
evidencia. El criterio para decidir si algo entra o sale del alcance es si
contribuye a responderla.

De ahí se derivan dos afirmaciones que el trabajo debe demostrar, y conviene
tenerlas separadas porque se prueban de formas distintas:

- **Afirmación de arquitectura.** Se puede intercambiar un componente numérico
  sin modificar el núcleo. Se demuestra *haciéndolo* y midiendo el impacto:
  cuántos archivos del núcleo hubo que tocar para añadir un solver nuevo.
- **Afirmación numérica.** El framework calcula correctamente, de modo que las
  diferencias observadas entre esquemas son del método y no de un error de
  implementación. Se demuestra con orden de convergencia medido y con
  comparación contra solución exacta.

La segunda es requisito de la primera: comparar dos implementaciones sin
verificar antes que ambas son correctas no demuestra nada.

---

## 2. Punto abierto crítico: los dos documentos de objetivos difieren

Existen dos redacciones de los objetivos específicos y **no exigen lo mismo**.
Las diferencias no son de estilo; cambian la cantidad de trabajo.

| | Presentación del tema | Anteproyecto | Impacto |
|---|---|---|---|
| Reconstructores | «al menos **dos** reconstructores» | «**tres**: orden 1 constante, orden 2 MUSCL y **orden 3+ WENO**» | WENO es la mayor adición técnica del proyecto |
| Solvers de Riemann | «**dos** solvers de Riemann» (sin nombrar) | «al menos dos: **HLLE y HLLC**» | Obliga a implementar HLLE, que no existe en el código Fortran de referencia |
| Casos 2D | «problemas de Riemann estandarizados en 1D **y 2D**» | «**al menos un caso** en 2D» | El anteproyecto es *menos* exigente aquí |
| Métricas de error | «métricas de error» | «normas **L1 y L2**» | El anteproyecto concreta |
| Caso base 1D | no se nombra | «incluyendo el **problema de Sod** como caso de referencia base» | El anteproyecto concreta |

**(A-1)** ¿Cuál de los dos documentos rige? Se asume que el anteproyecto, por ser
posterior y más detallado. Confirmar.

**(A-2)** ¿WENO es entregable comprometido o queda como trabajo futuro? Es la
diferencia de alcance más grande entre las dos versiones. Si entra, ver el punto
abierto (C-2) sobre el orden.

---

## 3. Alcance incluido, con criterios de aceptación

Los criterios numéricos son **propuestas**. Están puestos para que se puedan
aprobar o corregir con un número, no para darlos por acordados.

### Objetivo 1 — Arquitectura modular

| Entregable | Criterio de aceptación propuesto |
|---|---|
| Interfaces abstractas | Existen `Mesh`, `Reconstruction`, `RiemannSolver`, `TimeIntegrator` como clases abstractas con contrato documentado |
| Desacoplamiento real | Añadir un solver de Riemann nuevo no modifica ningún archivo del núcleo de volúmenes finitos. **Se mide y se reporta**: número de archivos del núcleo tocados = 0 |
| Sin estado global | Ninguna variable de configuración o de estado físico es global; todo se pasa explícitamente |
| Diagrama de componentes | Un diagrama con módulos, responsabilidades y contratos de interfaz |
| Configuración sin recompilar | Cambiar reconstructor, limitador, solver, integrador, malla y caso de prueba se hace solo editando el archivo de configuración |

**(B-1)** El diagrama de componentes: ¿formato concreto esperado (UML, C4, libre)?

### Objetivo 2 — Núcleo computacional

| Componente | Alcance | Criterio |
|---|---|---|
| Malla | Estructurada, uniforme, cartesiana, 1D y 2D, con celdas fantasma | Acceso con índices claros; el número de fantasmas se ajusta al stencil más ancho en uso |
| Formulación | Euler conservativas, gas ideal politrópico | Conservación discreta verificable: el flujo que sale de una celda es exactamente el que entra a la vecina |
| Reconstructores | Orden 1 constante, orden 2 MUSCL, orden 3+ WENO | Cada uno es una **clase concreta separada** que implementa la interfaz |
| Limitadores de pendiente | minmod, van Leer, superbee como mínimo | Cada uno seleccionable por configuración |
| Solvers de Riemann | HLL, HLLE, HLLC | Cada uno una clase concreta; misma interfaz; intercambiables por configuración |
| Integración temporal | Explícita, Runge-Kutta de orden ≥ 2, con control de paso por CFL | Estable hasta el CFL declarado |
| Condiciones de frontera | Transmisiva, reflectiva, periódica | Las tres, en 1D y 2D |

Notas de precisión, a partir de la lectura del código Fortran de referencia:

- El código de referencia implementa **HLL con estimación de velocidades de
  Davis** (`SL = min(uL−aL, uR−aR)`), que corresponde a la columna «Simple» del
  Cuadro 3.1 del anteproyecto. **No implementa HLLE**: no hay promedios de Roe
  ni la corrección de Einfeldt en ninguno de los dos códigos entregados. HLLE se
  implementa desde cero.
- El código de referencia **no implementa WENO**. Llega hasta MUSCL.

### Objetivo 3 — Validación

**Nivel de verificación (1D, contra solución analítica exacta).**

| Caso | ¿Existe en el Fortran de referencia? |
|---|---|
| Sod clásico (\(u_L = u_R = 0\)) | No. El caso 1 del Fortran es el Sod *modificado* de Toro, con \(u_L = 0.75\) |
| Lax | No |
| Problema 123 (dos rarefacciones) | Sí (caso 2) |
| Doble choque | Sí (caso 4) |
| Contacto casi estacionario | Sí (caso 5) |

Criterios propuestos:

- Error \(L_1\) y \(L_2\) frente a la solución exacta, reportados por variable.
- Orden de convergencia observado \(p_{obs} \ge 0.9\,p_{teórico}\) medido sobre
  una solución suave, con al menos cuatro resoluciones.
- Positividad: densidad y presión estrictamente positivas en todos los casos, sin
  que se active ningún piso numérico.

**Nivel de validación (2D, contra referencia de alta fidelidad).**

- Problema de Riemann 2D de Lax-Liu.

**(B-2)** El Objetivo 3 del anteproyecto pide «al menos un caso en 2D», pero la
metodología (Etapa 3) nombra además la **reflexión de Mach**. La reflexión de
Mach requiere una condición de frontera dependiente del tiempo que no existe en
el código de referencia. ¿Es entregable comprometido o meta opcional?

**(B-3)** ¿Cuántas configuraciones de Lax-Liu? El código de referencia trae la
configuración 3 únicamente.

**Banco de pruebas: la matriz de comparación.**

Este es el entregable que responde la pregunta de investigación. Consiste en
ejecutar el mismo problema variando **una sola** elección algorítmica a la vez, y
cuantificar el efecto.

**(B-4)** ¿Cuántas combinaciones? Tres reconstructores × tres solvers son nueve
por cada caso y cada resolución. ¿Se exige la matriz completa o basta con aislar
HLLE contra HLLC manteniendo el reconstructor fijo?

**(B-5)** ¿Qué tolerancia se considera aceptable frente al Fortran de referencia?
Propuesta: \(\max|diff| \le 10^{-12} + 10^{-9}\max|ref|\) en todas las variables,
**y el número de pasos temporales idéntico**. Este criterio ya se cumple en 1D.

### Objetivo 4 — Documentación dual

| Vertiente | Contenido | Criterio |
|---|---|---|
| Técnica | Arquitectura, contratos de interfaz, guía de extensión, suite de pruebas | Un desarrollador externo puede añadir un solver siguiendo solo la guía |
| Pedagógica | Manual de instalación, tutoriales progresivos 1D→2D, archivo de configuración de ejemplo por caso | Un estudiante instala y reproduce un caso sin ayuda |
| Reproducibilidad | Verificación final: instalar, configurar y simular desde cero siguiendo solo el manual, sin intervención de los autores | Se ejecuta y se documenta el resultado |

---

## 4. Alcance excluido explícitamente

Se declara fuera para que no reaparezca como entrega tardía. Si algo de aquí
debe entrar, requiere acuerdo expreso y revisión de plazos.

**Otros sistemas de ecuaciones.** Los objetivos hablan exclusivamente de las
ecuaciones de Euler. Quedan fuera magnetohidrodinámica (MHD 1D y 2D) y aguas
someras, que forman parte del proyecto del director pero no de esta tesis.

**Paralelización.** La presentación del tema la sitúa explícitamente en «trabajos
futuros» y el anteproyecto en «una fase posterior». El compromiso es que la
arquitectura **no impida** incorporar OpenMP después: bucles sobre celdas
aislados, sin dependencias entre iteraciones, documentados. No se entrega código
paralelo ni medidas de escalabilidad.

**Física adicional presente en el código de referencia.** Los códigos Fortran
incluyen gravedad con esquema bien balanceado, geometría cilíndrica y esférica,
Sedov-Taylor, Kelvin-Helmholtz y Rayleigh-Taylor. Ninguno aparece en los
objetivos. Se conservan los que ya están migrados —son referencias de regresión
valiosas contra el Fortran— pero **no se invierte tiempo adicional en ellos** ni
se consideran entregables.

**Mallas no estructuradas, geometría arbitraria, flujo viscoso, transferencia de
calor, química, multifase.** Fuera del modelo físico declarado.

**Interfaz gráfica.** El postprocesamiento se hace con herramientas externas
(matplotlib para 1D, ParaView para 2D).

---

## 5. Decisiones técnicas que requieren confirmación

El código actual se aparta en cuatro puntos de lo declarado en el anteproyecto.
Ninguno es un error, pero conviene resolverlos por escrito porque están escritos
en un documento ya entregado.

| # | El anteproyecto dice | El código hace hoy | Propuesta |
|---|---|---|---|
| **C-1** | C++17 (§5.1) | C++20 | Bajar a C++17. Se verificó que no se usa ninguna característica exclusiva de C++20: es un cambio de una línea |
| **C-2** | Google Test o Catch2 (§5.2) | Arnés de pruebas propio, sin dependencias externas | Migrar a Catch2. Se pide confirmar, porque el arnés propio se eligió deliberadamente para que el proyecto compile sin descargar nada |
| **C-3** | Configuración en JSON o YAML con *factory pattern* (§4.4) | Namelist de Fortran | Añadir lector JSON como formato principal y **conservar** el lector de namelist como entrada de compatibilidad. Ambos producen el mismo objeto de configuración. Permite seguir validando contra el Fortran indefinidamente |
| **C-4** | CSV o texto plano en 1D, VTK para ParaView en 2D (§5.3) | Formato `.xl` heredado del Fortran | Igual que C-3: añadir CSV y VTK como salidas de usuario y conservar `.xl`/`.xyl` para la regresión byte a byte |

En C-3 y C-4 la propuesta de conservar ambos no es duplicación innecesaria: es
precisamente la demostración práctica del desacoplamiento que defiende el
Objetivo 1, y preserva la capacidad de validar contra el código de referencia.

**(C-5)** ¿WENO de qué orden? WENO5 de Jiang-Shu es el estándar, pero obliga a
ampliar las celdas fantasma de 2 a 3 y a revisar las tres condiciones de
frontera. WENO3 cumple «orden 3+» con un stencil que cabe en los fantasmas
actuales.

---

## 6. Estado actual verificado

Lo que sigue está comprobado sobre el código, no estimado.

**Migración 1D terminada y validada.** El solver Euler 1D está migrado a C++ y
verificado contra el Fortran original compilado: **26 de 26 configuraciones**
dentro de tolerancia, con el número de pasos temporales coincidiendo exactamente
en todas. De 13 casos de regresión contra salidas archivadas, 9 son idénticos
byte a byte. La suite tiene 87 casos en 6 ejecutables bajo CTest, verdes en
Release y en Debug, y compila sin advertencias con `-Wall -Wextra -Wpedantic
-Wshadow -Wconversion -Wsign-conversion`.

**Inventario de módulos concretos por interfaz:**

| Interfaz | Implementaciones | Pendiente |
|---|---|---|
| `Reconstruction` | 1 (`MusclReconstruction`) | Extraer orden 1 como clase propia; añadir WENO |
| `RiemannSolver` | 2 (`Hll`, `Hllc`) | Añadir `Hlle` |
| `TimeIntegrator` | 2 (`ForwardEuler`, `SspRk2`) | — |
| `BoundaryCondition` | 3 (transmisiva, periódica, reflectiva) | Versión 2D |
| `DivergenceOperator` | 2 (cartesiana, radial) | Versión 2D |

Sobre el primer punto: el orden 1 funciona hoy como una *opción de limitador*
dentro de MUSCL (pendiente nula), no como módulo independiente. El resultado
numérico es correcto, pero el Objetivo 2 pide «módulos concretos para al menos
dos reconstructores». Extraerlo es trabajo menor y deja preparado el lugar donde
entra WENO.

**Frente a los objetivos, a grandes rasgos:**

| Objetivo | Presentación del tema | Anteproyecto |
|---|---|---|
| 1 — Arquitectura | Casi completo; falta el diagrama | Igual |
| 2 — Núcleo | Falta el 2D | Falta el 2D, HLLE y WENO |
| 3 — Validación | 1D hecho; falta 2D | Además faltan Sod clásico y Lax, y la matriz sistemática |
| 4 — Documentación | Técnica avanzada; falta la pedagógica | Igual |

---

## 7. Orden de trabajo propuesto

Ordenado por lo que más reduce riesgo, no por lo que más cuesta.

1. **HLLE.** Es la pregunta de investigación, cuesta poco, y al implementarlo se
   mide la afirmación de arquitectura del Objetivo 1: cuántos archivos del núcleo
   hubo que tocar. Si son cero, hay evidencia; si no, el diseño necesita revisión
   y es mucho mejor descubrirlo ahora.
2. **Sod clásico, Lax, y extraer el reconstructor de orden 1.** Huecos baratos y
   visibles frente a los objetivos.
3. **Alineación técnica (C-1 a C-4).** Cerrar la distancia con lo ya declarado.
4. **Matriz de comparación en 1D.** Con tres solvers y dos reconstructores ya se
   puede producir el entregable central del banco de pruebas.
5. **Núcleo 2D.** El mayor volumen de trabajo. Prioridad interna: malla, fronteras
   y solver direccional primero; Lax-Liu como caso de validación.
6. **WENO**, sujeto a (A-2) y (C-5).
7. **Documentación pedagógica y prueba de reproducibilidad desde cero.**

Los casos Kelvin-Helmholtz y Rayleigh-Taylor del código 2D de referencia no
aparecen en ningún objetivo. Se migrarán solo si sobra tiempo, por su valor como
regresión contra el Fortran, nunca antes de Lax-Liu.

---

## 8. Peticiones concretas

Independientes de las decisiones anteriores, y todas de bajo coste para el
director:

1. **`correr_pruebas.py`**, la suite de regresión del proyecto. El `Makefile` de
   `Code_Euler2D` la invoca (`cd .. && python3 correr_pruebas.py Code_Euler2D`)
   pero no venía en el paquete. Sin ella no se conoce su criterio de «pasa».
2. **`Code_Euler1D` actualizado.** El paquete recibido corresponde al estado del
   18 de agosto a las 07:36. El commit del mismo día a las 18:21 («El piso de
   presión deja de dispararse en silencio») declara haber modificado Euler1D, y
   ese cambio no está en la copia recibida. La migración se validó contra la
   versión anterior.
3. **Confirmación de que no hay entregas posteriores** al 18 de agosto dentro del
   alcance de Euler.
4. **Un ejemplo del estándar esperado** para el capítulo de resultados: una
   figura o tabla de un trabajo similar que sirva de referencia de formato y
   nivel de detalle.

---

## 9. Resumen de puntos abiertos

| # | Pregunta | Bloquea |
|---|---|---|
| A-1 | ¿Rige el anteproyecto sobre la presentación del tema? | Todo el alcance |
| A-2 | ¿WENO es entregable o trabajo futuro? | Objetivo 2 |
| B-1 | Formato del diagrama de componentes | Objetivo 1 |
| B-2 | ¿Reflexión de Mach: comprometida u opcional? | Objetivo 3 |
| B-3 | ¿Cuántas configuraciones de Lax-Liu? | Objetivo 3 |
| B-4 | ¿Matriz completa 3×3 o solo HLLE vs HLLC? | Objetivo 3 |
| B-5 | ¿Tolerancia aceptable frente al Fortran? | Criterio de validación |
| C-1 | ¿C++17 o se mantiene C++20? | Implementación |
| C-2 | ¿Catch2 o se mantiene el arnés propio? | Implementación |
| C-3 | ¿JSON como formato principal, conservando namelist? | Implementación |
| C-4 | ¿CSV y VTK, conservando `.xl`? | Implementación |
| C-5 | ¿WENO3 o WENO5? | Implementación |
