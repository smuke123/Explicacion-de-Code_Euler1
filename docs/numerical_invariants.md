# Invariantes numéricos y reglas de preservación

**Fecha:** 27 de septiembre de 2026

Este es el documento más importante del conjunto para la credibilidad del
trabajo, y el que hay que consultar antes de tocar cualquier fórmula.

## La regla fundamental

> **El comportamiento numérico del solver de referencia se preserva a menos que
> un cambio esté explícitamente aprobado, documentado y acompañado de su propia
> validación.**

Esto no es conservadurismo. Es lo que hace posible afirmar que una diferencia
observada entre dos esquemas es del método y no de un error de implementación.
Si el comportamiento numérico deriva sin control, la comparación central de la
tesis pierde su base.

## Jerarquía de prioridades

Cuando dos objetivos entran en conflicto, este es el orden:

```
1. Corrección matemática
2. Equivalencia con la referencia Fortran
3. Validación demostrable
4. Arquitectura
5. Legibilidad
6. Rendimiento
7. Preparación para paralelización
```

La arquitectura está en cuarto lugar a propósito. **Nunca se modifica el método
numérico para simplificar el diseño del software.** Si una abstracción exige
cambiar una fórmula, la abstracción es incorrecta.

---

# Parte I — Qué no se cambia en silencio

Los siguientes elementos están congelados. Modificar cualquiera requiere el
procedimiento de la Parte III.

| Elemento congelado | Qué significa concretamente |
|---|---|
| Ecuaciones | Forma conservativa, definición de la energía total, ecuación de estado |
| Esquemas numéricos | Estructura de la reconstrucción, del flujo y de las etapas temporales |
| Coeficientes | Todo número literal de una fórmula |
| Definición de los limitadores | Incluidas sus peculiaridades de comportamiento en los bordes |
| Ecuaciones de los solvers de Riemann | Estimación de velocidades de onda, estados estrella, selección de rama |
| Fórmulas de reconstrucción | Cálculo de pendientes y extrapolación a las caras |
| Cálculo de la condición CFL | Y **cuál de las dos fórmulas** aplica en cada dimensión |
| Lógica del paso de tiempo | Incluido el recorte del último paso |
| Condiciones de frontera | Qué se copia, qué se refleja y con qué signo |
| Comportamiento de las celdas fantasma | Anchura, orden de llenado, tratamiento de esquinas |
| Términos fuente | Discretización y punto de aplicación |
| Pisos de positividad | Valores, dónde se aplican y cuándo se cuentan |
| Condiciones iniciales | Estados, posición del diafragma, perfiles |
| Semántica de la salida | Formato, precisión, qué se escribe y cuándo |

---

# Parte II — Invariantes explícitos

Cada invariante tiene un identificador, el motivo por el que existe y cómo se
verifica. Los que provienen de un hallazgo real de la auditoría del proyecto
Fortran están marcados, porque son los que ya demostraron romperse.

### INV-001 — Un flujo por cara, compartido por las dos celdas
El flujo numérico en una cara se calcula **una vez** y se resta a una celda y se
suma a la otra. No se calculan dos flujos independientes por cara.

**Por qué.** Es lo que hace que la conservación sea estructural y no
aproximada. Si cada celda calculara su propio flujo, la masa se crearía o
destruiría en cada cara por error de redondeo.

**Verificación.** Test de conservación con fronteras periódicas: masa y energía
totales constantes con error relativo por debajo de \(10^{-12}\) (FR-036).

### INV-002 — El orden de las operaciones de punto flotante se preserva
No se reordenan, reasocian ni «simplifican» expresiones aritméticas respecto al
código de referencia.

**Por qué.** La aritmética de punto flotante no es asociativa.
\((a+b)+c \ne a+(b+c)\) en general. Reordenar una suma cambia el último bit, el
último bit cambia el `dt` del paso siguiente, y la trayectoria divergen. La
regresión byte a byte dejaría de pasar.

**Verificación.** Regresión contra las salidas archivadas: 9 de 13 casos son hoy
idénticos byte a byte. Cualquier reordenación lo rompería inmediatamente.

**Corolario práctico.** Prohibido `-ffast-math` y cualquier opción equivalente
(`ADR-003`).

