# Casos de uso

**Fecha:** 27 de septiembre de 2026

Se documentan solo los casos de uso que afectan al diseño. No hay diagramas UML
de casos de uso porque con nueve actores-acción no comunicarían nada que la
tabla no diga ya; el único diagrama que aporta información está en
`architecture.md` (componentes y dependencias).

## Actores

| Actor | Descripción |
|---|---|
| **Estudiante** | Ejecuta casos existentes y observa el efecto de cambiar esquemas. No lee C++ |
| **Investigador** | Añade esquemas y casos, produce comparaciones cuantitativas |
| **Desarrollador** | Mantiene y extiende el framework |
| **Sistema de CI** | Actor no humano: ejecuta la suite y la regresión |

---

## UC-01 — Ejecutar una simulación configurada

**Actor.** Estudiante
**Requisitos.** FR-001, FR-002, FR-007, FR-021, FR-022, FR-028

**Precondiciones.** El framework está compilado. Existe un archivo de
configuración válido.

**Flujo principal.**

1. El estudiante ejecuta `fvlab -i mi_caso.json -o out/mi_caso`.
2. El sistema lee la configuración y la valida.
3. El sistema resuelve el caso de prueba: aplica valores por defecto a lo que el
   usuario no especificó y reporta cualquier valor que el caso imponga.
4. El sistema valida la configuración resuelta **otra vez**.
5. El sistema asigna memoria, construye la malla y aplica la condición inicial.
6. El sistema imprime la configuración efectiva y la referencia analítica si
   existe.
7. El sistema itera: calcula `dt`, avanza un paso, vuelca si corresponde.
8. El sistema escribe el volcado final, la solución de referencia y el registro
   de ejecución.
9. El sistema imprime número de pasos, tiempo final y activaciones de pisos.

**Resultado esperado.** Código de retorno 0. En el directorio de salida: los
campos, la referencia y `run_info.txt`.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| El archivo no existe | Aborta con código 1 nombrando la ruta |
| Clave desconocida | Aborta con código 1 nombrando la clave (FR-003) |
| Valor fuera de rango | Aborta con código 1 nombrando clave y valor |
| El caso impone una frontera distinta de la pedida | Continúa, emitiendo aviso que nombra el conflicto (FR-008) |
| Aparece `NaN` o `Inf` | Aborta con código 2 indicando el paso y la celda (FR-012) |
| El directorio de salida no es escribible | Aborta con código 3 **antes** de simular |
| Un piso de positividad se activa | Continúa; informa el total al final (FR-011) |

**Nota de diseño.** El paso 4 —validar dos veces— es intencional. Un valor
inválido del usuario se detecta aunque el caso fuera a sobreescribirlo, y a la
vez se comprueba que lo que el caso impone sigue siendo coherente. El paso 8
sobre escritura antes de simular evita perder una corrida larga por un
directorio mal escrito.

---

## UC-02 — Comparar una solución numérica con la exacta

**Actor.** Estudiante
**Requisitos.** FR-024, FR-025, FR-033

**Precondiciones.** El caso elegido tiene solución analítica (es un problema de
Riemann).

**Flujo principal.**

1. El estudiante ejecuta UC-01 con `"write_reference": true`.
2. El sistema resuelve el problema de Riemann exacto para los estados iniciales
   del caso.
3. El sistema muestrea la solución exacta **en los centros de celda de la malla
   de la simulación**, al tiempo final alcanzado.
4. El sistema escribe la referencia en `reference/` con el mismo número de puntos
   y las mismas coordenadas que la salida numérica.
5. El estudiante grafica ambas curvas superpuestas.

**Resultado esperado.** Dos conjuntos de datos comparables punto a punto, sin
interpolación.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| El caso no tiene solución analítica | No se escribe `reference/`. **No se escribe una referencia sin significado** |
| El solver exacto no converge | Aborta con código 2 reportando el residuo |

**Nota de diseño.** La segunda fila es una decisión deliberada tomada a partir de
un defecto del código de referencia, que escribía archivos de «solución exacta»
para casos donde no existía. Es preferible la ausencia del archivo a un archivo
engañoso.

---

## UC-03 — Cambiar de esquema numérico sin recompilar

**Actor.** Estudiante, Investigador
**Requisitos.** FR-001, FR-015, FR-016, FR-017, FR-018, FR-019

**Precondiciones.** El framework está compilado.

**Flujo principal.**

1. El estudiante edita una sola clave del archivo de configuración, por ejemplo
   `"riemann_solver": "hllc"` → `"hll"`.
2. Ejecuta con un directorio de salida distinto.
3. Compara los dos directorios (UC-04).

