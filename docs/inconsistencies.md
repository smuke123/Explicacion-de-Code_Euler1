# Rarezas del código original y qué se hizo con cada una

Este documento existe porque el criterio de aceptación de la migración es la
**equivalencia numérica con el Fortran**. Una decisión rara que se "arregle" en
silencio rompe esa equivalencia y hace imposible demostrar que la traducción es
correcta. Por eso todo lo que se encontró está aquí, clasificado en tres grupos:

- **Preservado** — se reproduce el comportamiento original, aunque sea discutible.
- **Reestructurado** — cambia la organización del código, no el resultado
  numérico (verificado en [validation.md](validation.md)).
- **Desviación deliberada** — el comportamiento cambia, y aquí se justifica.

---

## Preservado

### 1. `sign(1.0d0, 0.0d0)` vale `+1`, no `0`

La intrínseca `sign` de Fortran devuelve $+1$ para argumento cero, a diferencia
de la función signo matemática. Aparece en minmod y superbee, donde
$\tfrac12(\mathrm{sgn}\,a + \mathrm{sgn}\,b)$ decide si la pendiente se anula.

Una implementación ingenua con `(x > 0) - (x < 0)` daría 0 en el cero y
cambiaría la pendiente limitada cuando una de las diferencias es exactamente
nula, cosa que ocurre a menudo en las zonas planas de un tubo de choque.

Replicado en `limiters::fortranSign`, con test propio.

### 2. La frontera transmisiva copia la celda de borde en **los dos** fantasmas

El Fortran escribe `Uc(:,-1) = Uc(:,1)`, no `Uc(:,2)`. Es extrapolación
constante, no lineal: la pendiente reconstruida en la cara extrema se anula.

Podría argumentarse que `Uc(:,-1) = Uc(:,2)` (extrapolación de orden superior)
es "mejor", pero cambiaría los resultados en las celdas del borde. Se preserva
literalmente y se documenta en el código.

### 3. El trabajo de la gravedad usa la velocidad del **inicio** del subpaso

En `rhs_fv`:

```fortran
Uout(2,k) = Uout(2,k) - dt*W(1,k)*grav
Uout(3,k) = Uout(3,k) - dt*W(1,k)*grav*W(2,k)
```

`W(2,k)` es la velocidad de $U_{in}$, no la de `Uout` recién calculada. Es una
elección legítima (tratamiento explícito de la fuente, consistente con el orden
de RK2), pero no es la única posible. Se conserva tal cual: cambiarla alteraría
el resultado en los últimos dígitos y rompería la equivalencia.

### 4. Los `*_exact.xl` de los tests 7 y 8 no significan nada

`set_test` asigna a los tests 7 y 8 unos estados izquierdo y derecho **idénticos**
($\rho = 1$, $u = 0$, $p = 1$), que son marcadores de posición porque esos casos
no usan un diafragma. Pero el cierre del programa hace:

```fortran
if (test == 6) then
   call save_exact_suave
else
   call exact_star(...)     ! con los estados L y R del test
   call save_exact(...)
end if
```

Para los tests 7 y 8 eso resuelve un problema de Riemann con $W_L = W_R$, cuya
solución es el estado uniforme $(1, 0, 1)$. Los archivos se escriben, pero **no
son la atmósfera hidrostática ni la solución de Sedov**: son constantes.

Se preserva el comportamiento (los archivos se siguen generando con el mismo
contenido, y por eso coinciden con el Fortran en la validación) y se avisa en
tres sitios: `examples/sedov_esferico.par`, este documento y
[validation.md](validation.md). La solución de Sedov requeriría el script
`tools/` del proyecto original, que no se migró.

### 5. Los pisos de positividad son parámetros de código, no del namelist

`rho_floor = p_floor = 1e-12` están fijados en el Fortran y no se pueden cambiar
sin recompilar. En C++ son campos de `SimulationConfig` **y** se pueden dar por
namelist (`rho_floor`, `p_floor`), pero los valores por defecto son los mismos.
Es una extensión que no altera ninguna corrida existente.

### 6. La presión inicial del test 7 usa el equilibrio **discreto**, no el continuo

El Fortran no escribe $p = \rho$ (el equilibrio continuo de una atmósfera
isoterma con $T_0 = 1$), sino la recurrencia trapezoidal

$$p_1 = \rho_1,\qquad p_k = p_{k-1} - g\,\Delta x\,\frac{\rho_{k-1}+\rho_k}{2}.$$

No es un descuido: es la misma cuadratura que usa el esquema, así que la
condición inicial está en equilibrio **exacto** para el operador discreto.
Imponer el equilibrio continuo dejaría un residuo $O(\Delta x^2)$ que el test
confundiría con error del esquema. Se preserva y se explica en el código.

### 7. `validate_input` se ejecuta **antes** que `set_test`