### INV-003 — `sign(1.0, 0.0)` vale +1, no 0
La función auxiliar de signo usada en los limitadores devuelve \(+1\) cuando el
argumento es cero, replicando el comportamiento de `sign` de Fortran y **no** el
de la función signo matemática.

**Por qué.** En las celdas donde la pendiente es exactamente nula, la diferencia
entre \(+1\) y \(0\) cambia el resultado del limitador. Es sutil y es
exactamente el tipo de detalle que alguien «corrige» de buena fe.

**Verificación.** Test unitario específico con argumento cero, y la regresión
completa de los cinco limitadores.

### INV-004 — Las dos etapas de Runge-Kutta usan arreglos destino separados
Ninguna etapa lee y escribe el mismo arreglo.

**Por qué.** `HALLAZGO DE AUDITORÍA.` Una versión anterior del código de
referencia llamaba a la función del operador con el mismo arreglo como entrada y
salida —aliasing ilegal en Fortran— y funcionaba por casualidad del compilador.
Aquí la separación es estructural: la firma exige dos arreglos distintos y el
error no se puede cometer.

**Verificación.** Inspección de la firma, más la regresión.

### INV-005 — La reconstrucción opera sobre un estado primitivo sincronizado
Quien invoca el operador espacial debe haber sincronizado la representación
primitiva con la conservada que pasa como entrada. En C++ esto es un parámetro
explícito de la firma, no una convención.

**Por qué.** `HALLAZGO DE AUDITORÍA.` En el código de referencia, la función del
operador recibía la conservada como argumento pero reconstruía leyendo un arreglo
primitivo **global**. Pasar una conservada no sincronizada evaluaba el operador
en un estado distinto del pedido, sin ningún aviso. El auditor midió el efecto:
error \(L_1\) del orden de \(3\times10^{-2}\) **sin producir ningún `NaN`**.

**Verificación.** La firma de la interfaz lo impone. No se puede llamar sin pasar
el campo primitivo.

### INV-006 — Cada dimensión usa su propia fórmula de CFL
En 1D:
\[ \Delta t = C\,\frac{\Delta x}{\max(|u| + a)} \]

En 2D sin separación dimensional:
\[ \Delta t = \frac{C}{\dfrac{s_x}{\Delta x} + \dfrac{s_y}{\Delta y}}, \qquad
   s_x = \max(|u| + a), \quad s_y = \max(|v| + a) \]

con los máximos calculados **por dirección**, no un único máximo global.

**Las dos fórmulas son distintas y no se pueden unificar.**

**Por qué.** `HALLAZGO DE AUDITORÍA, CLASIFICADO COMO MAYOR.` El solver 2D del
director usaba inicialmente el criterio heredado del 1D,
\(\min(\Delta x, \Delta y) / s_{max}\). Eso permite números de CFL efectivos por
encima de 0.5 y produce corrupción en patrón de **tablero de ajedrez de forma
silenciosa** a partir de CFL ≈ 0.75. Fue silenciosa porque los pisos de
positividad enmascaraban el `NaN` que habría delatado el problema. La auditoría
lo localizó con un barrido de CFL y un detector de alternancia de signo del
laplaciano.

**Verificación.** Barrido de CFL de 0.45 a 0.95 en un caso 2D, midiendo el
indicador de tablero. Debe mantenerse estable hasta el CFL declarado.

**Nota de diseño.** Por esto el controlador de CFL tiene **dos métodos** y no un
método con una rama. Son dos criterios de estabilidad distintos, no una
generalización del mismo.

### INV-007 — Los pisos de positividad se cuentan y se reportan
Cuando la densidad o la presión se recortan al piso, el evento se cuenta, y al
final de la ejecución se informa el total si es distinto de cero.

**Por qué.** `HALLAZGO DE AUDITORÍA, CLASIFICADO COMO MAYOR.` Cuando el piso
dispara, la representación primitiva **deja de ser** la imagen de la conservada
bajo la ecuación de estado. A partir de ese momento, el estado con el que el
código reconstruye, calcula velocidades características y escribe la salida no es
el estado del sistema, y nada lo indicaba. La auditoría midió el umbral: en un
caso magnetizado, 0 activaciones a \(\beta \approx 0.84\) y 695 a
\(\beta \approx 0.17\), con fuerte dependencia del limitador.

