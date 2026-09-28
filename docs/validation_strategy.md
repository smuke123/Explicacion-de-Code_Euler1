# Estrategia de verificación, validación y datos de referencia

**Fecha:** 27 de septiembre de 2026

Documentos relacionados: `testing.md` (inventario de la suite actual),
`validation.md` (resultados medidos), `numerical_invariants.md` (qué se preserva).
Este documento define la **política**; los otros registran su aplicación.

## La distinción que hay que tener clara

El anteproyecto (§6) la enuncia bien y conviene repetirla, porque casi todo lo
demás se deriva de ella:

> **Verificación:** ¿estamos resolviendo las ecuaciones correctamente?
> **Validación:** ¿estamos resolviendo las ecuaciones correctas?

Y hay un tercer concepto que no es ninguno de los dos y que se confunde con
ambos constantemente:

> **Regresión:** ¿seguimos obteniendo lo mismo que ayer?

La regresión **no demuestra corrección**. Demuestra estabilidad. Un código
consistentemente incorrecto pasa la regresión perfectamente. Es valiosa
—detecta cambios no intencionados con enorme sensibilidad— pero no se puede
presentar como evidencia de que el resultado es correcto.

---

# Parte I — Niveles de prueba

Para cada nivel: qué prueba, qué **no** prueba, y cuándo se ejecuta.

## 1. Pruebas unitarias

**Qué son.** Verificación de un componente en aislamiento, sin construir una
simulación.

**Alcance.** Ecuación de estado y conversiones; los cinco limitadores; los
solvers de Riemann cara a cara; las tres condiciones de frontera; el cálculo de
`dt`; el solver exacto contra valores publicados; el analizador de error; el
formateo de salida; el analizador de configuración.

**Qué prueban.** Que cada pieza hace lo que su contrato dice, para las entradas
probadas.

**Qué NO prueban.** Que las piezas encajen. Que el conjunto resuelva las
ecuaciones de Euler. Que el esquema sea estable. Un código con todas las unitarias
verdes puede producir resultados completamente incorrectos si el ensamblaje está
mal.

**Cuándo.** En cada compilación. Duran milisegundos.

**Propiedades que merece la pena comprobar en un solver de Riemann:**

| Propiedad | Criterio |
|---|---|
| Consistencia | Con \(W_L = W_R\), el flujo numérico es el flujo físico |
| Upwind supersónico | Con \(S_L \ge 0\), devuelve \(F_L\) **exactamente** |
| Equivalencia barrido / cara | El barrido vectorizado y la función de una cara coinciden exactamente |
| Simetría | Invertir izquierda y derecha invierte el signo de la velocidad del flujo |

La tercera es la que protege la decisión de tener dos rutas de código, una legible
y una optimizada (NFR-008).

## 2. Pruebas de integración

**Qué son.** Simulaciones completas en un directorio temporal, comprobando
propiedades del conjunto.

**Alcance.** Conservación global con fronteras periódicas; equilibrio
hidrostático; orden de convergencia; intercambiabilidad de componentes;
comportamiento ante configuraciones inválidas.

**Qué prueban.** Que el ensamblaje es coherente y que el esquema tiene las
propiedades matemáticas esperadas.

**Qué NO prueban.** Que los valores sean los correctos. Un esquema puede
conservar la masa perfectamente y situar el choque en el lugar equivocado.

**Cuándo.** En cada compilación. Duran segundos.

**Advertencia.** Un test de integración que solo comprueba «no explota» no
comprueba casi nada. Debe verificar una propiedad que un error real rompería:
una cantidad conservada, una simetría, un orden de convergencia, o una relación
de orden entre dos configuraciones que deba cumplirse necesariamente.

## 3. Pruebas de regresión

**Qué son.** Comparación de la salida actual contra salidas archivadas del solver
Fortran de referencia.

**Qué prueban.** Que el comportamiento numérico no ha cambiado. Con enorme
sensibilidad: detectan una diferencia en el último bit.

**Qué NO prueban.** **Corrección.** Esto es lo más importante de todo el
documento. Si la referencia estuviera mal, la regresión certificaría el error
para siempre.

**Criterio de fallo doble.** Una configuración pasa si, y solo si:

1. Todas las variables están dentro de tolerancia, y
2. el **número de pasos temporales coincide exactamente**.

El segundo criterio es el más sensible de los dos y es el que hay que vigilar.
Los campos pueden coincidir dentro de tolerancia tras una trayectoria distinta;
el recuento de pasos no. Si los pasos cambian, se alteró el cálculo de `dt` o la
trayectoria, y eso hay que explicarlo aunque los campos pasen.

