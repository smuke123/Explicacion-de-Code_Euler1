# Estándares de código y organización

**Fecha:** 27 de septiembre de 2026

Este documento describe en gran parte convenciones que el código ya sigue de
forma consistente. Donde se propone un cambio, está marcado.

**Principio rector:** la legibilidad del código numérico tiene prioridad sobre
cualquier otra consideración de estilo. Si una regla de estilo hace que una
fórmula se parezca menos a como está escrita en el texto de referencia, la regla
cede.

---

# Parte I — Organización del proyecto

## 1. Estructura de directorios

`PROPOSED` — evolución de la estructura actual, no reorganización.

```
fvlab/
├── CMakeLists.txt
├── README.md
├── .gitattributes                 los datos de referencia son binarios
├── .gitignore
├── input.json                     configuración por defecto
│
├── include/fvlab/                 cabeceras públicas, una por componente
│   ├── config/
│   ├── mesh/
│   ├── physics/
│   ├── numerical/
│   │   ├── reconstruction/
│   │   ├── riemann/
│   │   ├── source/
│   │   └── time/
│   ├── boundary/
│   ├── initial/
│   ├── io/
│   ├── simulation/
│   └── validation/
│
├── src/                           implementaciones, misma jerarquía
│   ├── main.cpp
│   └── tools/                     ejecutables auxiliares
│
├── tests/
│   ├── CMakeLists.txt
│   ├── unit/                      por componente
│   ├── integration/               simulaciones completas
│   ├── regression/                contra salidas archivadas del Fortran
│   └── reference/                 DATOS de referencia (solo lectura, binarios)
│
├── examples/                      configuraciones listas para usar, una por caso
├── tools/                         Python: figuras y análisis (fuera de CMake)
│   └── requirements.txt
├── scripts/                       validación directa contra el Fortran
├── reference_fortran/             copia del código del director (solo lectura)
├── third_party/                   Catch2, cabecera única incorporada
└── docs/
```

### Qué va en cada directorio, y qué no

| Directorio | Contiene | **No** contiene |
|---|---|---|
| `include/fvlab/` | Todo lo que un consumidor de la biblioteca necesita | Detalles de implementación privados |
| `src/` | Implementaciones y ejecutables | Cabeceras públicas |
| `tests/unit/` | Pruebas que no construyen `Simulation` | Pruebas que leen archivos de datos |
| `tests/integration/` | Simulaciones completas en directorios temporales | Comparaciones con el Fortran |
| `tests/regression/` | Comparación con salidas archivadas | Datos: esos van en `reference/` |
| `tests/reference/` | **Solo datos producidos por el Fortran** | Nada generado por el C++ |
| `examples/` | Un archivo de configuración por caso de prueba | Salidas |
| `tools/` | Análisis y figuras en Python | Nada que CMake compile |
| `reference_fortran/` | Copia literal, sin modificar | Nada nuestro |
| `third_party/` | Código externo sin modificar | Código propio |

La separación de `tests/reference/` es la más importante y tiene su propia
política en `validation_strategy.md`: es lo que evita que el C++ acabe
validándose contra su propia salida.

### Cambio respecto a la estructura actual

Hoy `tests/` es plano: los seis ejecutables conviven en un directorio. Se
propone separar en `unit/`, `integration/` y `regression/` porque los tres tipos
tienen **coste y significado distintos**: los unitarios se ejecutan
constantemente y duran milisegundos; los de regresión leen 21 MB y duran
segundos. Separarlos permite ejecutar los rápidos en cada guardado y los lentos
antes de un commit.

## 2. Organización de cabeceras y fuentes

**Regla:** una cabecera por componente; un `.cpp` por cabecera **salvo** cuando
la cabecera solo contiene tipos pequeños o funciones `inline`.

Las cabeceras sin `.cpp` son deliberadas: partirlas impediría la expansión
`inline` justo en los puntos más calientes. Aplica a los estados, los campos, la
ecuación de estado, las mallas, los solvers de Riemann y los limitadores.

**Guarda de inclusión:** `#pragma once`. Soportado por los tres compiladores
relevantes y elimina la clase de error de las macros duplicadas.

**Orden de inclusión**, separado por líneas en blanco:

```cpp
// 1. La cabecera propia de este .cpp, primero y sola.
//    Así se comprueba que la cabecera es autosuficiente.
#include "fvlab/numerical/riemann/hllc.hpp"

// 2. Biblioteca estándar, en orden alfabético.
#include <algorithm>
#include <cmath>
#include <vector>

// 3. Cabeceras del proyecto, en orden alfabético.
#include "fvlab/physics/ideal_gas.hpp"
#include "fvlab/physics/state.hpp"
```

La cabecera propia va primero por una razón concreta: si compila ahí, es
autosuficiente y no depende de que el consumidor incluya otras cosas antes.

**Rutas de inclusión:** siempre desde la raíz de `include/`, con comillas y con
el prefijo del proyecto. Nunca rutas relativas con `..`.

## 3. Namespaces

**Regla:** un namespace de proyecto, `fvlab`. Subnamespaces solo donde hay
riesgo real de colisión de nombres cortos (por ejemplo `fvlab::limiters`).

`PROPOSED` — el namespace actual es `euler1d` y queda incorrecto al entrar el
2D. El cambio es un renombrado mecánico. Ver `ADR-001`.

Detalles de implementación que no van en la cabecera se ponen en un namespace
anónimo dentro del `.cpp`, no en un `detail::` público.

**Prohibido:** `using namespace` en cabeceras. En `.cpp` se admite para un
subnamespace del proyecto, nunca para `std`.

---

# Parte II — Convenciones de nomenclatura

Las siguientes reglas describen lo que el código ya hace.

| Elemento | Convención | Ejemplo |
|---|---|---|
| Clases, estructuras, enumerados | `PascalCase` | `HllcRiemannSolver`, `SimulationConfig` |
| Funciones y métodos | `camelCase` | `computeInterfaceStates`, `toConserved` |
| Variables locales y parámetros | `snake_case` | `max_speed`, `wave_speed_left` |
| Miembros de datos privados | `snake_case_` con guion bajo final | `limiter_`, `config_` |
| Miembros públicos de tipos valor | `snake_case`, sin sufijo | `state.rho`, `state.u` |
| Constantes de compilación | `k` + `PascalCase` | `kGhostCells`, `kTestCaseCount` |
| Valores de enumerado | `PascalCase` | `LimiterKind::VanLeer` |
| Archivos | `snake_case` | `exact_riemann_solver.cpp` |
| Directorios | `snake_case`, singular | `riemann/`, `reconstruction/` |
| Macros | Se evitan. Si son inevitables, `FVLAB_` + `MAYÚSCULAS` | `FVLAB_ASSERT` |

## Nomenclatura en código numérico: la excepción

En una fórmula, los nombres largos y descriptivos **empeoran** la legibilidad.
Comparar:

```cpp
// Fiel a la fórmula del texto: se puede verificar leyendo.
const double s_star = (pR - pL + rhoL*uL*(SL - uL) - rhoR*uR*(SR - uR))
                    / (rhoL*(SL - uL) - rhoR*(SR - uR));

// "Descriptivo" e imposible de comparar con el libro.
const double contact_wave_speed =
    (right_pressure - left_pressure
     + left_density*left_velocity*(left_wave_speed - left_velocity) - ...
```

**Regla:** dentro del cuerpo de una fórmula se usa la notación del texto de
referencia (`SL`, `SR`, `s_star`, `qL`, `aR`), y se indica en un comentario de
qué fuente y qué ecuación se trata. Fuera de las fórmulas, nombres descriptivos.

Es una excepción consciente al estilo general, y es la que sirve al objetivo
pedagógico del proyecto.

## Idioma

| Elemento | Idioma |
|---|---|
| Identificadores | Inglés |
| Comentarios | Español |
| Documentación | Español |
| Nombres de casos de prueba | Español descriptivo: `el_flujo_hllc_es_consistente` |
| Mensajes de error al usuario | Español |

**Regla de codificación:** los comentarios y las cadenas literales en código
fuente usan **solo ASCII**, sin acentos ni caracteres especiales. La
documentación en Markdown usa UTF-8 completo.

No es pedantería: elimina una clase de problema real al compilar en Windows con
distintas páginas de código, y es lo que el código ya hace de forma consistente.

---

