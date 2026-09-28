# Trazabilidad

**Fecha:** 27 de septiembre de 2026

## Para qué sirve este documento

Conecta cada requisito con el componente que lo implementa, la prueba que lo
verifica y la evidencia que lo respalda:

```
Requisito → Componente → Implementación → Prueba → Evidencia
```

Tiene dos usuarios, y conviene no perder de vista al segundo.

El primero es el equipo de desarrollo: la tabla revela de un vistazo qué
requisito no tiene prueba, que es la forma más común de que algo se dé por hecho
sin estarlo.

El segundo es el **evaluador de ingeniería de sistemas**. Para él, esta tabla es
la evidencia de que el proyecto se gestionó como ingeniería y no como
programación exploratoria. Una fila crítica sin prueba es una observación en la
defensa; la tabla completa es una respuesta.

## Cómo se mantiene

**Regla:** la tabla se actualiza en el mismo commit que el código, no después.
Forma parte de la *Definition of Done* de todo módulo y de todo método numérico.

**Verificación:** en cada cierre de hito se comprueba que ninguna fila de
prioridad crítica tenga la columna de prueba vacía.

Estados de la columna de evidencia:

| Símbolo | Significado |
|---|---|
| ✅ | Implementado, probado, y con la evidencia registrada |
| ⚠ | Implementado pero la verificación es parcial o indirecta |
| ⬚ | Pendiente |
| 🔒 | Bloqueado por un punto abierto |

---

## Tabla de trazabilidad

### Configuración

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-001 | `SimulationConfig`, lectores | `test_config` | Todos los ejemplos se ejecutan sin recompilar | ✅ |
| FR-002 | `SimulationConfig::validate` | `test_config` | Rangos imposibles abortan con código 1 | ✅ |
| FR-003 | analizador de namelist | `test_config` | Clave errónea aborta nombrándola | ✅ |
| FR-004 | lector JSON | ⬚ | — | 🔒 `C-3` |
| FR-005 | analizador de namelist | `test_config`, `test_regression` | Los `input.par` archivados se leen sin cambios | ✅ |
| FR-006 | CLI `--dry-run` | ⬚ | — | ⬚ |

### Malla y estado

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-007 (1D) | `Mesh1D` | `test_physics` | Centros y caras verificados analíticamente | ✅ |
| FR-007 (2D) | `Mesh2D` | ⬚ | — | ⬚ |
| FR-008 | `TestCaseDefinition`, `resolveTestCase` | `test_config` | Valores del usuario sobreviven; imposiciones se avisan | ✅ |
| FR-009 | `CellField` | ⬚ | Hoy la anchura es constante | ⬚ |
| FR-010 (1D) | `IdealGas`, `SimulationState` | `test_physics` | Conversión ida y vuelta dentro de redondeo | ✅ |
| FR-010 (2D) | `IdealGas` multicomponente | ⬚ | — | ⬚ |
| FR-011 | `FloorCounters` | `test_physics`, `test_integration` | Cero activaciones en casos estándar; crece al elevar el piso | ✅ |
| FR-012 | `CflController::scan` | `test_physics` | `NaN` inyectado aborta | ✅ |

### Fronteras

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-013 (1D) | tres clases de `BoundaryCondition` | `test_numerics` | Igualdad exacta celda a celda | ✅ |
| FR-013 (2D) | versiones 2D | ⬚ | — | ⬚ |
| FR-014 | frontera por borde | ⬚ | — | ⬚ |

### Reconstrucción

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-015 (orden 1) | `FirstOrderReconstruction` | ⚠ vía limitador | Funciona, pero no es módulo separado | ⚠ |
| FR-015 (MUSCL) | `MusclReconstruction` | `test_numerics`, `test_integration` | Orden de convergencia ≈ 2 medido | ✅ |
| FR-015 (WENO) | `WenoReconstruction` | ⬚ | — | 🔒 `A-2` |
| FR-016 | `limiters::*` | `test_numerics`, `test_regression` | Los cinco reproducen el Fortran | ✅ |

### Solvers de Riemann

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-017 | `HllRiemannSolver` | `test_numerics` | Consistencia y upwind supersónico | ✅ |
| FR-018 | `HlleRiemannSolver` | ⬚ | **No existe en ningún material entregado** | ⬚ |
| FR-019 | `HllcRiemannSolver` | `test_numerics`, `test_integration` | Resuelve el contacto en menos celdas que HLL | ✅ |
| FR-020 | `FaceState<NT>` | ⬚ | Pendiente del spike de dimensionalidad | ⬚ |

### Tiempo

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-021 | `SspRk2`, `ForwardEuler` | `test_integration`, `test_regression` | Número de pasos idéntico al Fortran | ✅ |
| FR-022 (1D) | `CflController` | `test_physics` | `dt` verificado analíticamente | ✅ |
| FR-022 (2D) | `CflController` sin separación | ⬚ | `INV-006`: fórmula distinta | ⬚ |

### Referencia y casos

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-023 | `SourceTerm`, `DivergenceOperator` | `test_integration` | Equilibrio hidrostático a 1.0094e-15 | ✅ |
| FR-024 | `ExactRiemannSolver` | `test_exact_riemann` | **Tabla 4.3 de Toro**, referencia externa | ✅ |
| FR-025 | `Simulation::referenceProfile` | `test_integration` | Mismo número de puntos y coordenadas | ✅ |
| FR-026 (123, doble choque) | tabla de casos | `test_config`, `test_regression` | Casos 2 y 4 de Toro | ✅ |
| FR-026 (Sod clásico, Lax) | tabla de casos | ⬚ | **Faltan los dos** | ⬚ |
| FR-027 | caso Lax-Liu 2D | ⬚ | — | 🔒 `B-3` |

