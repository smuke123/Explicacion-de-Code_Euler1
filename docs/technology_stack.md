# Decisiones de tecnología y herramientas

**Fecha:** 27 de septiembre de 2026

Cada decisión está registrada también en `decision_log.md` con su identificador
`ADR-xxx`. Aquí está el razonamiento; allí el registro formal.

---

## 1. Lenguaje y estándar

**Decisión:** C++17. `PROPOSED` — sujeto al punto abierto `C-1`.

El anteproyecto (§5.1) declara C++17 explícitamente y nombra `std::variant` e
`if constexpr` como las mejoras relevantes. El CMake actual declara C++20.

Se verificó por búsqueda que el código **no usa ninguna característica exclusiva
de C++20**: no hay `concept`, `requires`, `consteval`, `std::span`,
`std::format`, `std::ranges`, operador `<=>` ni corrutinas. Bajar el estándar es
un cambio de una línea.

Argumento a favor de bajar: el documento entregado dice C++17, y un evaluador
puede preguntar por la discrepancia. C++17 tiene además mejor soporte en
compiladores antiguos, lo que ayuda a la portabilidad (NFR-004) y a la adopción
en laboratorios universitarios.

Consecuencia que hay que aceptar: si más adelante se quiere expresar el contrato
`Mesh` como un *concept* de C++20 (sección 9.4 de `architecture.md`), en C++17
hay que documentarlo como convención y verificarlo con `static_assert`, que es
menos elegante pero funciona.

**Decisión:** sin extensiones del compilador. `CMAKE_CXX_EXTENSIONS OFF`, ya
activo. Código estándar portable, sin `__attribute__` ni intrínsecos.

---

## 2. Sistema de construcción

**Decisión:** CMake como sistema principal, Ninja como generador preferido.
`DECIDED` · `ADR-002`

| Opción | Evaluación |
|---|---|
| **CMake** | Elegido. El anteproyecto lo nombra (§5.1). Multiplataforma real, integra CTest, y es el estándar de facto en software científico |
| **Make** | Descartado como principal. Es lo que usa el código Fortran de referencia y funciona, pero no es portable a Windows sin capa de compatibilidad y no gestiona configuraciones múltiples |
| **Ninja** | Elegido como **generador**, no como alternativa a CMake. Compilación incremental sensiblemente más rápida, que importa porque la suite se ejecuta muy a menudo |

Requisitos sobre la configuración de CMake:

- Versión mínima declarada de forma que funcione en distribuciones con soporte a
  largo plazo. Verificar el `cmake_minimum_required` actual contra la versión más
  antigua que se quiera soportar.
- Configuraciones `Debug` y `Release` funcionales, ambas cubiertas por CI.
  `Debug` activa las aserciones de rango (NFR-013).
- Compilación fuera del árbol de fuentes obligatoria.
- Objetivo de tests opcional mediante opción, para que un usuario que solo quiera
  el solver no compile 87 casos de prueba.
- **Sin descarga de dependencias durante la configuración.** Ver NFR-005.

---

## 3. Compiladores

**Decisión:** GCC como compilador primario, Clang como secundario obligatorio,
MSVC no soportado en esta fase. `PROPOSED`

| Compilador | Estado | Razón |
|---|---|---|
| **GCC** | Primario | Es el del entorno de desarrollo (MSYS2 UCRT64 en Windows) y el que compila el Fortran de referencia, lo que permite validación directa en la misma máquina |
| **Clang** | Secundario, en CI | Detecta clases de problema distintas a las de GCC. Compilar limpio con ambos es una verificación real de que el código es estándar |
| **MSVC** | No soportado inicialmente | No aporta a los objetivos y sí coste: distinto conjunto de advertencias y sin `gfortran` para la validación directa. Se puede añadir después sin cambios de diseño |

Declarar MSVC fuera de alcance ahora es preferible a afirmar portabilidad que no
se verifica. NFR-004 se redacta en consecuencia: la afirmación es «compila limpio
en GCC y Clang», no «funciona en cualquier compilador».

**Conjunto de advertencias obligatorio**, ya activo y cumplido:

```
-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
```

