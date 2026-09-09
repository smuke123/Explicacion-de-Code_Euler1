# Método numérico

Referencia principal: E. F. Toro, *Riemann Solvers and Numerical Methods for
Fluid Dynamics*, 3.ª ed., Springer 2009. Las ecuaciones citadas siguen su
numeración.

## 1. Ecuaciones y ecuación de estado

Se resuelven las ecuaciones de Euler 1D en forma conservativa:

$$\partial_t U + \partial_x F(U) = S,\qquad
U = \begin{pmatrix}\rho \\ \rho u \\ E\end{pmatrix},\quad
F(U) = \begin{pmatrix}\rho u \\ \rho u^2 + p \\ u(E+p)\end{pmatrix}.$$

El cierre es el gas ideal politrópico:

$$E = \frac{p}{\gamma-1} + \tfrac12 \rho u^2,\qquad
c = \sqrt{\frac{\gamma p}{\rho}} .$$

Las variables primitivas son $W = (\rho, u, p)$. Las conversiones están
centralizadas en `IdealGas`:

$$W \to U:\quad (\rho,\ \rho u,\ p/(\gamma-1) + \rho u^2/2)$$

$$U \to W:\quad \rho = \max(U_1, \rho_{\min}),\quad
u = U_2/\rho,\quad
p = \max\big((\gamma-1)(U_3 - \tfrac12\rho u^2),\ p_{\min}\big)$$

El **orden importa**: el piso de densidad se aplica *antes* de dividir, de modo
que la velocidad se calcula con la densidad ya corregida. Es lo que hace el
Fortran en `cons2prim_all` y se conservó literalmente.

## 2. Discretización en volúmenes finitos

El dominio $[x_{\min}, x_{\max}]$ se divide en $N$ celdas de ancho
$\Delta x = (x_{\max}-x_{\min})/N$. Los centros son

$$x_k = x_{\min} + (k - \tfrac12)\Delta x,\qquad k = 1,\dots,N,$$

y las caras $x_{k+1/2} = x_{\min} + k\,\Delta x$ con $k = 0,\dots,N$.

$U_k$ es el promedio de $U$ sobre la celda $k$. Integrando la ley de
conservación sobre la celda se obtiene la forma semidiscreta exacta

$$\frac{dU_k}{dt} = -\frac{F_{k+1/2} - F_{k-1/2}}{\Delta x} + S_k,$$

donde $F_{k\pm1/2}$ son flujos en las caras. Todo el contenido del método está
en cómo se aproximan esos flujos.

### Convención de índices

Hay **dos celdas fantasma por lado**, porque reconstruir la cara $k+1/2$ con
MUSCL necesita la plantilla $\{k-1, k, k+1, k+2\}$.

| Índice | Significado |
|---|---|
| `k = 1 .. N` | celdas físicas |
| `k = 0, -1` | fantasmas izquierdos |
| `k = N+1, N+2` | fantasmas derechos |
| `fluxes[k]`, `k = 0..N` | flujo numérico en la cara $k+1/2$ |

La celda $k$ usa `fluxes[k]` como flujo derecho y `fluxes[k-1]` como izquierdo.

Se conservó la indexación base-1 del Fortran (`U(3,-1:N+2)`). En C++,
`CellField::operator[](k)` traduce a un slot interno `k + 1`, así que el
almacenamiento es un `std::vector` contiguo de $N + 4$ elementos y el índice
lógico es idéntico al del original.

## 3. Esquema de Godunov y reconstrucción MUSCL

El método de Godunov calcula el flujo de cada cara resolviendo un problema de
Riemann local entre los estados a ambos lados. En primer orden esos estados son
los promedios de celda, lo que da un esquema monótono pero muy difusivo.

**MUSCL** (*Monotone Upstream-centred Scheme for Conservation Laws*) eleva el
orden extrapolando linealmente dentro de cada celda con una pendiente limitada
$\sigma$:

$$q^L_{k+1/2} = W_k + \tfrac12\sigma_k,\qquad
q^R_{k+1/2} = W_{k+1} - \tfrac12\sigma_{k+1}.$$

La pendiente se calcula a partir de las diferencias hacia atrás y hacia delante,

$$a = W_k - W_{k-1},\qquad b = W_{k+1} - W_k,$$

y se **limita** para no crear extremos nuevos. La reconstrucción se hace sobre
variables **primitivas**, no conservadas: es la elección del código original.