### Salida

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-028 | escritor CSV | ⚠ formato `.xl` | Funciona, pero no es CSV | 🔒 `C-4` |
| FR-029 | escritor VTK | ⬚ | — | 🔒 `C-4` |
| FR-030 | `OutputManager`, `fortran_format` | `test_physics`, `test_regression` | 9 de 13 casos byte a byte | ✅ |
| FR-031 | `run_info.txt` | ⚠ | Falta el hash de commit y las opciones del compilador | ⚠ |

### Validación

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-032 | `xl_reader`, `test_regression` | `test_regression` | **26 de 26 configuraciones, pasos idénticos** | ✅ |
| FR-033 | `ErrorAnalyzer` | `test_physics` | Métricas verificadas contra valor analítico | ✅ |
| FR-034 | script de convergencia | `test_integration` | Orden ≈ 2 para MUSCL; falta la matriz completa | ⚠ |
| FR-035 | matriz de comparación | ⬚ | **El entregable central. No existe** | 🔒 `B-4` |
| FR-036 | — | `test_integration` | Conservación con periódicas por debajo de 1e-12 | ✅ |

### Extensibilidad

| Req. | Componente | Prueba | Evidencia | Estado |
|---|---|---|---|---|
| FR-037 | los cinco puntos de variación | ⬚ | **Implementado pero sin medir.** Se mide al añadir HLLE | ⚠ |
| FR-038 | `TestCaseDefinition` | `test_config` | Documentado en `extending.md` | ✅ |

---

## Requisitos no funcionales

| Req. | Mecanismo | Verificación | Estado |
|---|---|---|---|
| NFR-001 | Suite de regresión | 26 de 26 con pasos idénticos | ✅ |
| NFR-002 | Sin fuentes de no determinismo | ⬚ **Falta la prueba explícita** | ⚠ |
| NFR-003 | `run_info.txt` + documentación | ⬚ Prueba desde cero pendiente | ⬚ |
| NFR-004 | CMake multiplataforma | ⚠ Solo verificado en GCC/MSYS2 | ⚠ |
| NFR-005 | Sin dependencias externas | Compila con solo compilador y CMake | ✅ |
| NFR-006 | Configuración por valor | Inspección: sin variables globales mutables | ✅ |
| NFR-007 | Interfaces con granularidad de barrido | Inspección de las firmas | ✅ |
| NFR-008 | Función de una cara por solver | Revisión cruzada código / documento | ✅ |
| NFR-009 | Pruebas sin `Simulation` | 4 de 6 ejecutables no la instancian | ✅ |
| NFR-010 | Esta tabla | Revisada en cada cierre de hito | ⚠ |
| NFR-011 | Excepciones y códigos de salida | ⚠ Falta prueba del código 2 y 3 | ⚠ |
| NFR-012 | Advertencias completas | Cero advertencias hoy | ✅ |
| NFR-013 | Aserciones en `Debug` | Suite verde en ambas configuraciones | ✅ |
| NFR-014 | Arreglos como miembros | Inspección | ✅ |
| NFR-015 | — | 🔒 Falta fijar el número objetivo | 🔒 |
| NFR-016 | `performance.md` | Bucles enumerados; sin código paralelo | ✅ |
| NFR-017 | Conjunto documental | ⚠ Falta la vertiente pedagógica | ⚠ |

---

## Huecos que hay que cerrar

Ordenados por lo que más pesa en la defensa.

### Críticos

| Hueco | Por qué importa | Dónde se cierra |
|---|---|---|
| **FR-035** — matriz de comparación | Es el entregable que responde la pregunta de investigación. Hoy no existe | Hito 6 |
| **FR-018** — HLLE | Está en la pregunta de investigación y no existe en ningún material entregado | Hito 4 |
| **FR-037** — acoplamiento sin medir | Convierte la afirmación del `Obj-1` en un número. Se mide al añadir HLLE | Hito 4 |
| **FR-027** — caso 2D | Ambos documentos de objetivos lo exigen | Hito 10 |
| **NFR-003** — reproducibilidad desde cero | El anteproyecto la declara «no negociable» | Hito 14 |

### Importantes

| Hueco | Nota | Dónde |
|---|---|---|
| FR-026 — Sod clásico y Lax | Dos filas en una tabla. El propio texto llama a Sod «el primer caso que debe superar cualquier implementación» | Hito 3 |
| FR-015 — orden 1 como módulo | Funciona, pero el objetivo pide «módulos concretos» | Hito 3 |
| NFR-002 — prueba de determinismo | Toda la estrategia de regresión depende de ello y no está comprobado | Hito 2 |
| NFR-004 — portabilidad real | Hoy solo se verifica un compilador | Hito 2 |
| FR-031 — registro completo | Sin hash de commit, un resultado no se puede volver a producir | Hito 2 |
| NFR-011 — códigos de error | Los códigos 2 y 3 no están probados | Hito 2 |

Es significativo que cinco de los seis huecos importantes se cierren en los hitos
2 y 3, que son baratos. Son huecos de infraestructura y de constancia, no de
capacidad técnica.
