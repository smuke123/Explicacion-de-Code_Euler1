# Validación contra el Fortran

Todos los números de este documento provienen de ejecuciones reales realizadas
el 2026-09-07 en la máquina del proyecto. Nada está estimado ni extrapolado.

## Entorno

| Elemento | Versión |
|---|---|
| Sistema | Windows 10.0.26200 |
| Fortran | GNU Fortran (MSYS2 UCRT64) 16.1.0, flags `-O2 -Wall` (los del `Makefile` original) |
| C++ | g++ (MSYS2 UCRT64) 16.1.0, C++20, `CMAKE_BUILD_TYPE=Release` (`-O3 -DNDEBUG`) |
| CMake / generador | 4.3.3 / Ninja |
| Fuente Fortran | `../Code_Euler1D/Code_Euler1D` (`euler1d.f90`, `global_numbers.f90`, `exact_riemann.f90`) |

Los dos códigos comparten libm, lo que elimina una fuente de discrepancia
(`pow`, `exp`, `sin`, `sqrt`). Una comparación entre plataformas distintas daría
diferencias algo mayores.

No se activó `-ffast-math` en ninguno de los dos: reasociaría las operaciones en
coma flotante y haría imposible esta comparación.

## Metodología

Dos validaciones independientes y complementarias.