Tras reconstruir se aplican pisos de positividad a $\rho$ y $p$ de los estados de
cara. Es la protección prevista por el algoritmo: una pendiente limitada puede
sobrepasar el cero junto a un choque fuerte.

### Limitadores

Todos cumplen $\sigma = 0$ cuando $ab \le 0$ (extremo local), condición
necesaria para que el esquema sea TVD.

| Clave | Nombre | Fórmula |
|---|---|---|
| `fo` | primer orden | $\sigma = 0$ |
| `mm` | minmod | $\tfrac12(\mathrm{sgn}\,a + \mathrm{sgn}\,b)\min(\lvert a\rvert,\lvert b\rvert)$ |
| `vl` | van Leer | $\dfrac{a\lvert b\rvert + b\lvert a\rvert}{\lvert a\rvert+\lvert b\rvert}$ (media armónica) |
| `mc` | monotonized central | $\mathrm{sgn}(c)\min(\lvert c\rvert, 2\lvert a\rvert, 2\lvert b\rvert)$, con $c = \tfrac12(a+b)$ |
| `sb` | superbee | máximo en módulo de los dos minmod cruzados $(2a,b)$ y $(a,2b)$ |

Ordenados de más disipativo a más compresivo: `mm` < `vl` < `mc` < `sb`. Superbee
resuelve las discontinuidades en menos celdas pero tiende a escalonar los
perfiles suaves.

**Detalle que hay que preservar**: `sign` de Fortran devuelve $+1$ para $x = 0$
(`sign(1.0d0, 0.0d0) == +1.0`), a diferencia de una implementación ingenua que
devolvería 0. Aparece en minmod y superbee, donde
$\tfrac12(\mathrm{sgn}\,a + \mathrm{sgn}\,b)$ decide si la pendiente se anula.
Está replicado en `limiters::fortranSign`.

### Reconstrucción hidrostática (con gravedad)

Con $g \neq 0$, reconstruir la presión sin más deja un desequilibrio $O(\Delta x)$
que hace que una atmósfera en reposo se ponga a circular. Se usa la
reconstrucción de Käppeli–Mishra: se reconstruye la **desviación** respecto a la
extrapolación hidrostática local,

$$\delta_{k,k\pm1} = (p_{k\pm1} - p_k) \pm g\,\Delta x\,\frac{\rho_k + \rho_{k\pm1}}{2},$$

y la cara lleva la presión hidrostática más la desviación limitada:

$$p^L_{k+1/2} = p_k - \tfrac12 \rho_k g \Delta x + \tfrac12\sigma(\delta).$$

En el equilibrio discreto trapezoidal las $\delta$ son **exactamente cero**,
ambas caras coinciden y el flujo cancela la fuente $-\rho g$ a nivel de
redondeo. El test 7 lo comprueba: con $N = 200$ y $t_f = 10$, `max|u|` queda en
$1.0\times10^{-15}$ tras 2630 pasos.

## 4. Solvers de Riemann aproximados

Ambos usan las estimaciones de velocidad de señal de **Davis** (Toro
§10.5.1), que son cotas directas sin promedio de Roe:

$$S_L = \min(u_L - c_L,\ u_R - c_R),\qquad
S_R = \max(u_L + c_L,\ u_R + c_R).$$

Si $S_L \ge 0$ toda la información viene de la izquierda y $F = F(W_L)$; si
$S_R \le 0$, $F = F(W_R)$. Estos casos supersónicos se comprueban antes de
cualquier otra cosa.

### HLL (Toro §10.3)

Aproxima la solución con **dos** ondas y un único estado intermedio constante:

$$F^{\mathrm{HLL}} = \frac{S_R F_L - S_L F_R + S_L S_R (U_R - U_L)}{S_R - S_L}.$$

Es robusto y muy simple, pero al no incluir la onda de contacto la difumina.

### HLLC (Toro §10.4)

Añade la **onda de contacto**, con velocidad

$$S_* = \frac{p_R - p_L + \rho_L u_L (S_L - u_L) - \rho_R u_R (S_R - u_R)}
{\rho_L (S_L - u_L) - \rho_R (S_R - u_R)},$$

y estados estrella a cada lado ($K = L, R$):

$$U^*_K = \rho_K\frac{S_K - u_K}{S_K - S_*}
\begin{pmatrix}
1 \\[2pt] S_* \\[2pt]
\dfrac{E_K}{\rho_K} + (S_* - u_K)\left(S_* + \dfrac{p_K}{\rho_K(S_K - u_K)}\right)
\end{pmatrix},$$