**Resultado esperado.** Dos corridas que difieren **únicamente** en la decisión
algorítmica modificada. Todo lo demás —malla, CFL, caso, tiempo final, número de
volcados— es idéntico.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| Nombre de esquema desconocido | Aborta con código 1 listando los valores admitidos |
| Combinación incompatible (por ejemplo, limitador con WENO) | Aborta con código 1 explicando por qué la combinación no aplica |

**Importancia.** Este es el caso de uso que justifica todo el proyecto. Si
requiere recompilar, editar código o cambiar más de una clave, la arquitectura
ha fallado en su objetivo. El número de pasos temporales puede cambiar
legítimamente entre esquemas, porque `dt` depende del estado; lo que no puede
cambiar es nada de la configuración.

---

## UC-04 — Medir el error entre dos conjuntos de resultados

**Actor.** Estudiante, Investigador, Sistema de CI
**Requisitos.** FR-033

**Precondiciones.** Dos directorios con archivos de los mismos campos y la misma
malla.

**Flujo principal.**

1. Se ejecuta `fvlab-compare out/A out/B`.
2. El sistema empareja los archivos por nombre.
3. Para cada campo, calcula \(\max|diff|\), \(L_1\), \(L_2\) y error relativo.
4. Imprime una tabla y devuelve 0 si todo está dentro de tolerancia, 1 si no.

**Resultado esperado.** Métricas por campo y un código de retorno utilizable como
puerta en CI.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| Distinto número de puntos | Aborta con código 1: **no se interpola** |
| Falta un campo en uno de los directorios | Aborta nombrando el archivo ausente |
| El campo de referencia es casi nulo | Usa tolerancia mixta absoluta y relativa (ver `validation_strategy.md`) |

**Nota de diseño.** No interpolar es deliberado. Comparar mallas distintas
requiere una decisión sobre cómo proyectar, y esa decisión introduce error propio
que contaminaría la medición. Los estudios de convergencia comparan cada malla
contra la solución exacta muestreada en **esa** malla, no entre sí.

---

## UC-05 — Estudio de convergencia

**Actor.** Investigador
**Requisitos.** FR-034, FR-025, FR-033

**Precondiciones.** Un caso con solución de referencia **suave**. Con
discontinuidades el orden formal no se alcanza y la medición no sirve para
verificar el esquema.

**Flujo principal.**

1. El investigador ejecuta el mismo caso con \(N, 2N, 4N, 8N\) celdas.
2. Para cada malla, el sistema escribe la referencia muestreada en esa malla.
3. Se calcula \(L_1\) de cada corrida contra su propia referencia.
4. Se estima \(p_{obs} = \log_2(\|e(\Delta x)\| / \|e(\Delta x/2)\|)\) para cada
   par consecutivo.
5. Se tabula.

**Resultado esperado.** Una tabla de resolución, error y orden observado. Con
reconstrucción de orden 1 se espera \(p_{obs} \approx 1\); con MUSCL, \(\approx
2\); con WENO, \(\ge 3\).

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| El caso tiene discontinuidades | El orden observado cae hacia 1 sea cual sea el esquema. **No indica un error**; indica que el caso no sirve para esta medición |
| El error satura al refinar | Señal de que se alcanzó el suelo de redondeo, o de que la referencia no es exacta |

**Importancia.** Es la prueba de corrección que da derecho a comparar esquemas.
Sin ella, UC-06 compara dos implementaciones que podrían estar ambas mal.

---

## UC-06 — Producir la matriz de comparación de esquemas

**Actor.** Investigador
**Requisitos.** FR-035

**Precondiciones.** UC-05 superado para cada reconstructor. Los solvers a
comparar están implementados y verificados individualmente.

**Flujo principal.**

1. Se fija un caso, una malla y un CFL.
2. Se ejecutan todas las combinaciones de reconstructor y solver.
3. Para cada una se mide el error contra la solución exacta.
4. Se tabula, y se grafica el detalle de la discontinuidad de contacto.

**Resultado esperado.** Una tabla que aísla el efecto de cada elección
algorítmica y una figura que lo muestra. Es el entregable que responde la
pregunta de investigación.

**Nota.** El caso más informativo para comparar HLLE con HLLC es el de **contacto
casi estacionario** (caso 5 de Toro): un contacto que apenas se mueve es donde
los esquemas de dos ondas más lo difuminan y donde el de tres ondas más gana.

**Punto abierto.** `B-4` — si se exige la matriz completa de reconstructores por
solvers o basta aislar HLLE contra HLLC.

---

## UC-07 — Añadir un caso de prueba

**Actor.** Investigador
**Requisitos.** FR-008, FR-038

**Flujo principal.**