**Verificación.** Con el piso en su valor real, los casos estándar reportan cero.
Elevando el piso artificialmente, el contador crece de forma monótona. Ambos
comprobados por test.

**Corolario.** El contador **no** se incrementa dentro de una región paralela sin
reducción. El solver 2D de referencia tiene precisamente esa carrera de datos
(incremento compartido sin `reduction` ni `atomic`), documentada en
`inconsistencies.md`, y **no debe reproducirse**.

### INV-008 — La estructura de índices de las celdas fantasma se preserva
Los índices de celda van de \(-1\) a \(N+2\), con \(1..N\) las celdas físicas,
igual que el código de referencia.

**Por qué.** Permite auditar la migración comparando línea a línea con el
original, que es el criterio de aceptación del proyecto. Cambiar a base cero
habría hecho la verificación mucho más costosa sin ganar nada.

**Verificación.** Aserciones de rango en compilación de depuración, y la
regresión.

**Nota de evolución.** La **anchura** de la zona fantasma sí puede cambiar: WENO5
necesita 3 en lugar de 2 (FR-009). Lo que no cambia es la convención de
numeración. Ampliar la anchura obliga a revisar las tres condiciones de frontera
y es un cambio del núcleo que se documenta como tal.

### INV-009 — El solver exacto no participa en el avance temporal
La solución exacta del problema de Riemann se usa exclusivamente como referencia
de verificación y para generar perfiles de comparación.

**Por qué.** Si participara en el avance, el esquema dejaría de ser el que se
está evaluando, y la comparación contra la solución exacta se volvería circular.

**Verificación.** Por construcción: el solver exacto cuelga del orquestador y no
del operador espacial. Visible en el diagrama de componentes.

### INV-010 — El formato de salida preserva la semántica del original
Descriptor de punto flotante con exponente de **tres** dígitos, terminadores de
línea LF escritos en modo binario, y bloques encabezados por la marca de tiempo.

**Por qué.** El exponente de tres dígitos fue un arreglo deliberado de la
auditoría: los desbordamientos por defecto del orden de \(10^{-263}\) que
aparecen en las velocidades de casos simétricos rompían el analizador de las
herramientas de Python con el formato de dos dígitos. Y los terminadores LF en
modo binario son lo que permite la comparación byte a byte en Windows.

**Verificación.** Comparación byte a byte con las salidas archivadas, y la regla
de `.gitattributes` que marca los datos de referencia como binarios.

### INV-011 — No se vuelca dos veces el mismo instante
Si el último paso temporal ya produjo un volcado, el cierre de la simulación no
lo repite.

**Por qué.** Duplicar el bloque final rompe los analizadores que toman «el último
bloque» y falsea cualquier comparación automatizada.

**Verificación.** Test que cuenta los bloques del archivo de salida.

### INV-012 — Los valores por defecto del caso no pisan lo que el usuario dio
Un valor especificado explícitamente por el usuario no se sobreescribe. Un valor
que el caso impone por necesidad física se aplica **y se reporta**.

**Por qué.** `HALLAZGO DE AUDITORÍA.` En el código de referencia los valores del
caso pisaban los del usuario siempre y en silencio, de modo que configurar un
dominio distinto no tenía efecto y no había señal de ello.

**Verificación.** Tests que dan un valor explícito y comprueban que sobrevive, y
que provocan una imposición y comprueban que se emite el aviso.

### INV-013 — Las claves desconocidas de configuración son error
**Por qué.** `HALLAZGO DE AUDITORÍA.` La lectura de un namelist de Fortran es
secuencial: una errata de un carácter deja asignado todo lo anterior al error y
el resto en su valor por defecto, y la ejecución continúa sin advertencia. El
auditor lo verificó: escribir `Nyy` por `Ny` ejecutaba una malla de 128×256 en
lugar de 128×64 e imprimía un error plausible; metido en una tabla de
convergencia, el orden observado pasaba de 1.98 a −0.98 **sin ninguna señal**.

**Verificación.** Test con una clave mal escrita: debe abortar con código 1
nombrando la clave.

### INV-014 — Los estados no finitos se detectan, no se absorben
La presencia de `NaN` o `Inf` en el campo aborta la ejecución.