# Parte III — Reglas de escritura

## 4. Inicialización

**Regla:** toda variable se inicializa en su declaración. Sin excepciones.

```cpp
double max_speed = 0.0;              // sí
double max_speed;                    // no
```

Los miembros se inicializan en la declaración de la clase, no en el constructor,
salvo que dependan de un parámetro.

**Llaves frente a paréntesis:** llaves por defecto, porque prohíben conversiones
con estrechamiento. Paréntesis cuando la conversión es intencional y está
comentada, y con `std::vector` cuando se quiere el constructor de tamaño.

```cpp
std::vector<double> flux(n_faces, 0.0);   // n elementos: paréntesis
FaceState<1> q{rho, un, {ut}, p};         // agregado: llaves
```

## 5. Corrección de `const`

- Todo parámetro que no se modifica es referencia `const` o valor.
- Todo método que no modifica el objeto es `const`.
- Los componentes numéricos son `const` tras construirse.
- Las variables locales que no cambian son `const`. En código numérico esto es
  especialmente valioso: deja claro de un vistazo qué es dato de entrada y qué es
  acumulador.

**Los tipos pequeños se pasan por valor**, no por referencia `const`: un
`PrimitiveState` son tres `double` y caben en registros.

## 6. Propiedad de recursos

| Situación | Mecanismo |
|---|---|
| Componentes polimórficos | `std::unique_ptr`, poseídos por `Simulation` |
| Arreglos de trabajo | `std::vector`, miembro del componente que los usa |
| Tamaño fijo conocido | `std::array` |
| Referencia sin propiedad | Referencia `const`, nunca puntero |

**Prohibido:** `new`, `delete`, punteros propietarios crudos,
`std::shared_ptr` (no hay propiedad compartida en este diseño, y usarlo
ocultaría un error de modelado).

**Regla:** nadie almacena una referencia ni un puntero a algo que no posee y que
pueda desaparecer antes. Las referencias a la malla y a la ecuación de estado son
válidas porque `Simulation` garantiza que viven más que los componentes.

## 7. Contenedores y asignación de memoria

**Regla crítica:** el bucle temporal no asigna memoria. Todos los arreglos de
trabajo —etapas de Runge-Kutta, estados de cara, flujos— son miembros de sus
componentes, asignados una vez en la construcción.

No es una optimización prematura: es previsibilidad. Asignar dentro del bucle
introduce variabilidad en el tiempo de ejecución y dificulta razonar sobre el
coste. Verificable por inspección (NFR-014).

## 8. Precisión numérica

**Regla:** `double` en todo el código numérico. Nunca `float`. El Fortran de
referencia usa `real(kind=8)` y la regresión byte a byte lo exige.

**Literales:** siempre con parte decimal explícita y sin sufijo.

```cpp
const double half = 0.5;        // sí
const double half = 1 / 2;      // no: división entera, da 0
const double g = 1.4f;          // no: float
```

**Aritmética entera y flotante mezclada:** conversión explícita siempre. Con
`-Wconversion` activo, lo implícito es un error de compilación.

**Potencias enteras:** no se usa `std::pow` con exponente entero. Se usa
multiplicación directa o la función auxiliar de potencia entera, que reproduce el
comportamiento del compilador Fortran. `std::pow(r, 2)` y `r*r` **no** garantizan
el mismo resultado bit a bit, y eso rompe la regresión.

**Reasociación:** no se reordenan operaciones de punto flotante «porque es
equivalente». No lo es. `(a + b) + c` y `a + (b + c)` dan resultados distintos.
El orden de las operaciones en las fórmulas se preserva respecto al Fortran. Ver
`numerical_invariants.md` `INV-002`.

**Comparación de flotantes:** nunca `==` sobre valores calculados, con una
excepción deliberada: cuando se verifica que una operación preserva un valor
**exactamente** —copiar celdas fantasma, aplicar un piso— la igualdad exacta es
el criterio correcto y se expresa con tolerancia cero.

## 9. Manejo de errores

Tres mecanismos, con fronteras claras.

