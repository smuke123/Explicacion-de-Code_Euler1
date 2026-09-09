# Euler1D_Cpp

Solver 1D de las ecuaciones de Euler para flujo compresible de gas ideal, por
volúmenes finitos tipo Godunov. Es la migración a C++20 del proyecto Fortran
`Code_Euler1D`, con la misma formulación matemática y resultados numéricamente
equivalentes (ver [docs/validation.md](docs/validation.md)).

## Qué resuelve

Las ecuaciones de Euler en forma conservativa con términos fuente:

$$\partial_t U + \partial_x F(U) = S$$

con variables conservadas y flujo físico

$$U = \begin{pmatrix}\rho \\ \rho u \\ E\end{pmatrix},\qquad
F(U) = \begin{pmatrix}\rho u \\ \rho u^2 + p \\ u(E+p)\end{pmatrix},$$

cerradas por la ecuación de estado de gas ideal politrópico

$$E = \frac{p}{\gamma - 1} + \frac{1}{2}\rho u^2 .$$

El término fuente $S$ cubre gravedad constante ($S = (0, -\rho g, -\rho g u)$) y
geometría cilíndrica o esférica.

### Métodos implementados

| Componente | Opciones |
|---|---|
| Reconstrucción | MUSCL de 2.º orden, o 1.er orden (`fo`) |
| Limitadores | `fo`, `mm` (minmod), `vl` (van Leer), `mc` (monotonized central), `sb` (superbee) |
| Solver de Riemann | `hll`, `hllc` |
| Integración temporal | SSP-RK2 (Heun); `ForwardEuler` disponible |
| Paso de tiempo | Adaptativo por CFL |
| Fronteras | `trans` (transmisiva), `per` (periódica), `refl` (pared) |
| Geometría | plana, cilíndrica, esférica (forma conservativa) |
| Referencia | solver de Riemann **exacto** (Toro cap. 4) |

Detalles matemáticos en [docs/numerical_method.md](docs/numerical_method.md).

## Compilación

Requiere un compilador con C++20 y CMake ≥ 3.20. No hay dependencias externas.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

En Windows con MSYS2 UCRT64, añadiendo el toolchain al `PATH`:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Guía detallada de MSYS2 (qué shell usar, qué paquetes instalar, cómo depurar y
graficar): **[docs/msys2_guide.md](docs/msys2_guide.md)**.

Produce dos ejecutables en `build/`: `xEuler1D` (el solver) y `euler1d_compare`
(comparador cuantitativo de dos corridas).

Opciones de CMake:

| Opción | Por defecto | Efecto |
|---|---|---|
| `EULER1D_BUILD_TESTS` | `ON` | Compila la suite de tests |
| `EULER1D_ENABLE_OPENMP` | `OFF` | Enlaza OpenMP (ver `docs/performance.md`) |

> No se activa `-ffast-math` ni `-Ofast`: reasociarían las operaciones en coma
> flotante y romperían la equivalencia numérica con el Fortran, que es el
> criterio de aceptación de la migración.

## Ejecución

```bash
./build/xEuler1D                                   # lee ./input.par
./build/xEuler1D -i examples/toro2_casi_vacio.par -o output/toro2
./build/xEuler1D --help
```

| Argumento | Significado |
|---|---|
| `-i, --input <archivo>` | Namelist a leer (por defecto `input.par`) |
| `-o, --output <dir>` | Directorio de salida (por defecto el actual) |
| `-q, --quiet` | No imprimir progreso |

### Archivos de salida

En el directorio de salida se escriben `rho_1.xl`, `vel_1.xl`, `press_1.xl`
(solución numérica), sus equivalentes `*_exact.xl` (solución de referencia) y
`run_info.txt` con la configuración efectiva, el número de pasos y el tiempo de
ejecución.

El formato de los `.xl` es **idéntico al del Fortran** para que las dos
implementaciones se puedan comparar directamente: cada instante volcado es un
bloque con cabecera `#Time = ` seguido de filas `x  valor` en formato
`es20.10e3`, y una línea en blanco al final.

## Configuración

Se usa el mismo namelist Fortran `&euler` del proyecto original, de modo que
cualquier `input.par` existente funciona sin editarlo. Cambiar la configuración
no requiere recompilar.

```fortran
&euler
 test = 1
 N = 400
 cfl = 0.9
 gamma = 1.4
 limiter = 'vl'
 solver = 'hllc'
/
```