1. Se añade una entrada declarativa a la tabla de casos: nombre, descripción,
   estados, dominio y tiempo por defecto, fronteras impuestas si las hay, y si
   tiene solución de referencia.
2. Se incrementa la constante que dimensiona la tabla.
3. Se añade un archivo de configuración de ejemplo.
4. Se añade una prueba que verifique los valores declarados.

**Resultado esperado.** El caso es seleccionable por configuración sin haber
modificado el solver.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| Se olvida incrementar la constante | **Error de compilación**, no fallo en ejecución |
| El caso no es un problema de Riemann | Se marca como tal y se añade la rama de condición inicial correspondiente |
| El caso no tiene solución analítica | Se marca como tal; no se escribe referencia (ver UC-02) |

**Nota de diseño.** Que el olvido sea un error de compilación es intencional: la
tabla se dimensiona con la misma constante que valida el rango, de modo que las
dos mitades del cambio no pueden desincronizarse.

---

## UC-08 — Añadir un método numérico

**Actor.** Desarrollador
**Requisitos.** FR-037, FR-020

**Precondiciones.** El método encaja en uno de los puntos de variación
existentes: reconstrucción, solver de Riemann, integrador temporal, término
fuente o condición de frontera.

**Flujo principal.**

1. Se implementa una clase que satisface la interfaz correspondiente.
2. Se añade el valor al enumerado de configuración.
3. Se añaden las conversiones de cadena en los dos sentidos.
4. Se añade la rama en el punto único de construcción de componentes.
5. Se escriben las pruebas unitarias del nuevo componente.
6. **Se cuenta y se registra** el número de archivos del núcleo modificados.

**Resultado esperado.** El método es seleccionable por configuración. El número
del paso 6 es **cero**.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| El método necesita un stencil más ancho | Hay que ampliar la zona fantasma (FR-009) y revisar las fronteras. Es un cambio del núcleo y **hay que documentarlo como tal** |
| El método no encaja en ningún punto de variación | Señal de que falta un punto de variación. Requiere decisión arquitectónica registrada en `decision_log.md` |
| El paso 6 da un número distinto de cero | La afirmación del `Obj-1` no se cumple para ese eje. Se documenta y se corrige el diseño |

**Importancia.** El paso 6 es lo que convierte la afirmación arquitectónica del
`Obj-1` en un resultado medible en lugar de argumentado. El caso de referencia
para medirlo es la incorporación de HLLE.

---

## UC-09 — Validar contra el solver de referencia

**Actor.** Desarrollador, Sistema de CI
**Requisitos.** FR-030, FR-032

**Precondiciones.** El código Fortran de referencia está disponible y hay un
compilador Fortran, o existen salidas archivadas.

**Flujo principal.**

1. Se compila el solver Fortran en un directorio temporal.
2. Para cada configuración de la matriz de validación, se ejecutan **ambos**
   códigos con el mismo archivo de entrada.
3. Se comparan las salidas campo a campo.
4. Se comprueba además que el **número de pasos temporales** coincide.
5. Se reporta una tabla y se devuelve código distinto de cero si algo falla.

**Resultado esperado.** Todas las configuraciones dentro de tolerancia, con los
pasos coincidiendo exactamente.

**Flujos alternativos.**

| Situación | Comportamiento |
|---|---|
| No hay compilador Fortran | Se degrada a regresión contra salidas archivadas, y **se indica que el nivel de validación es menor** |
| El número de pasos difiere | Fallo, aunque los campos estén dentro de tolerancia. Indica que se alteró el cálculo de `dt` o la trayectoria |
| Una configuración solo existe en uno de los dos códigos | Se excluye explícitamente y se documenta por qué |

**Nota de diseño.** El paso 4 es el criterio más sensible de todos. Los campos
pueden coincidir dentro de tolerancia por casualidad tras una trayectoria
distinta; el recuento de pasos no. En la migración 1D este criterio se cumple en
las 26 configuraciones, hasta 2673 pasos.

---

## Casos de uso deliberadamente excluidos

| Excluido | Razón |
|---|---|
| Reanudar una simulación desde un volcado | Ningún objetivo lo pide. Las corridas de validación son cortas |
| Ejecutar varias simulaciones en paralelo desde el framework | Se resuelve fuera, con un script. No justifica complejidad interna |
| Interfaz gráfica de configuración | `§5.3` asigna la visualización a matplotlib y ParaView |
| Modificar parámetros durante la ejecución | Rompería el determinismo (NFR-002) y la reproducibilidad (NFR-003) |
| Mallado adaptativo o refinamiento local | Fuera del alcance: los objetivos dicen «dominios estructurados» |