De modo que el `bc` que el usuario escribió se valida y luego los tests 6, 7 y 8
lo sobreescriben. La validación no es inútil (detecta una cadena inválida), pero
el valor validado puede no ser el que se use.

Se preserva el orden (validar → resolver), y además se valida **otra vez**
después de resolver, porque el test puede cambiar el dominio (`xmax = 1.2` en el
test 8). Ver el punto 12 para el aviso que se añadió.

---

## Reestructurado (sin cambio numérico)

### 8. `rhs_fv` reconstruía leyendo el array global `W`, no su argumento `Uin`

La firma era `rhs_fv(Uin, Uout)`, pero el bucle MUSCL leía `W(m,k)`, una variable
del programa principal. Pasar un `Uin` no sincronizado con `W` evaluaba el
operador espacial sobre un estado distinto del pedido, **sin ningún aviso**. El
código funcionaba porque el bucle temporal llamaba a `cons2prim_all` y `boundary`
entre etapas, es decir, por una convención que había que recordar.

En C++, `SemiDiscreteOperator::applyUpdate(u_in, w, dt, u_out)` recibe `w` como
parámetro explícito. La obligación está en la firma y no se puede olvidar.

### 9. Aliasing en la segunda etapa de RK2

Una versión anterior llamaba `rhs_fv(U1, U1)`: el mismo array como `intent(in)` e
`intent(out)`, que es comportamiento indefinido en Fortran. **El propio código
original ya lo detectó y lo corrigió**, dejando una nota de auditoría en
`euler1d.f90` líneas 96–99.

En C++ `SspRk2` tiene dos arrays de etapa separados como miembros, así que la
separación es estructural en lugar de una convención documentada.

### 10. La actualización cartesiana se calcula y se descarta en geometría radial

Con `alpha_geo > 0`, `rhs_fv` calcula `Uout = Uin - dt/dx*(F(k)-F(k-1))`, aplica
la gravedad, y a continuación **sobreescribe `Uout` por completo** con la forma
radial y vuelve a aplicar la gravedad. El primer cálculo es trabajo muerto.

En C++ son dos implementaciones de `DivergenceOperator` (`CartesianDivergence` y
`RadialDivergence`) y solo se ejecuta la que corresponde. El resultado numérico
es idéntico porque el valor descartado nunca influía.

### 11. Selección de algoritmo dentro de los bucles internos

`reconstructor` hacía `select case (trim(limiter))` por celda y por componente;
`riemann_flux` comparaba `trim(solver) == 'hll'` por cara. Son decisiones que
solo pueden cambiar entre corridas, evaluadas millones de veces por corrida.

En C++ la selección ocurre una vez, en `Simulation::buildComponents()`. Ver
[performance.md](performance.md).

### 12. Los tests sobreescriben la configuración del usuario en silencio

`set_test` asigna `bc = 'per'` (test 6), `bc = 'refl'` (tests 7 y 8),
`alpha_geo = 2` y `xmax = 1.2` (test 8), pisando lo que el usuario hubiera
puesto en `input.par` sin decir nada.

Se **preserva el comportamiento** (la configuración efectiva es la misma), pero
`SimulationConfig` registra qué campos fijó el usuario explícitamente y
`resolveTestCase` devuelve un informe. Si una imposición pisa una elección
explícita, se avisa por pantalla:

```
 AVISO: el test sobreescribe opciones dadas por el usuario:
  bc: 'trans' -> 'per' (impuesto por el test 6)
```

Además, `run_info.txt` guarda siempre la configuración **efectiva**, no la del
archivo de entrada.

### 13. Los pisos se aplicaban en silencio

`cons2prim_all` hace `max(..., floor)` sin dejar rastro. Un piso que salta una
vez es la protección prevista; uno que salta diez mil veces significa que la
simulación perdió validez física, y en el original no había forma de saberlo.

Se preserva **exactamente** la aritmética (mismo orden: piso en $\rho$ antes de
dividir, luego $u$, luego piso en $p$), pero se instrumenta con `FloorCounters`,
que cuenta activaciones y guarda la energía interna específica más negativa
vista. Si hubo alguna, la corrida lo avisa y lo registra en `run_info.txt`.

---

## Desviaciones deliberadas

### 14. `ultimo_guardado` se usaba sin inicializar

```fortran
integer :: i, nstep, ultimo_guardado          ! sin valor inicial
...
do while (t < tf)
   ...
   ultimo_guardado = -1                       ! se asigna DENTRO del bucle
   ...
end do
if (ultimo_guardado /= nstep) call save_output(1)
```

Si el bucle no llega a ejecutarse nunca (`tf ≤ 0`), la comparación final lee una
variable indefinida. El propio gfortran lo señala al compilar:

```
euler1d.f90:117:6: Warning: 'ultimo_guardado' may be used uninitialized
                            [-Wmaybe-uninitialized]
```