y el flujo $F = F_K + S_K (U^*_K - U_K)$ según el signo de $S_*$.

HLLC resuelve exactamente un contacto estacionario aislado (flujo de masa cero),
lo que HLL no consigue: es la diferencia que se mide en el test 5, con su
contacto casi inmóvil. Hay un test unitario específico para esto.

## 5. Solución exacta (referencia, no avance temporal)

`ExactRiemannSolver` implementa el solver exacto del capítulo 4 de Toro. **No
participa en el avance temporal**: existe para validar, generar referencias y
calcular errores.

Dado el par de estados, la presión de la región estrella resuelve

$$f(p) = f_L(p; W_L) + f_R(p; W_R) + (u_R - u_L) = 0,$$

donde cada $f_K$ es la relación de choque (Rankine–Hugoniot) si $p > p_K$ o el
invariante de Riemann de rarefacción si $p \le p_K$ (ecs. 4.6–4.8 y 4.37).

- **Newton–Raphson** con derivada analítica, tolerancia relativa $10^{-14}$ y
  máximo 200 iteraciones. Con la semilla correcta converge en menos de 60.
- **Semilla TRRS** (ec. 4.46): la solución de dos rarefacciones. Es positiva por
  construcción y aguanta el test 2, donde $p^*$ baja a $1.9\times10^{-3}$
  partiendo de $p_L = p_R = 0.4$ y una semilla aritmética falla.
- **Salvaguarda $p \le 0$**: si una iteración se sale a presión no física se
  bisecta hacia $0^+$ en lugar de abortar.
- **Criterio de vacío** (ec. 4.40): si
  $2(c_L + c_R)/(\gamma-1) \le u_R - u_L$, las dos rarefacciones no llegan a
  comunicarse y no existe región estrella. Se señala con $p^* = 0$.

Después se **muestrea la solución autosimilar** $W(s)$ con $s = x/t$ (§4.5),
decidiendo de qué lado del contacto cae $s$ y, dentro de cada lado, si la onda es
choque o rarefacción, incluido el interior del abanico. La rama de vacío (§4.6)
está implementada: sin ella, el muestreo entra en la fórmula del abanico con base
negativa y devuelve **NaN silencioso** en $\rho$ y $p$.

Valores estrella de referencia (Tabla 4.3 de Toro), verificados por
`test_exact_riemann`:

| Test | $p^*$ | $u^*$ |
|---|---|---|
| 1 | 0.46629 | 1.36091 |
| 2 | 0.0018939 | 0 |
| 3 | 460.894 | 19.5975 |
| 4 | 1691.65 | 8.68977 |
| 5 | 460.894 | ≈ 0 |

## 6. Integración temporal: SSP-RK2

Se usa **SSP-RK2** (Heun), de segundo orden y *strong stability preserving*:

$$U^{(1)} = U^n + \Delta t\, L(U^n)$$
$$U^{(2)} = U^{(1)} + \Delta t\, L(U^{(1)})$$
$$U^{n+1} = \tfrac12\left(U^n + U^{(2)}\right)$$

donde $L$ es el operador de volúmenes finitos. Entre etapas hay que
**resincronizar**: recalcular primitivas desde las conservadas y rellenar
fantasmas, porque $L$ reconstruye sobre primitivas.

Los dos arrays destino son **separados**. El Fortran llamaba `rhs_fv(U1, U1)`,
el mismo array como `intent(in)` e `intent(out)`: aliasing ilegal y
comportamiento indefinido según el estándar, que funcionaba por casualidad del
compilador (ver `docs/inconsistencies.md`).

La interfaz `TimeIntegrator` es independiente del operador espacial, así que
añadir RK3 o RK4 no exige tocar el solver. `ForwardEuler` está implementado y se
usa en un test para comprobar que la interfaz es realmente intercambiable.

## 7. Paso de tiempo por CFL

$$\Delta t = \mathrm{CFL}\cdot\frac{\Delta x}{\max_k\left(|u_k| + c_k\right)}$$

El máximo se inicializa en $10^{-30}$ (como el Fortran) para que un campo
exactamente en reposo y frío no divida por cero. El último paso se recorta con
`if (t + dt > tf) dt = tf - t`, de modo que la simulación aterriza exactamente
en $t_f$.