**Cuándo.** Antes de cada commit y en CI. Duran segundos y leen unos 21 MB.

**Estado actual medido.** 13 casos archivados; 9 idénticos byte a byte.

## 4. Validación directa contra el Fortran

**Qué es.** Compilar el código Fortran de referencia y ejecutar **ambos** códigos
con el mismo archivo de entrada, comparando las salidas.

**Por qué es mejor que la regresión.** No depende de archivos archivados que
pueden estar desactualizados, y cubre configuraciones para las que nadie guardó
una salida. Es el nivel de validación más fuerte disponible.

**Qué NO prueba.** Sigue sin probar corrección absoluta: prueba equivalencia con
un código que se considera correcto por otras razones.

**Cuándo.** Antes de cerrar cada hito, y en CI si hay `gfortran` disponible.

**Estado actual medido.** 26 de 26 configuraciones dentro de tolerancia, con el
número de pasos coincidiendo exactamente en todas, hasta 2673 pasos.

## 5. Validación analítica

**Qué es.** Comparación contra la solución exacta del problema de Riemann,
calculada independientemente.

**Qué prueba.** Que el código resuelve las ecuaciones que dice resolver. Es el
único nivel que prueba corrección en sentido fuerte, porque la referencia no
viene de ningún código: viene de la matemática.

**Qué NO prueba.** Nada sobre casos que no tengan solución analítica. Y la
comparación está limitada por el error de discretización, que no es un fallo sino
el objeto de estudio.

**Referencia externa disponible.** Los valores \(p^*\) y \(u^*\) de la Tabla 4.3
de Toro para las cinco configuraciones clásicas. Es la única referencia del
proyecto que **no** proviene ni del Fortran ni del C++, y por eso es la más
valiosa: rompe cualquier circularidad.

## 6. Análisis de convergencia

**Qué es.** Refinamiento sistemático de la malla y estimación del orden observado.

**Qué prueba.** Que la implementación tiene el orden de precisión que su teoría
predice. Es la verificación más rigurosa que existe para un esquema numérico,
porque es muy difícil que una implementación con un error produzca el orden
correcto por casualidad.

**Qué NO prueba.** Nada si el caso tiene discontinuidades: ahí el orden formal no
se alcanza y la medición no distingue un esquema correcto de uno con un error.
Por eso el orden se mide **solo sobre soluciones suaves**.

**Criterio.** \(p_{obs} \ge 0.9\,p_{teórico}\), con al menos cuatro resoluciones.

**Importancia.** Es lo que da derecho a comparar esquemas entre sí. Sin él, la
matriz de comparación compara dos implementaciones que podrían estar ambas mal.

## 7. Pruebas de consistencia física

**Qué son.** Comprobación de propiedades que la física impone.

**Alcance.** Conservación de masa y energía; positividad estricta de densidad y
presión; que un equilibrio exacto se mantenga exacto; simetrías del problema.

**Qué prueban.** Que el resultado es físicamente admisible.

**Qué NO prueban.** Que sea el resultado correcto. Un estado físicamente
admisible puede estar completamente equivocado.

**Caso 2D específico.** Sin solución analítica disponible, las simetrías se
convierten en la herramienta principal de verificación. La auditoría del solver
2D de referencia usó tres, y las tres son buenas ideas para adoptar:

| Prueba de simetría | Qué detecta |
|---|---|
| Simetría radial en la explosión cilíndrica | Anisotropía del esquema. Se mide como dispersión relativa en cascarones de radio |
| Transposición x↔y con u↔v | Error en la rotación o en la de-rotación del solver direccional |
| Sod a lo largo de x contra Sod a lo largo de y | Diferencia de tratamiento entre las dos direcciones |

La segunda es especialmente valiosa: el auditor la verificó a \(10^{-16}\), lo
que descarta prácticamente cualquier error en el empaquetado y desempaquetado de
las componentes.

## 8. Qué nivel se necesita para afirmar qué

Tabla de correspondencia entre afirmaciones y evidencia. Útil al redactar la
memoria de tesis.