| Clave | Tipo | Por defecto | Significado |
|---|---|---|---|
| `test` | entero 1–8 | 1 | Caso de prueba |
| `N` | entero ≥ 4 | 400 | Celdas físicas |
| `cfl` | real en (0,1] | 0.9 | Número de Courant |
| `gamma` | real > 1 | 1.4 | Índice adiabático |
| `limiter` | `fo\|mm\|vl\|mc\|sb` | `'vl'` | Limitador de pendiente |
| `solver` | `hll\|hllc` | `'hllc'` | Solver de Riemann |
| `bc` | `trans\|per\|refl` | `'trans'` | Condición de frontera |
| `grav` | real | 0 | Gravedad en +x |
| `alpha_geo` | 0, 1 o 2 | 0 | Geometría: plana, cilíndrica, esférica |
| `xmin`, `xmax` | real | 0, 1 | Dominio |
| `x0` | real | *del test* | Posición del diafragma |
| `tf` | real | *del test* | Tiempo final |
| `every_out` | entero ≥ 0 | 0 | Volcados intermedios (0 = solo inicial y final) |
| `rho_floor`, `p_floor` | real > 0 | 1e-12 | Pisos de positividad |

`x0` y `tf` negativos son centinelas: significan «usa el valor por defecto del
test», igual que en el Fortran.

Una clave desconocida, un valor fuera de rango o una opción inexistente
producen un error explícito y abortan. **Ninguna opción inválida selecciona
otro algoritmo en silencio**, que era el fallo concreto que motivó esta parte
del diseño.

### Casos de prueba

| `test` | Problema | `x0` | `tf` | Impone |
|---|---|---|---|---|
| 1 | Sod modificado, rarefacción sónica | 0.3 | 0.2 | — |
| 2 | «123»: dos rarefacciones, casi vacío | 0.5 | 0.15 | — |
| 3 | Blast izquierdo (Woodward–Colella) | 0.5 | 0.012 | — |
| 4 | Colisión de dos choques | 0.4 | 0.035 | — |
| 5 | Test 3 desplazado: contacto casi estacionario | 0.8 | 0.012 | — |
| 6 | Onda de entropía suave (convergencia) | — | 1.0 | `bc='per'` |
| 7 | Atmósfera hidrostática isoterma | — | 10.0 | `bc='refl'`, `grav=1` si es 0 |
| 8 | Sedov–Taylor esférico | — | 0.5 | `bc='refl'`, `alpha_geo=2`, `xmax=1.2` |

Los tests 1–5 son la Tabla 4.1 de Toro. Cuando un test sobreescribe una opción
que el usuario fijó explícitamente, se avisa por pantalla (el Fortran lo hacía
en silencio).

Hay configuraciones listas en [`examples/`](examples).

## Tests

```bash
ctest --test-dir build --output-on-failure
```

87 casos en seis ejecutables: unitarios (`test_physics`, `test_numerics`,
`test_exact_riemann`, `test_config`), integración (`test_integration`) y
regresión contra las salidas archivadas del Fortran (`test_regression`).
Detalles y tolerancias en [docs/testing.md](docs/testing.md); cómo escribir tus
propias pruebas, en [docs/extending.md](docs/extending.md).

## Validación contra el Fortran

```powershell
powershell -ExecutionPolicy Bypass -File scripts\validate_against_fortran.ps1
```

El script compila el proyecto Fortran en una carpeta temporal (sin tocar el
árbol original), corre los dos códigos con el mismo `input.par` sobre una matriz
de 26 configuraciones y compara los `.xl` celda a celda.

Busca el Fortran primero en la carpeta hermana `../Code_Euler1D/Code_Euler1D` y,
si no está, en la copia archivada en [`reference_fortran/`](reference_fortran),
de modo que la validación funciona aunque solo se tenga este repositorio. Con
`-FortranSource <ruta>` se puede apuntar a otra copia.

Resultado medido: **26 de 26 casos dentro de tolerancia**, con el número de
pasos temporales coincidiendo exactamente en todos. Números completos en
[docs/validation.md](docs/validation.md).

## Documentación

| Documento | Contenido |
|---|---|
| [docs/msys2_guide.md](docs/msys2_guide.md) | Guía paso a paso con MSYS2 UCRT64: shell correcta, paquetes, compilar, ejecutar, depurar, graficar, problemas frecuentes |
| [docs/extending.md](docs/extending.md) | Cómo añadir tus propias pruebas, casos físicos y métodos numéricos |
| [docs/architecture.md](docs/architecture.md) | Componentes, responsabilidades, dependencias, flujo de datos |
| [docs/numerical_method.md](docs/numerical_method.md) | Volúmenes finitos, Godunov, MUSCL, limitadores, HLL/HLLC, RK2, CFL, fronteras |
| [docs/validation.md](docs/validation.md) | Validación cuantitativa contra el Fortran |
| [docs/testing.md](docs/testing.md) | Suite de pruebas y tolerancias |
| [docs/performance.md](docs/performance.md) | Regiones paralelizables y decisiones de rendimiento |
| [docs/inconsistencies.md](docs/inconsistencies.md) | Rarezas del código original y qué se hizo con cada una |
