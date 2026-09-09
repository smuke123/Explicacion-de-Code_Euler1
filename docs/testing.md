# Suite de pruebas

```bash
ctest --test-dir build --output-on-failure
```

87 casos repartidos en seis ejecutables. Estado actual: **todos pasan**, tanto
en Release (~11 s) como en Debug con los `assert` de rango activos (~41 s). El
tiempo lo domina la regresión, que ejecuta 14 simulaciones completas.

| Ejecutable | Casos | Tipo | Tiempo |
|---|---|---|---|
| `test_physics` | 15 | unitario: EOS, malla, campos, CFL, formato | 0.03 s |
| `test_numerics` | 21 | unitario: limitadores, reconstrucción, HLL/HLLC, fronteras | 0.03 s |
| `test_exact_riemann` | 12 | unitario contra referencia publicada | 0.04 s |
| `test_config` | 25 | unitario: parser, validación, resolución de casos | 0.04 s |
| `test_integration` | 12 | simulaciones completas | 0.30 s |
| `test_regression` | 2 (14 corridas) | regresión contra el Fortran archivado | 9.2 s |

## El arnés

No se usa GoogleTest ni Catch2. El encargo pide no introducir dependencias
externas innecesarias, y lo único que hace falta son casos registrados,
comparaciones con tolerancia y un código de salida para CTest. Eso son ~120
líneas en `tests/test_support.hpp`.

Cada archivo de test es un ejecutable independiente, así que un fallo por
`segfault` en un área no impide ver el resto.

Macros disponibles:

| Macro | Uso |
|---|---|
| `CHECK(cond)` | Condición booleana |
| `CHECK_NEAR(a, b, tol)` | Diferencia **absoluta** ≤ `tol` |
| `CHECK_REL(v, ref, tol)` | Diferencia **relativa** a `\|ref\|` ≤ `tol` |
| `CHECK_THROWS(expr, Tipo)` | La expresión lanza esa excepción |

Los fallos imprimen archivo, línea, ambos valores con 17 dígitos y la
diferencia frente a la tolerancia, para que el diagnóstico no requiera un
depurador.

## Qué se comprueba y con qué tolerancia

Las tolerancias no son números redondos elegidos al azar: cada una responde a
una fuente de error concreta.

### `test_physics` — física básica e infraestructura

| Qué | Tolerancia | Por qué ese valor |
|---|---|---|
| Ida y vuelta $W \to U \to W$ | 1e-15 (ρ, u), 1e-14 (p) | La presión sufre la cancelación $E - \rho u^2/2$ |
| $E = p/(\gamma-1) + \rho u^2/2$ | 1e-13 | Valor exacto conocido (21.5) |
| $c = \sqrt{\gamma p/\rho}$ | 1e-15 | Una sola raíz: precisión de máquina |
| Componentes de $F(U)$ | 1e-13 | Productos de magnitudes O(10) |
| Orden de los pisos | exacto (0.0) | El piso devuelve literalmente 1e-12 |
| Centros y caras de la malla | 1e-15 | Aritmética exacta en potencias de 2 |
| Fórmula del CFL | 1e-16 | Una división |
| Formato `es20.10e3` | igualdad de cadenas | Debe ser byte a byte el de Fortran |

Casos de robustez incluidos: campo en reposo y frío (no debe dividir por cero,
la semilla 1e-30 lo evita) y campo con NaN (**debe lanzar**).

### `test_numerics` — componentes numéricos

Los limitadores se comprueban contra sus fórmulas cerradas (1e-15) y contra la
propiedad TVD: los cuatro no triviales deben anularse **exactamente** cuando las
diferencias tienen signo opuesto. También se verifica el orden de compresividad
`mm ≤ vl ≤ mc ≤ sb` en una rampa monótona, y el convenio
`sign(1.0d0, 0.0d0) = +1` de Fortran.

La reconstrucción se comprueba con tres propiedades exactas:

- campo uniforme → estados de cara sin salto (1e-15);
- rampa lineal → el estado de cara cae en el valor analítico del borde
  (1e-14): es la definición de segundo orden;
- `fo` → estados de cara iguales a los promedios de celda (**exacto**);
- atmósfera en equilibrio discreto → los dos estados de cara coinciden (1e-15):
  la propiedad de buen balance a nivel de reconstrucción.

Los solvers de Riemann se comprueban por propiedades matemáticas, no por
valores memorizados:

- **consistencia**: $W_L = W_R \Rightarrow F = F(W)$ (1e-13);
- **upwind supersónico**: $S_L \ge 0 \Rightarrow F = F_L$ (1e-14);
- **contacto estacionario**: HLLC da flujo de masa cero (1e-14) y HLL no
  (se exige $|F_\rho| > 10^{-3}$, o sea que el test fallaría si alguien
  "arreglara" HLL y con ello dejara de ser HLL);
- **barrido vs. cara individual**: resultados **idénticos** (tolerancia 0.0), que
  es lo que garantiza que la ruta rápida y la legible no divergen;
- las velocidades de Davis acotan las características.

Las tres fronteras se comprueban con tolerancia **exacta** (0.0), porque copian
valores; solo el caso con gravedad usa 1e-15 por la aritmética del equilibrio.

### `test_exact_riemann` — contra referencia publicada

Es la única parte de la validación que **no depende de que el Fortran sea
correcto**: compara con la Tabla 4.3 de Toro. Tolerancia relativa 1e-5, que
refleja las 5–6 cifras significativas del libro, no la precisión del solver.

Además: el Newton converge en menos de 60 iteraciones en los cinco tests; la
semilla TRRS es positiva incluso en el test 2 ($p^* \approx 1.9\times10^{-3}$);
lejos del abanico el muestreo devuelve los estados iniciales; en la región
estrella $p$ y $u$ son constantes a ambos lados del contacto mientras $\rho$
salta; el muestreo es monótono dentro de la rarefacción; y las dos ramas de
$f_K$ (choque y rarefacción) empalman con continuidad $C^1$ en $p = p_K$.

La **región de vacío** tiene su propio caso: se construyen estados que disparan
el criterio 4.40 y se muestrean 97 valores de $s$ comprobando que todos son
finitos y no negativos. Sin la rama de vacío, esa formula devuelve NaN
silencioso.

### `test_config` — configuración

Un tercio de los casos son de **rechazo**, porque el requisito central es que
nada se degrade a un valor por defecto: clave desconocida, solver inexistente
(`'roe'`, `'hlle'`), limitador inexistente, frontera inexistente, `alpha_geo = 3`,
grupo `&euler` ausente o sin cerrar, cadena sin comillas, valor no numérico,
`N < 4`, `gamma ≤ 1`, `cfl` fuera de (0,1], `xmax ≤ xmin`, piso no positivo.

El resto cubre compatibilidad con el namelist Fortran (notación `0.5d0`, claves
insensibles a mayúsculas, comentarios `!`, comas separadoras) y la resolución de
los casos de prueba: centinelas negativos, valores explícitos del usuario que
deben sobrevivir, imposiciones de los tests 6/7/8 y el aviso cuando una de esas
imposiciones pisa una elección explícita.

### `test_integration` — simulaciones completas