**Por qué.** `std::max` y el `max` de Fortran absorben `NaN` silenciosamente
según el orden de los argumentos. Un `NaN` en el campo podía no afectar al máximo
calculado y propagarse durante cientos de pasos.

**Verificación.** El barrido de cálculo de `dt` calcula además una suma de
comprobación; si no es finita, aborta. Test con un `NaN` inyectado.

**Nota de diseño.** La detección se aprovecha de un barrido que ya existe, de
modo que no añade coste. Comprobar `isfinite` celda a celda en los bucles
calientes sí lo añadiría.

---

# Parte III — Procedimiento para cambiar comportamiento numérico

Cuando se encuentra un posible defecto, o cuando hay una razón para desviarse de
la referencia, el procedimiento es este. No se salta ningún paso.

## 1. Documentar antes de tocar

Se registra en `inconsistencies.md`: qué se observó, en qué archivo y línea, y
cuál es el comportamiento esperado frente al observado.

## 2. Medir el efecto

Cuantificar qué cambia si se corrige. No basta con «está mal»: hay que saber si
el efecto es de \(10^{-16}\) o de \(10^{-2}\), porque la decisión depende de eso.
Si el efecto es indetectable, corregirlo puede no merecer la pérdida de la
regresión byte a byte.

## 3. Clasificar

| Clase | Qué es | Qué se hace |
|---|---|---|
| **Preservado** | Peculiaridad sin consecuencia, o cuyo cambio rompería la equivalencia sin beneficio | Se replica y se documenta por qué |
| **Reestructurado** | El comportamiento observable es idéntico, la organización del código cambia | Se hace; la regresión debe seguir intacta |
| **Desviación deliberada** | El comportamiento cambia a propósito | Requiere los pasos 4 a 6 |

Esta clasificación ya está en uso: `inconsistencies.md` cataloga 17
peculiaridades del original repartidas en las tres clases.

## 4. Decidir explícitamente

Una desviación deliberada se registra en `decision_log.md` con su
identificador, la alternativa de preservar el comportamiento, y la razón por la
que se descarta.

`PENDING INPUT FROM DIRECTOR` — cualquier desviación que afecte a un resultado
publicable debe consultarse. No es una decisión de ingeniería de software.

## 5. Aislar el cambio

La desviación va en **un commit propio**, que no contiene nada más. Así se puede
revertir sin arrastrar trabajo no relacionado, y el efecto sobre la regresión es
atribuible sin ambigüedad.

## 6. Validar la desviación

Tres cosas, las tres obligatorias:

1. Un test nuevo que falle con el comportamiento antiguo y pase con el nuevo.
2. Actualizar los datos de referencia afectados, **regenerándolos con el Fortran
   corregido si la corrección también se le aplica a él**. Nunca con el C++: eso
   convertiría la validación en autoconfirmación.
3. Reportar el efecto sobre la regresión completa: qué casos cambian, en cuánto,
   y si el número de pasos temporales se altera.

El punto 2 es el más fácil de hacer mal y el más grave. Ver
`validation_strategy.md`.

---

# Parte IV — Señales de alarma

Situaciones en las que conviene detenerse antes de continuar.

| Señal | Qué suele significar |
|---|---|
| La regresión sigue pasando pero el número de pasos cambió | Se alteró la trayectoria. Los campos coinciden por casualidad dentro de tolerancia |
| Un caso empieza a activar pisos de positividad | Algo se desestabilizó. Antes reportaba cero |
| El orden de convergencia observado baja de golpe | Puede ser un error real, o un caso con discontinuidades usado por error para medir orden |
| Un refactor «equivalente» cambia el último bit | INV-002. Se reordenó una expresión |
| El resultado depende del número de hilos | Hay una reducción mal hecha, o una carrera de datos |
| El resultado depende del nivel de optimización | Hay comportamiento indefinido, o se activó una opción que relaja la aritmética |
| Aparece una «solución exacta» para un caso que no la tiene | Se está escribiendo una referencia sin significado |
| Dos ejecuciones idénticas dan salidas distintas | Se rompió el determinismo (NFR-002). La regresión deja de tener valor |

Las dos últimas filas de la tabla describen fallos que **invalidan la estrategia
de validación completa**, no solo un caso. Tienen prioridad sobre cualquier otro
trabajo en curso.