**Detección de estados no finitos.** `std::max(x, \text{NaN})` devuelve `x`: un
NaN en el campo **no** se propaga al máximo, y la simulación seguiría corriendo
sobre basura. Para detectarlo, el mismo barrido acumula la suma de las
velocidades de señal, que sí propaga NaN e Inf, y se comprueba una vez por paso.
Cuesta una adición por celda. Un estado no finito lanza una excepción: es un
fallo del cálculo, no algo que se pueda tapar con un piso.

## 8. Condiciones de frontera

Las celdas fantasma se rellenan **tanto en conservadas como en primitivas**,
porque la reconstrucción lee las primitivas.

### Transmisiva (`trans`)

Gradiente cero: las ondas salen del dominio sin reflejarse. Los **dos** fantasmas
copian la misma celda de borde ($U_{-1} = U_1$, no $U_2$), lo que da
extrapolación constante y anula la pendiente reconstruida en la cara extrema.

### Periódica (`per`)

$U_0 = U_N$, $U_{-1} = U_{N-1}$, $U_{N+1} = U_1$, $U_{N+2} = U_2$. Con esto los
flujos de las caras $0$ y $N$ coinciden y la suma telescópica se anula: la masa
se conserva a precisión de máquina, lo que da la prueba más directa de que la
actualización es realmente conservativa.

### Reflectiva (`refl`)

Pared sólida: espejo en $\rho$, antisimetría en $u$ ($u \to -u$).

Con gravedad, la presión de los fantasmas **no se refleja**: continúa el
equilibrio hidrostático discreto,

$$p_{\text{fantasma}} = p_{\text{interior}} \pm g\,\Delta x\,
\frac{\rho_{\text{fantasma}} + \rho_{\text{interior}}}{2}.$$

Con el espejo plano ($\delta p = 0$) las desviaciones hidrostáticas no se
cancelan en las caras de pared y un fluido en reposo genera espuriamente
$|u| \sim 10^{-3}$.

## 9. Términos fuente y geometría

### Gravedad

$$S = (0,\ -\rho g,\ -\rho g u)$$

El trabajo de la gravedad usa la velocidad del **inicio** del subpaso, no la
recién actualizada. Es lo que hace el Fortran y se conserva.

### Geometría cilíndrica y esférica

Con $\alpha = $ `alpha_geo` ($0$ plano, $1$ cilíndrico, $2$ esférico), la
divergencia se escribe en **forma conservativa** con áreas $A = r^\alpha$ en las
caras y volumen exacto de la celda:

$$V_k = \frac{r_+^{\alpha+1} - r_-^{\alpha+1}}{\alpha+1},\qquad
\frac{dU_k}{dt} = -\frac{A_+ F_{k+1/2} - A_- F_{k-1/2}}{V_k}
+ \underbrace{\frac{p_k (A_+ - A_-)}{V_k}\,\mathbf{e}_{\text{mom}}}_{\text{término geométrico}} .$$

El término geométrico de presión cancela **exactamente** la divergencia de una
presión uniforme, de modo que un ambiente en reposo no se acelera solo por la
curvatura de la malla (propiedad de *buen balance*).

La forma alternativa $-(\alpha/r)F$ **no conserva**: según la auditoría del
código Fortran, el arranque del Sedov inyectaba un 42 % de energía espuria y
sesgaba $\xi_0$ en $+6.8\,\%$. Hay un test de integración que comprueba la
conservación de energía en el Sedov esférico.

Para $r^\alpha$ se usa exponenciación binaria por multiplicaciones en lugar de
`std::pow(double, int)`: es lo que compila gfortran con `__builtin_powi`, y
`std::pow` podría diferir en el último bit.

## 10. Pisos y robustez

Se distinguen cuatro situaciones, que el código trata de forma distinta:

| Situación | Tratamiento |
|---|---|
| Protección prevista por el algoritmo (pendiente limitada que cruza el cero) | Piso silencioso en los estados de cara |
| Estado conservado no físico ($\rho$ o $p$ por debajo del piso) | Piso **contabilizado** en `FloorCounters`, aviso al final |
| NaN o Inf en el campo | **Excepción**: la simulación perdió validez |
| Configuración inválida | **Excepción** antes de empezar a calcular |

Los pisos son $\rho_{\min} = p_{\min} = 10^{-12}$, como en el Fortran.

La diferencia importante frente al original: aquí un piso que se dispara no se
interpreta como «protección exitosa». `FloorCounters` cuenta las activaciones y
guarda la energía interna específica más negativa vista; si hubo alguna, la
corrida lo avisa por pantalla y lo registra en `run_info.txt`. Un piso que salta
miles de veces indica que el resultado dejó de ser físico.