| Mecanismo | Para qué | Ejemplo |
|---|---|---|
| **Excepción** | Condición que el usuario puede corregir, o resultado del experimento que hay que reportar | Clave desconocida en la configuración; `NaN` en el campo |
| **Aserción** | Invariante que, si falla, es un error del programador | Índice de celda fuera del rango de fantasmas |
| **Código de retorno** | Frontera del proceso | `main` traduce excepciones a códigos de salida |

### Política de excepciones

- Se lanzan excepciones para errores de configuración (`ConfigError`) y para
  estados numéricos no físicos (`std::runtime_error`).
- Se capturan **solo** en `main`, que las traduce a código de salida y mensaje.
- **Prohibido el `catch` vacío o que solo registre y continúe.** NFR-011.
- Los componentes numéricos no lanzan dentro de los bucles celda a celda. La
  detección se hace en el barrido de comprobación (el cálculo de `dt`), que ya
  recorre todo el campo. Así el bucle caliente no paga nada.

### Política de aserciones

- Activas en `Debug`, ausentes en `Release`. Nunca se usan para validar entrada
  del usuario: eso es una excepción.
- El acceso a los campos verifica el rango de índices, porque el error más
  probable al generalizar a 2D es indexar mal las celdas fantasma (NFR-013).

### Mensajes de error

Un mensaje útil nombra tres cosas: qué se esperaba, qué se recibió y dónde.

```
ERROR de configuracion: 'cfl' debe estar en (0, 1]; recibido: 1.5
ERROR numerico: campo no finito en el paso 1247, celda 203 (densidad = nan)
```

El segundo es el que importa. `NaN detectado` obliga a reproducir el fallo para
localizarlo; con el paso y la celda se va directo.

## 10. Comentarios

**Regla:** un comentario explica **por qué**, no qué. El código ya dice qué hace.

Los comentarios que valen la pena en este proyecto son de tres tipos:

**Procedencia de una fórmula.** Indispensable para la verificación.

```cpp
// Velocidad del contacto, ecuacion (10.37) de Toro (2009).
```

**Justificación de una decisión no obvia**, especialmente las que replican una
peculiaridad del Fortran. Sin el comentario, el siguiente lector la "arregla".

```cpp
// sign(1.0, 0.0) devuelve +1 en Fortran, no 0. Se replica a proposito:
// cambiarlo altera el limitador en las celdas de pendiente nula.
```

**Contrato que la firma no puede expresar.**

```cpp
// El llamador debe haber sincronizado `w` con `u_in` antes de llamar.
// Omitirlo produce un resultado incorrecto sin sintoma.
```

**Lo que no se comenta:** lo que el código ya dice, el historial de cambios (eso
es de git), ni notas dirigidas al revisor de un *pull request*.

### Comentarios de documentación

Las interfaces públicas llevan comentario `///` que describe el contrato:
propósito, qué garantiza, qué exige del llamador, y unidades o convenciones no
evidentes. Es el entregable de «interfaces de programación» del `Obj-4`.

## 11. Tamaño y forma de las funciones

| Métrica | Límite | Nota |
|---|---|---|
| Longitud de función | ~50 líneas | Un solver de Riemann puede pasarse: es una fórmula, partirla la haría ilegible |
| Longitud de archivo | ~800 líneas | Ninguno se acerca hoy |
| Profundidad de anidamiento | 4 niveles | Un barrido 2D ya consume dos |
| Parámetros | 5 | Más sugiere que falta un tipo |

Los límites son guías, no reglas automáticas. El criterio real: **si una función
no se puede verificar leyéndola contra la fórmula que implementa, es demasiado
larga.**

## 12. Formato

`PROPOSED` — hoy no hay formateador configurado.

**Propuesta:** `clang-format` con un archivo de configuración versionado, basado
en el estilo de Google con dos cambios: límite de 100 columnas en lugar de 80, y
prohibición de reordenar los bloques de inclusión, para preservar el orden de la
sección 2.

Razón del ancho: las fórmulas numéricas con nombres de la notación del texto
caben en 100 columnas y no en 80. Forzarlas a 80 produce cortes que las hacen
irreconocibles.

**No** se aplica formateo automático en el guardado ni en un gancho de commit
sobre archivos ya escritos: generaría un diff enorme que enterraría los cambios
reales. Se aplica a archivos nuevos y, sobre los existentes, en un único commit
de formato separado y anotado.