`-Wconversion` y `-Wsign-conversion` no son decoración: detectan exactamente los
errores más probables en este dominio, que son conversiones con pérdida en
aritmética de punto flotante y comparaciones de signo mixto al indexar celdas
fantasma con índices que pueden ser negativos.

**Decisión:** no se usa `-ffast-math` ni ninguna opción que relaje la aritmética
de punto flotante. `DECIDED` · `ADR-003`. Rompería el determinismo (NFR-002) y la
regresión byte a byte contra el Fortran, que es el criterio de aceptación central
del proyecto. Registrado como decisión porque es tentador y sería difícil de
diagnosticar después.

---

## 4. Pruebas

**Decisión:** Catch2 v2 incorporado al repositorio como cabecera única, con CTest
como ejecutor. `PROPOSED` — sujeto al punto abierto `C-2` · `ADR-004`

Aquí hay un conflicto real entre dos requisitos y conviene exponerlo antes de la
decisión:

- El anteproyecto (§5.2) nombra **Google Test o Catch2**.
- NFR-005 exige que compilar y probar **no requiera descargar nada**, porque es
  una barrera de adopción y una fuente de fallos de reproducibilidad.

| Opción | Cumple §5.2 | Cumple NFR-005 | Coste |
|---|---|---|---|
| Arnés propio (actual) | No | Sí | Ninguno; ya funciona |
| GoogleTest vía `FetchContent` | Sí | **No**: requiere red | Medio |
| Catch2 v3 vía `FetchContent` | Sí | **No**: requiere red | Medio |
| **Catch2 v2, cabecera única incorporada** | **Sí** | **Sí** | Migrar 87 casos |

La cuarta opción satisface los dos requisitos. Catch2 v2 se distribuye como un
único `catch.hpp` que se puede versionar en `third_party/`, sin red, sin
submódulos y sin gestor de paquetes.

La migración es en su mayor parte mecánica, y el mapeo es directo:

| Arnés actual | Catch2 |
|---|---|
| `TEST(nombre)` | `TEST_CASE("nombre")` |
| `CHECK(cond)` | `CHECK(cond)` — igual |
| `CHECK_NEAR(a, b, tol)` | `REQUIRE_THAT(a, WithinAbs(b, tol))` |
| `CHECK_REL(v, ref, tol)` | `REQUIRE_THAT(v, WithinRel(ref, tol))` |
| `CHECK_THROWS(e, T)` | `REQUIRE_THROWS_AS(e, T)` |

Lo que se gana, además de cumplir el documento: mensajes de fallo que muestran
los valores, secciones para compartir preparación entre casos, etiquetas para
filtrar, y familiaridad para un evaluador.

Lo que se pierde: el repositorio crece en un archivo grande, y la compilación de
los tests se hace más lenta.

**CTest se conserva en cualquier caso.** Es el ejecutor y ya está integrado; la
decisión solo afecta a cómo se escriben las aserciones. Eso acota el riesgo: la
migración no toca la infraestructura de CI.

**Recomendación de ejecución:** migrar en un solo hito, no gradualmente. Dos
arneses conviviendo es peor que cualquiera de los dos.

---

## 5. Integración continua

**Decisión:** GitHub Actions. `PROPOSED` · `ADR-005`

No está en los objetivos explícitamente, pero es **el mecanismo por el que varios
requisitos no funcionales se verifican en lugar de afirmarse**. Sin CI, NFR-004
(portabilidad), NFR-012 (sin advertencias), NFR-002 (determinismo) y NFR-001
(regresión) son promesas.

Matriz mínima propuesta:

| Trabajo | Qué verifica |
|---|---|
| GCC / Release | Compilación limpia y suite verde |
| GCC / Debug | Aserciones de rango activas (NFR-013) |
| Clang / Release | Portabilidad del estándar (NFR-004) |
| Regresión con `gfortran` | Compila el Fortran de referencia y compara (NFR-001) |
| Determinismo | Dos ejecuciones idénticas byte a byte (NFR-002) |

Advertencias como error en CI, no en la compilación local: en local estorban al
depurar; en CI son una puerta.