| Caso | Qué comprueba | Umbral |
|---|---|---|
| toro1 termina en `tf` | El recorte del último paso | 1e-14 |
| toro1 genera los 7 archivos | Contrato de salida | existencia |
| Tests 1–5 completos | Estados finitos y positivos en todas las celdas | — |
| 5 limitadores × 2 solvers en toro1 | Las 10 combinaciones corren | — |
| Test 6 conserva la masa | Conservatividad de la actualización | 1e-12 rel. |
| Sedov conserva la energía | Forma conservativa de la geometría | 1e-8 rel. |
| Atmósfera en reposo | Buen balance | `max\|u\| < 1e-10` |
| $L_1$ decrece al refinar | Convergencia (N = 50, 100, 200) | monotonía |
| MUSCL vs. primer orden | Orden observado | `fo` ∈ (0.6, 1.4); MUSCL > 1.4 |
| HLLC vs. HLL en toro1 | HLLC tiene menor $L_1$ | comparación |
| Integrador intercambiable | Euler y RK2 sobre el mismo operador | dan distinto |
| `run_info.txt` | Registra la configuración **efectiva** | subcadenas |

Notas sobre los umbrales:

- **1e-12 en la masa** en lugar de exactamente cero: la suma telescópica es
  exacta, pero sumar $N$ términos acumula redondeo.
- **1e-8 en la energía del Sedov**: hay un choque fuerte y el ambiente tiene
  $p = 10^{-9}$; el margen frente al 42 % de energía espuria que produciría la
  forma no conservativa es de siete órdenes de magnitud.
- **`max|u| < 1e-10`** en la atmósfera: el valor medido es ~1e-15 (1.0094e-15
  con N = 200 y `tf = 10`), y un esquema no balanceado daría ~1e-3. El umbral
  está en medio, a cinco órdenes de cada extremo.
- **Los órdenes de convergencia** se acotan por arriba *y* por abajo para primer
  orden: un test que solo exigiera «> 0.6» pasaría si el esquema fuera
  accidentalmente de segundo orden, y entonces no estaría comprobando lo que
  dice.
- **El test del integrador exige que Euler y RK2 difieran**: si coincidieran, la
  segunda etapa no se estaría aplicando y el test pasaría por la razón
  equivocada.

### `test_regression` — contra el Fortran

Recorre `tests/reference/`, ejecuta cada `input.par` y compara los seis `.xl`
con el criterio mixto $10^{-12} + 10^{-9}\max|\text{ref}|$ descrito en
[validation.md](validation.md). Donde hay `log.txt` compara además el **número
de pasos** (igualdad exacta) y los valores $p^*, u^*$ (1e-6 relativo).

Los casos sin `.xl` archivados se reportan explícitamente como no comparados en
lugar de contarse como aprobados.

## Cómo añadir un test

```cpp
TEST(nombre_descriptivo_en_snake_case) {
    CHECK_NEAR(loQueCalculo(), valor_esperado, 1e-14);
}
```

Se registra solo. Para un archivo nuevo, añadir el nombre (sin `.cpp`) a
`EULER1D_TESTS` en `tests/CMakeLists.txt` y reconfigurar CMake.

La guía completa —cómo elegir la tolerancia, cómo escribir un test de
integración, cómo congelar un caso de regresión propio y cómo añadir un caso
físico nuevo— está en **[extending.md](extending.md)**.

## Cobertura: qué no cubre la suite

- **No hay medición de cobertura de líneas.** No se configuró `gcov`/`lcov`.
- **Geometría cilíndrica (`alpha_geo = 1`)** no tiene test propio: ningún caso de
  prueba la usa. `RadialDivergence` está escrito de forma genérica en $\alpha$ y
  se ejercita con $\alpha = 2$.
- **`every_out > 0`** (volcados intermedios) no se comprueba: los tests solo
  miran el último bloque de cada archivo.
- **Lectura de `.xl` malformados** (`xl_reader`) se prueba solo por la vía feliz.
- **Fugas de memoria y errores de acceso**: no se ejecuta bajo ASan/UBSan ni
  Valgrind. El código usa `std::vector` y `unique_ptr` exclusivamente, sin `new`
  ni `delete` desnudos, y `CellField::operator[]` tiene `assert` de rango que
  está activo en Debug.
- **Concurrencia**: no aplica todavía; el código es secuencial.
- **`main.cpp`** (parseo de argumentos) no tiene test: se ejercita a mano y desde
  el script de validación.