| Para afirmar… | Hace falta |
|---|---|
| «el componente X hace lo que su contrato dice» | Unitaria |
| «el ensamblaje es coherente» | Integración |
| «no rompimos nada» | Regresión |
| «es equivalente al código del director» | Validación directa |
| «resuelve correctamente las ecuaciones de Euler» | Validación analítica **y** convergencia |
| «el esquema tiene orden 2» | Convergencia sobre solución suave |
| «HLLC resuelve el contacto mejor que HLLE» | Convergencia de ambos **más** comparación cuantitativa |
| «el resultado es reproducible» | Determinismo **y** registro de ejecución **y** la prueba desde cero |

La penúltima fila es la afirmación central de la tesis, y la tabla muestra que
requiere dos niveles: no basta comparar, hay que haber verificado antes.

---

# Parte II — Política de datos de referencia

Esta parte existe para prevenir un fallo concreto: **que el C++ acabe validándose
contra su propia salida**. Es un error fácil de cometer, silencioso, y que
invalidaría la estrategia de validación completa.

## Clasificación por procedencia

Todo dato de referencia del proyecto pertenece a **exactamente una** de estas
cuatro clases, y la clase determina para qué sirve.

### Clase A — Referencia externa

**Origen.** Literatura publicada, independiente de cualquier código del proyecto.

**Ejemplos.** Los valores \(p^*\) y \(u^*\) de la Tabla 4.3 de Toro. Las
soluciones de referencia de Lax-Liu. La tasa de crecimiento lineal
\(\sigma = \sqrt{Agk}\) de Chandrasekhar.

**Sirve para.** Validación en sentido fuerte. Es la clase más valiosa porque
rompe toda circularidad.

**Ubicación.** Codificada en los tests, con cita de la fuente, página y tabla.

### Clase B — Solución analítica calculada

**Origen.** Calculada por el solver exacto del proyecto, a partir de la
formulación matemática.

**Sirve para.** Verificación y análisis de convergencia.

**Advertencia que hay que tener presente.** Esta clase es *casi* independiente,
pero no del todo: la calcula código nuestro. Por eso el solver exacto se valida
contra la Clase A antes de usarse como referencia de nada más. Ese es el orden
correcto: Clase A valida el solver exacto, y el solver exacto valida el esquema.

**Ubicación.** Generada en cada ejecución, en `reference/` del directorio de
salida. **No se versiona.**

### Clase C — Salida archivada del Fortran

**Origen.** Ejecución del solver Fortran del director.

**Sirve para.** Regresión y validación directa. **No** para afirmar corrección
absoluta.

**Ubicación.** `tests/reference/`, versionada, marcada como binaria en
`.gitattributes`, **solo lectura**.

**Reglas estrictas:**

- Solo se regenera ejecutando el Fortran, nunca el C++.
- Cada directorio conserva el `input.par` que la produjo y el `log.txt` con el
  recuento de pasos.
- **No contiene `run_info.txt`**, porque incluye el tiempo de pared y cambiaría
  en cada regeneración.
- Se documenta de qué versión del Fortran proviene. Con el hallazgo de que el
  `Code_Euler1D` recibido está un commit por detrás del que el director tiene,
  esto pasa de recomendable a necesario.

### Clase D — Salida del propio C++ congelada

**Origen.** Ejecución del framework, guardada como línea base.

**Sirve para.** Detectar regresiones **y nada más.**

**No sirve para.** Validación, en ningún sentido. Comparar el C++ contra su
propia salida anterior solo demuestra que no ha cambiado.

**Ubicación.** `PROPOSED` — un directorio **separado**, `tests/baseline/`,
nunca mezclado con `tests/reference/`.

## La regla de separación de directorios

```
tests/
├── reference/      Clase C: SOLO salida del Fortran. Nunca se escribe desde C++
└── baseline/       Clase D: líneas base del C++. Nunca se llama "referencia"
```

Es una barrera física, no una convención. Mezclar las dos clases en el mismo
directorio es exactamente cómo ocurre el fallo de autovalidación: alguien
regenera «la referencia» con el código que quería validar, y a partir de ahí todo
pasa siempre.

**Salvaguarda propuesta:** el test de regresión aborta si encuentra en
`tests/reference/` un archivo con marca de haber sido escrito por el C++. El
`run_info.txt` sirve de delator, porque solo el C++ lo produce.

## Cómo se determinan las tolerancias

**Principio.** Una tolerancia se justifica razonando sobre el origen del error,
no eligiendo un número que haga pasar el test.

### Tolerancia mixta

Para comparar campos se usa:

\[ \max|diff| \le \varepsilon_{abs} + \varepsilon_{rel}\cdot\max|ref| \]

con \(\varepsilon_{abs} = 10^{-12}\) y \(\varepsilon_{rel} = 10^{-9}\).