El trabajo de regresión con `gfortran` es el más valioso y el más frágil, porque
depende de que el código Fortran de referencia siga disponible en el
repositorio. Ver `validation_strategy.md`.

---

## 6. Control de versiones

**Decisión:** Git con repositorio en GitHub. `CONFIRMED` — el anteproyecto (§5.4)
lo declara «componente no negociable».

### Estrategia de ramas

**Decisión:** troncal con ramas de vida corta. `PROPOSED` · `ADR-006`

Con dos autores, `git-flow` es burocracia. La propuesta:

- `main` siempre compila y tiene la suite verde. Es la rama que un evaluador
  clona.
- Una rama por hito o por componente, de vida corta, fusionada cuando cumple su
  *Definition of Done* (ver `development_workflow.md`).
- Etiqueta en cada hito cerrado, para que la memoria de tesis pueda citar una
  versión exacta.
- Etiqueta en la versión que produce cada figura o tabla publicada. Esto es lo
  que hace operativa la afirmación de reproducibilidad de §5.4: un resultado
  citado debe apuntar a un commit.

### Convención de mensajes de commit

**Decisión:** prefijo de tipo más descripción, con cuerpo explicativo cuando el
cambio tiene contenido. `PROPOSED`

```
<tipo>: <descripción en imperativo>

<cuerpo: qué cambió, por qué, y qué evidencia lo respalda>
```

Tipos: `feat`, `fix`, `refactor`, `docs`, `test`, `perf`, `build`, `chore`.

**Nota honesta:** los commits actuales del repositorio no llevan prefijo de tipo,
aunque sí tienen cuerpo descriptivo. La historia existente se deja como está; la
convención aplica a partir de su adopción.

Regla específica de este proyecto: **todo commit que toque código numérico debe
indicar en el cuerpo si la regresión cambió y por qué.** Si el número de pasos
temporales de algún caso cambia, el commit tiene que decirlo explícitamente. Es
la salvaguarda contra alterar el resultado sin darse cuenta.

### Qué se versiona y qué no

| Se versiona | No se versiona |
|---|---|
| Código fuente y cabeceras | Directorios de compilación |
| `CMakeLists.txt` y configuración de CI | Binarios y objetos |
| Documentación | Salidas de simulación |
| Ejemplos de configuración | Figuras generadas |
| **Datos de referencia del Fortran** (`tests/reference/`) | Archivos temporales del sistema |
| Copia del Fortran de referencia (`reference_fortran/`) | Entornos virtuales de Python |
| Cabecera de Catch2 incorporada | |

Los datos de referencia se versionan a propósito, aunque ocupen unos 21 MB: son
lo que hace que la regresión funcione en un clon limpio, sin depender de que el
material del director siga accesible.

**Regla crítica de `.gitattributes`.** Los archivos de referencia se marcan como
binarios. Se comparan byte a byte con la salida del Fortran, de modo que cualquier
conversión de fin de línea invalidaría la validación. Ya está implementado y
verificado clonando el repositorio y comparando resúmenes criptográficos.

---

## 7. Herramientas de Python

**Decisión:** se conservan en Python y se incorporan al proyecto. No se
reescriben en C++. `PROPOSED` · `ADR-007`

Estado actual: existen **seis** herramientas en los paquetes Fortran del
director, **ninguna migrada**.

| Herramienta | Origen | Qué hace | ¿Se incorpora? |
|---|---|---|---|
| `fig_toro.py` | 1D | Figura de los casos de Toro contra la solución exacta | Sí: es el entregable gráfico del `Obj-3` |
| `fig_convergencia.py` | 1D | Tabla y figura de orden de convergencia | Sí: es el `Obj-3` (FR-034) |
| `sedov_exacto.py` | 1D | Solución exacta de Sedov-Taylor | No: Sedov está fuera de alcance |
| `fig_fuentes.py` | 1D | Figuras de gravedad y geometría | No: fuera de alcance |
| `fig_euler2d.py` | 2D | Mapas 2D **y métrica de simetría radial** | Sí, adaptado: la métrica es validación, no dibujo |
| `fig_rt.py` | 2D | Rayleigh-Taylor contra teoría lineal | No: fuera de alcance |