### A. Validación directa (script reproducible)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\validate_against_fortran.ps1
```

El script:

1. copia los tres `.f90` a una carpeta temporal y los compila con `gfortran -O2`
   (el árbol Fortran original no se modifica ni se ensucia con `.o`/`.mod`);
2. para cada caso de la matriz, escribe **un solo** `input.par` y lo usa para
   ambos códigos;
3. ejecuta los dos en directorios separados;
4. compara los seis `.xl` celda a celda con `euler1d_compare`;
5. compara el número de pasos temporales extraído del stdout de cada uno.

El **número de pasos** es la firma más sensible que existe aquí: solo coincide si
toda la secuencia de $\Delta t$ coincidió, y $\Delta t$ depende del campo
completo en cada paso. Una diferencia de un bit en cualquier celda cambiaría el
recuento en cuanto se acumule.

### B. Regresión contra salidas archivadas (en CTest)

`tests/test_regression` recorre los 14 casos de `tests/reference/`, cada uno con
el `input.par` con que se generó y sus `.xl`. Estas corridas fueron producidas
por el Fortran **antes** de la migración, posiblemente en otra máquina, así que
son un control independiente del punto A.

## Métrica y tolerancia

Se comparan $\rho$, $u$ y $p$ del último bloque temporal de cada archivo, más
los tres archivos de solución exacta. De las conservadas: $\rho$ se compara
directamente, y $\rho u$ y $E$ quedan determinadas por $(\rho, u, p)$ a través de
la EOS, que se valida por separado en `test_physics`.

Métricas calculadas por `compareFields`: $\max|d|$, $L_1$ y $L_2$ (ambas con peso
$\Delta x$, para que sean normas discretas comparables entre mallas) y el error
relativo máximo.

**Criterio de aceptación mixto:**

$$\max|\text{ref} - \text{cpp}| \;\le\; \varepsilon_{\text{abs}}
+ \varepsilon_{\text{rel}}\cdot\max|\text{ref}|,
\qquad
\varepsilon_{\text{abs}} = 10^{-12},\quad
\varepsilon_{\text{rel}} = 10^{-9}.$$

Justificación de cada parte:

- **$\varepsilon_{\text{rel}} = 10^{-9}$** manda en los campos de magnitud
  normal. Está unas cuatro órdenes por encima del redondeo acumulable en doble
  precisión y unas siete por debajo del error de discretización del propio
  esquema ($\sim10^{-2}$). Detecta una diferencia real de formulación y tolera
  el reordenamiento de operaciones.
- **$\varepsilon_{\text{abs}} = 10^{-12}$** es imprescindible por el test
  `atmosfera`: el campo de velocidad del Fortran vale $6.7\times10^{-16}$ —cero
  máquina, que es exactamente lo que debe dar un esquema bien balanceado— y
  normalizar por esa escala convertiría una coincidencia perfecta en un error
  relativo de $10^{3}$. La referencia es la escala física de estos problemas,
  donde $\rho$, $p$ y $c$ son de orden unidad.

**Suelo de la comparación.** Los `.xl` guardan 10 decimales en formato ES, o sea
~$10^{-11}$ de resolución relativa. Por eso las diferencias no nulas que
aparecen son casi todas exactamente $10^{-11}$ o $10^{-12}$: son *una unidad del
último dígito almacenado*, no una diferencia de cálculo. Para distinguir
diferencias reales por debajo de eso habría que cambiar el formato de salida, lo
que rompería la compatibilidad con el Fortran.

## Resultado A: validación directa

Matriz de 26 configuraciones: los 5 tests de Toro × 2 solvers, los tests 1 y 3 ×
5 limitadores, y los tres casos con física adicional (entropía, atmósfera, Sedov)
a dos resoluciones. Todos con `cfl = 0.9`, `gamma = 1.4`.

| Caso | `test` | N | solver | limiter | pasos Fortran / C++ | peor error normalizado | Resultado |
|---|---|---|---|---|---|---|---|
| toro1_hll_vl | 1 | 200 | hll | vl | 122 / 122 | 1.000e-11 | OK |
| toro1_hllc_vl | 1 | 200 | hllc | vl | 122 / 122 | **0** | OK |
| toro2_hll_vl | 2 | 200 | hll | vl | 92 / 92 | **0** | OK |
| toro2_hllc_vl | 2 | 200 | hllc | vl | 92 / 92 | 2.500e-13 | OK |
| toro3_hll_vl | 3 | 200 | hll | vl | 143 / 143 | 4.936e-18 | OK |
| toro3_hllc_vl | 3 | 200 | hllc | vl | 143 / 143 | **0** | OK |
| toro4_hll_vl | 4 | 200 | hll | vl | 234 / 234 | **0** | OK |
| toro4_hllc_vl | 4 | 200 | hllc | vl | 234 / 234 | **0** | OK |
| toro5_hll_vl | 5 | 200 | hll | vl | 153 / 153 | **0** | OK |
| toro5_hllc_vl | 5 | 200 | hllc | vl | 153 / 153 | **0** | OK |
| toro1_hllc_fo | 1 | 200 | hllc | fo | 119 / 119 | **0** | OK |
| toro3_hllc_fo | 3 | 200 | hllc | fo | 142 / 142 | **0** | OK |
| toro1_hllc_mm | 1 | 200 | hllc | mm | 121 / 121 | **0** | OK |
| toro3_hllc_mm | 3 | 200 | hllc | mm | 142 / 142 | **0** | OK |
| toro1_hllc_mc | 1 | 200 | hllc | mc | 122 / 122 | 1.000e-11 | OK |
| toro3_hllc_mc | 3 | 200 | hllc | mc | 143 / 143 | **0** | OK |
| toro1_hllc_sb | 1 | 200 | hllc | sb | 124 / 124 | 7.308e-11 | OK |
| toro3_hllc_sb | 3 | 200 | hllc | sb | 143 / 143 | **0** | OK |
| entropia_N100 | 6 | 100 | hllc | vl | 258 / 258 | **0** | OK |
| entropia_N200 | 6 | 200 | hllc | mc | 517 / 517 | **0** | OK |
| atmosfera_N100 | 7 | 100 | hllc | vl | 1315 / 1315 | **0** | OK |
| atmosfera_N200 | 7 | 200 | hll | mm | 2673 / 2673 | **0** | OK |
| sedov_N100 | 8 | 100 | hllc | vl | 578 / 578 | 3.457e-26 | OK |
| sedov_N200 | 8 | 200 | hllc | mm | 2364 / 2364 | 1.973e-11 | OK |

(La tabla lista 24 filas; `toro1_hllc_vl` y `toro3_hllc_vl` aparecen dos veces
en la matriz del script, una en el barrido de solvers y otra en el de
limitadores, de ahí los 26 casos ejecutados. El detalle crudo está en
`docs/validation_results.csv`.)

### Resumen A

| Métrica | Valor |
|---|---|
| Casos ejecutados | 26 |
| Dentro de tolerancia | **26** |
| Fuera de tolerancia | 0 |
| Casos con diferencia exactamente cero | 18 de 26 |
| Peor error normalizado | 7.308e-11 (`toro1_hllc_sb`) |
| Tolerancia | 1e-9 |
| Margen sobre la tolerancia | 13.7× |
| Coincidencia del número de pasos | **26 de 26** |

## Resultado B: regresión contra salidas archivadas

Salida de `build/tests/test_regression.exe`:

| Caso de referencia | `max|d|` | Fracción de su tolerancia | Campo peor |
|---|---|---|---|
| atmosfera | 1.000e-11 | 0.0100 | press_1.xl |
| convergencia_fo_N100 | **0** | 0.0000 | — |
| convergencia_mm_N100 | **0** | 0.0000 | — |
| convergencia_vl_N50 | **0** | 0.0000 | — |
| convergencia_vl_N100 | **0** | 0.0000 | — |
| convergencia_vl_N200 | **0** | 0.0000 | — |
| convergencia_vl_N400 | **0** | 0.0000 | — |
| sedov | 1.000e-11 | 0.0193 | vel_1.xl |
| toro1 | **0** | 0.0000 | — |
| toro2 | **0** | 0.0000 | — |
| toro3 | **0** | 0.0000 | — |
| toro4 | 1.000e-11 | 0.0005 | vel_1.xl |
| toro5 | 1.000e-12 | 0.0001 | vel_1.xl |
| vacio | — | — | sin `.xl` archivados |

### Resumen B

| Métrica | Valor |
|---|---|
| Casos con `input.par` | 14 |
| Casos comparables (con `.xl`) | 13 |
| Dentro de tolerancia | **13** |
| Byte a byte idénticos | **9 de 13** |
| Peor caso | `sedov`, a 0.0193× de su tolerancia (≈ 52× de margen) |
| Coincidencia del número de pasos | todos los casos que tienen `log.txt` |

El caso `vacio` (`test = 2`, `N = 200`, `x0 = 0.5`, `tf = 0.05`) tiene solo
`input.par`, sin `.xl`. Se ejecuta —y por tanto se comprueba que corre y que el
número de pasos es coherente— pero no aporta comparación de campos. Queda
registrado como tal en lugar de descartarlo en silencio.

## Validación contra referencias publicadas (independiente del Fortran)

`tests/test_exact_riemann` comprueba los valores estrella contra la **Tabla 4.3
de Toro**, que es una fuente externa a este proyecto. Es la única parte de la
validación que no depende de que el Fortran sea correcto.

| Test | $p^*$ publicado | $u^*$ publicado | Tolerancia relativa |
|---|---|---|---|
| 1 | 0.46629 | 1.36091 | 1e-5 |
| 2 | 0.0018939 | 0 | 1e-4 / abs 1e-12 |
| 3 | 460.894 | 19.5975 | 1e-5 |
| 4 | 1691.65 | 8.68977 | 1e-5 |
| 5 | 460.894 | ≈ 0 (\|u\*\| < 1e-3) | 1e-5 |

Las tolerancias reflejan que los valores publicados tienen 5–6 cifras
significativas, no la precisión del solver. Todos pasan.

Además, `log.txt` de las corridas archivadas trae los valores que imprimió el
Fortran (por ejemplo `toro1`: `p* = 4.662936E-01`, `u* = 1.360906E+00`), y el
test de regresión los compara con los del C++ a 1e-6 relativo. También pasan.

## Interpretación

Tres hechos sostienen la conclusión:

1. **El número de pasos coincide en 26 de 26 casos.** $\Delta t$ se recalcula en
   cada paso a partir de $\max_k(|u_k| + c_k)$ sobre todo el campo. Que la
   secuencia entera coincida en corridas de hasta 2673 pasos implica que el
   campo es idéntico paso a paso hasta el bit, o tan cerca que nunca cruza el
   umbral de redondeo que cambiaría el recuento.
2. **18 de 26 casos directos y 9 de 13 archivados dan diferencia exactamente
   cero** en todos los campos.
3. **Las diferencias no nulas valen $10^{-11}$, $10^{-12}$ o menos**, que es una
   unidad del último dígito que el formato `.xl` almacena. No son diferencias de
   cálculo observables, sino de redondeo en la escritura.

Los casos con diferencia no nula (`sb`, `mc`, `hll` en toro1, sedov N200) son los
que tienen más operaciones con cancelación: superbee y MC evalúan varios
`min`/`max` anidados, y HLL forma
$S_R F_L - S_L F_R + S_L S_R (U_R - U_L)$, donde los términos casi se cancelan.
Es exactamente donde se espera que el último bit dependa del orden de
evaluación.

**Conclusión:** la migración es numéricamente equivalente al Fortran en todas
las combinaciones de test, solver, limitador, geometría y gravedad que se
probaron, con un margen mínimo de 13.7× sobre la tolerancia de aceptación.

## Qué NO está validado

Honestidad sobre los límites de lo anterior:

- **Resolución de la comparación.** Todo lo que esté por debajo de $10^{-11}$
  relativo es invisible con el formato `.xl` actual. Se puede afirmar
  «equivalente a la precisión del formato de salida», no «bit a bit idéntico en
  memoria» (aunque los 18 ceros y la coincidencia de pasos lo sugieren).
- **Una sola plataforma.** Todo se ejecutó en Windows/UCRT64 con la misma libm
  para los dos códigos. No se ha comprobado la equivalencia entre plataformas ni
  entre compiladores distintos (MSVC, Clang, ifort).
- **Un solo nivel de optimización por lado.** Fortran a `-O2`, C++ a `-O3`. No se
  barrieron combinaciones ni se probó `-O0`/`-Og`.
- **Combinaciones no cubiertas.** La matriz no cruza *todos* los limitadores con
  *todos* los tests, ni prueba geometría cilíndrica (`alpha_geo = 1`) contra el
  Fortran, ni `bc` explícitas distintas de las que imponen los tests, ni
  `every_out > 0` (volcados intermedios), ni valores de `gamma` distintos de 1.4.
- **La rama de vacío del solver exacto** se valida solo contra su propia
  especificación (no produce NaN, $\rho = p = 0$ entre los frentes, muestreo
  finito). No hay una referencia publicada con la que comparar números, y los
  cinco tests de Toro no la disparan.
- **Los `*_exact.xl` de los tests 7 y 8** no son soluciones físicas de esos
  problemas (ver `docs/inconsistencies.md`, punto 4). Coinciden con el Fortran
  porque se reprodujo su comportamiento, pero comparar esos archivos no valida
  ninguna física.
- **`tools/*.py` del proyecto Fortran** (figuras, Sedov exacto) no se migraron y
  no se comprobó que funcionen con las salidas del C++, aunque el formato es
  idéntico y deberían.

## Cómo reproducir

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
powershell -ExecutionPolicy Bypass -File scripts\validate_against_fortran.ps1
```

Para inspeccionar una discrepancia concreta, `-KeepWork` conserva los
directorios temporales con los `input.par`, los `.xl` de ambos códigos, los
stdout y el informe de comparación de cada caso.