En C++ se inicializa a `-1` antes del bucle. Eso reproduce exactamente el caso
normal (`tf > 0`, que es el único que ocurre en los ocho tests) y hace
determinista el degenerado: con `tf ≤ 0` se escribe el estado inicial, que es lo
razonable. **Es la única forma de que el programa tenga comportamiento definido**;
mantener la indefinición no era una opción.

### 15. `'HLL'` en mayúsculas: el Fortran lo rechaza, el C++ lo acepta

El Fortran compara `trim(solver)` con `'hll'` de forma sensible a mayúsculas.
Históricamente eso significó que `solver = 'HLL'` corriera HLLC en silencio; la
versión auditada lo convirtió en un error, que es lo correcto.

El parser de C++ pasa el valor a minúsculas antes de comparar, así que acepta
`'HLL'`, `'Hll'` y `'hll'`. La justificación: los namelists de Fortran son
insensibles a mayúsculas en las **claves**, así que la asimetría con los valores
sorprende; y aceptar `'HLL'` nunca selecciona un algoritmo distinto del que el
usuario pidió, que es la propiedad que importaba. Un valor que no corresponde a
ningún algoritmo (`'roe'`, `'hlle'`) sigue siendo un error duro.

Hay un test explícito (`el_valor_del_solver_es_insensible_a_mayusculas`) para que
la decisión quede fijada y no se revierta por accidente.

### 16. Un NaN en el campo aborta en lugar de propagarse

`compute_dt` hace `smax = max(smax, abs(u) + a)`. Tanto `max` de Fortran como
`std::max` de C++ devuelven el primer argumento cuando el segundo es NaN: **un
NaN en el campo no llega al máximo**, `dt` sale finito y la simulación sigue
corriendo sobre basura hasta el final, escribiendo archivos llenos de NaN sin
haber avisado.

En C++, `CflController::scan` acumula además la suma de las velocidades de señal,
que sí propaga NaN e Inf, y `computeTimeStep` lanza una excepción si no es
finita. Cuesta una adición por celda.

Es una desviación: el Fortran habría seguido. Pero el requisito explícito del
proyecto es que un estado no físico se detecte y se informe, no que se enmascare,
y un NaN no es algo que un piso pueda arreglar. Hay un test que lo comprueba
(`un_campo_con_nan_es_un_error_no_algo_que_pisar`) — y fue ese test el que
descubrió el problema.

### 17. Una clave desconocida en el namelist es un error

La lectura de namelist de gfortran acepta claves que no estén en la lista de
variables según cómo se configure, y el original detecta el `iostat` pero no
distingue una errata de un fallo de formato. El parser de C++ rechaza
explícitamente cualquier clave que no reconozca:

```
input.par: clave desconocida en &euler: 'limitador'
```

Es más estricto que el original. El motivo es el mismo requisito de siempre: una
errata de un carácter no debe hacer correr un experimento distinto del que se
pidió.

---

## Resumen

| # | Asunto | Categoría | Efecto numérico |
|---|---|---|---|
| 1 | `sign(x, 0) = +1` | Preservado | — |
| 2 | Transmisiva copia dos veces la celda 1 | Preservado | — |
| 3 | Trabajo de gravedad con `u` del inicio | Preservado | — |
| 4 | `*_exact.xl` sin sentido en tests 7 y 8 | Preservado + documentado | — |
| 5 | Pisos como parámetros de código | Preservado (+ configurable) | — |
| 6 | Equilibrio discreto en la IC del test 7 | Preservado | — |
| 7 | Validar antes de resolver el test | Preservado (+ segunda validación) | — |
| 8 | `rhs_fv` leía `W` global | Reestructurado | ninguno |
| 9 | Aliasing en RK2 (ya corregido en el original) | Reestructurado | ninguno |
| 10 | Actualización cartesiana descartada | Reestructurado | ninguno |
| 11 | `select case` en bucles internos | Reestructurado | ninguno |
| 12 | Sobreescritura silenciosa por `set_test` | Reestructurado (+ aviso) | ninguno |
| 13 | Pisos silenciosos | Reestructurado (+ contadores) | ninguno |
| 14 | `ultimo_guardado` sin inicializar | **Desviación** | solo si `tf ≤ 0` |
| 15 | `'HLL'` en mayúsculas | **Desviación** | ninguno (antes era error) |
| 16 | NaN silencioso en el CFL | **Desviación** | aborta en vez de seguir |
| 17 | Clave desconocida en el namelist | **Desviación** | aborta en vez de continuar |

Las cuatro desviaciones tienen en común que solo cambian el comportamiento en
situaciones en que el original producía un resultado indefinido, silenciosamente
erróneo o inservible. En todas las corridas válidas —las 26 de la validación
directa y las 13 archivadas— el comportamiento es el mismo.