Razones para no reescribir en C++: el postprocesamiento no es parte del núcleo
computacional, matplotlib no tiene equivalente razonable en C++, y §5.3 asigna
explícitamente la visualización a Python y ParaView. Reescribirlas sería trabajo
sin relación con ningún objetivo.

Condiciones de incorporación:

- Viven en `tools/`, fuera de la compilación de CMake.
- `tools/requirements.txt` con versiones mínimas, para que la instalación sea
  reproducible.
- **Ninguna herramienta de Python es requisito para compilar ni para ejecutar la
  suite.** Si falta matplotlib, el proyecto sigue funcionando; solo no se generan
  figuras. Esto preserva NFR-005.
- Las herramientas que **calculan métricas** —no solo dibujan— necesitan sus
  propias pruebas, porque producen números que van en la memoria de tesis. La
  métrica de simetría radial de `fig_euler2d.py` es el caso claro.

`PENDING INPUT FROM DIRECTOR` — si prefiere conservar el estilo de figuras de sus
herramientas (mapas en *inferno*, perfiles en blanco y negro) como estándar del
proyecto. Afecta solo a la presentación, pero conviene decidirlo una vez.

---

## 8. Documentación

**Decisión:** Markdown en `docs/` como formato principal; comentarios de
documentación en las cabeceras públicas; Doxygen opcional y nunca obligatorio
para compilar. `PROPOSED`

| Necesidad | Solución | Razón |
|---|---|---|
| Arquitectura, decisiones, estrategia | Markdown en `docs/` | Se lee en GitHub sin generar nada, se versiona y se revisa en el diff |
| Referencia de interfaces (`Obj-4`) | Comentarios de documentación en las cabeceras | La documentación vive junto al contrato que describe, que es lo único que evita que se desincronice |
| Manual de usuario y tutoriales | Markdown en `docs/` con ejemplos ejecutables | Debe poder seguirse copiando y pegando |
| Fórmulas matemáticas | LaTeX embebido en Markdown | GitHub lo renderiza |
| Sitio HTML generado | Doxygen, **opcional** | No se exige para compilar ni para los entregables |

El razonamiento sobre Doxygen: el `Obj-4` pide documentar «las interfaces de
programación», y hacerlo en las cabeceras cumple el objetivo. Generar HTML es un
extra que casi nadie lee y que añadiría una dependencia de construcción. Los
comentarios estructurados no cuestan nada y sirven igual leídos en el código.

Regla de mantenimiento: **un cambio de comportamiento que no actualiza la
documentación afectada está incompleto.** Forma parte de la *Definition of Done*.

---

## 9. Resumen de la pila

| Capa | Elección | Estado |
|---|---|---|
| Lenguaje | C++17, sin extensiones | `PROPOSED` (`C-1`) |
| Construcción | CMake + Ninja | `DECIDED` |
| Compilador primario | GCC (MSYS2 UCRT64 en el equipo de desarrollo) | `PROPOSED` |
| Compilador secundario | Clang, verificado en CI | `PROPOSED` |
| Compilador no soportado | MSVC | `PROPOSED` |
| Aritmética | IEEE 754 estricta, sin `-ffast-math` | `DECIDED` |
| Pruebas | Catch2 v2 incorporado + CTest | `PROPOSED` (`C-2`) |
| CI | GitHub Actions, matriz de 5 trabajos | `PROPOSED` |
| Control de versiones | Git, troncal con ramas cortas | `PROPOSED` |
| Configuración | JSON principal, namelist de compatibilidad | `PROPOSED` (`C-3`) |
| Salida | CSV en 1D, VTK en 2D, `.xl` de compatibilidad | `PROPOSED` (`C-4`) |
| Postproceso | Python con matplotlib, en `tools/`, opcional | `PROPOSED` |
| Documentación | Markdown + comentarios en cabeceras | `PROPOSED` |
| Dependencias externas obligatorias | **Ninguna** | `DECIDED` |

La última fila es la que más conviene defender. Un proyecto científico que se
compila con solo un compilador y CMake elimina de golpe una clase entera de
problemas de reproducibilidad, y eso responde directamente a uno de los cuatro
problemas que la justificación de la tesis identifica.