La parte absoluta no es opcional. Con un criterio puramente relativo, un campo
cuya referencia es casi nula —una velocidad que debería ser exactamente cero en
un problema simétrico— produce un error relativo enorme a partir de una
diferencia físicamente irrelevante. Este problema se encontró en la práctica con
el caso hidrostático.

### Órdenes de magnitud razonables por origen

| Origen del error | Tolerancia razonable |
|---|---|
| Una o dos operaciones aritméticas | \(10^{-15}\), o exacto |
| Copia de valores (celdas fantasma, pisos) | **Exacto**: tolerancia cero |
| Cancelación catastrófica, como \(E - \rho u^2/2\) | \(10^{-14}\) |
| Cadena de ~10 operaciones sobre magnitudes O(10–1000) | \(10^{-13}\) |
| Suma de \(N\) términos (masa, energía total) | \(10^{-12}\) relativo |
| Valor publicado con 6 cifras significativas | \(10^{-5}\) **relativo** |
| Resultado de una simulación completa | Se mide antes de fijarlo |

### Método para la última fila

Escribir el test con una tolerancia absurdamente estricta, verlo fallar, leer el
valor real que reporta el arnés, y **entonces** fijar el umbral con margen
razonado. Así el número queda justificado en lugar de adivinado, y queda
registrado cuál era el valor real en el momento de fijarlo.

## Diferencias de punto flotante: cuándo son aceptables

| Situación | Aceptable | Nota |
|---|---|---|
| Coincidencia byte a byte | Ideal | Se logra hoy en 9 de 13 casos |
| Diferencia en el último bit, mismo número de pasos | Sí | Distinta asociación en una suma, o distinta función intrínseca del compilador |
| Diferencia creciente con el tiempo, mismo número de pasos | Con cautela | Divergencia caótica desde una diferencia de redondeo. Hay que comprobar que la magnitud es consistente con eso |
| **Distinto número de pasos** | **No** | Se alteró el cálculo de `dt` o la trayectoria. Hay que explicarlo |
| Diferencia que depende del nivel de optimización | **No** | Comportamiento indefinido, o una opción que relaja la aritmética |
| Diferencia que depende del número de hilos | **No** | Reducción mal hecha o carrera de datos |

Las tres últimas filas no son «tolerancia excedida»: son defectos que hay que
corregir antes de continuar.

---

# Parte III — Matriz de validación

`PROPOSED` — qué se ejecuta en cada nivel.

## Verificación 1D (contra solución analítica)

| Caso | Estado | Nivel de referencia |
|---|---|---|
| Sod clásico | **Falta** (FR-026) | Clase A + B |
| Sod modificado de Toro | Disponible | Clase A + B |
| Lax | **Falta** (FR-026) | Clase A + B |
| Problema 123 (dos rarefacciones) | Disponible | Clase A + B |
| Blast izquierdo | Disponible | Clase B |
| Doble choque | Disponible | Clase A + B |
| Contacto casi estacionario | Disponible | Clase B |
| Onda de entropía (suave) | Disponible | Clase B — **el caso de convergencia** |

## Validación 2D

| Caso | Estado | Nivel de referencia |
|---|---|---|
| Lax-Liu, configuración 3 | Pendiente (FR-027) | Clase A + C |
| Explosión cilíndrica | Pendiente | Clase C + simetría radial |
| Reflexión de Mach | Punto abierto `B-2` | Clase A |

## Matriz de comparación de esquemas

El entregable que responde la pregunta de investigación (FR-035). Caso y malla
fijos, variando una sola decisión:

| | HLL | HLLE | HLLC |
|---|---|---|---|
| Orden 1 | — | — | — |
| MUSCL + van Leer | — | — | — |
| WENO | — | — | — |

Cada celda: \(L_1\) y \(L_2\) contra la solución exacta, y anchura del contacto
medida en celdas.

**Caso recomendado:** el de contacto casi estacionario. Un contacto que apenas se
mueve es donde los esquemas de dos ondas más lo difuminan y donde el de tres
ondas más gana, así que es el que produce la diferencia más visible y más fácil
de defender.

**Prerrequisito no negociable.** Ninguna celda de esta tabla se rellena antes de
que el esquema correspondiente haya superado su análisis de convergencia. El
orden es: verificar cada uno, después comparar.

**Punto abierto `B-4`** — si se exige la matriz completa o basta con aislar HLLE
contra HLLC con el reconstructor fijo.
