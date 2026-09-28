# Flujo de desarrollo y definición de terminado

**Fecha:** 27 de septiembre de 2026

## Ciclo de vida de un cambio

```
Requisito (FR/NFR con criterio de aceptación)
        ↓
Diseño (¿encaja en un punto de variación existente?)
        ↓
Prueba primero (falla)
        ↓
Implementación (la prueba pasa)
        ↓
Pruebas de integración
        ↓
Regresión + validación
        ↓
Documentación + trazabilidad
        ↓
Commit
```

Dos observaciones sobre este ciclo, porque de otro modo se convierte en
ceremonia.

**El paso de diseño es una pregunta, no un documento.** «¿Encaja en un punto de
variación existente?» Si la respuesta es sí, se implementa. Si es no, hay una
decisión arquitectónica que registrar en `decision_log.md` antes de escribir
código. La mayoría de los cambios responden sí y el paso dura treinta segundos.

**«Prueba primero» significa que la prueba debe fallar antes de implementar.**
Una prueba que nunca se vio fallar no ha demostrado que comprueba algo. Es el
único requisito real del ciclo; el orden de escritura importa menos que haber
observado el fallo.

## Ritmo de ejecución de pruebas

Los tres niveles tienen coste muy distinto y no tiene sentido ejecutarlos con la
misma frecuencia.

| Cuándo | Qué se ejecuta | Duración |
|---|---|---|
| Al compilar | Unitarias e integración | segundos |
| Antes de un commit | Todo, incluida la regresión | ~10 s en Release |
| Antes de cerrar un hito | Todo + validación directa contra el Fortran | minutos |
| En CI, en cada envío | Todo, en GCC y Clang, Debug y Release | minutos |

Esta es la razón de separar `tests/unit`, `tests/integration` y
`tests/regression` en directorios distintos (ver `coding_standards.md` §1).

---

# Definición de terminado

Una tarea está terminada cuando cumple **todos** los puntos de su lista. No hay
«terminado salvo la documentación»: eso es no terminado.

## Un módulo nuevo

- [ ] Implementa una interfaz existente, o su nuevo punto de variación está
      registrado como decisión
- [ ] Comentario de documentación en la cabecera con el contrato: qué garantiza,
      qué exige del llamador, unidades y convenciones
- [ ] Pruebas unitarias que no construyen una simulación
- [ ] Al menos una prueba que **falló** antes de la implementación
- [ ] Sin advertencias con el conjunto completo activado
- [ ] Verde en `Debug` y en `Release`
- [ ] Sin estado global mutable ni asignación de memoria en el bucle temporal
- [ ] Regresión intacta, o el cambio explicado en el mensaje del commit
- [ ] `traceability.md` actualizado: requisito → componente → prueba
- [ ] Si es seleccionable por configuración: valor en el enumerado, conversiones
      de cadena en ambos sentidos, rama en la fábrica y ejemplo en `examples/`

## Un método numérico nuevo

Todo lo del módulo nuevo, **más**:

- [ ] Procedencia de cada fórmula citada en un comentario: fuente, edición,
      ecuación
- [ ] Propiedades matemáticas verificadas. Para un solver de Riemann: las cuatro
      de `validation_strategy.md` §1
- [ ] Una función de una sola cara, legible y comparable con la fórmula, además
      del barrido optimizado
- [ ] Prueba de que la función legible y el barrido coinciden **exactamente**
- [ ] **Análisis de convergencia superado** antes de usarlo en cualquier
      comparación
- [ ] Documentado en `numerical_methods.md`
- [ ] **Número de archivos del núcleo modificados, contado y registrado.** El
      criterio es cero (FR-037)

El penúltimo punto es el que produce la evidencia del `Obj-1`. Sin él, la
afirmación arquitectónica de la tesis queda argumentada en lugar de medida.

## Una corrección de defecto

- [ ] Prueba que reproduce el fallo, escrita **antes** de la corrección, y que se
      vio fallar
- [ ] Causa raíz identificada, no solo el síntoma
- [ ] Si el defecto está en el código de referencia: clasificado como preservado,
      reestructurado o desviación deliberada, según el procedimiento de
      `numerical_invariants.md` Parte III
- [ ] Si es desviación deliberada: commit aislado, registro en `decision_log.md`,
      y efecto sobre la regresión reportado
- [ ] Búsqueda de la misma clase de defecto en el resto del código

El último punto vale la pena: varios de los hallazgos de la auditoría del
proyecto del director resultaron afectar a los seis códigos, no solo al que se
estaba auditando.

## Un caso de prueba nuevo

- [ ] Entrada declarativa en la tabla de casos
- [ ] Constante de dimensionamiento actualizada (si se olvida, **no compila**)
- [ ] Archivo de configuración de ejemplo en `examples/`
- [ ] Prueba que verifica los valores declarados
- [ ] Marcado correctamente si tiene o no solución de referencia. **No se
      escriben referencias sin significado**
- [ ] Documentado: qué problema es, qué fallo detecta, de dónde salen los valores
- [ ] Si se congela como línea base: en `tests/baseline/`, nunca en
      `tests/reference/`

El penúltimo punto es el más valioso y el más olvidado. Un caso de prueba cuyo
propósito no está escrito se convierte, al cabo de unos meses, en un número que
nadie se atreve a cambiar.

## Un modelo físico nuevo

Fuera del alcance actual, pero definido por si el alcance cambia.

- [ ] Todo lo del método numérico nuevo
- [ ] Formulación matemática documentada antes de implementar
- [ ] Referencia de validación identificada. **Si no hay ninguna disponible, no
      se implementa**: no se puede validar
- [ ] `PENDING INPUT FROM DIRECTOR` — cualquier modelo físico nuevo sale del
      alcance acordado y requiere decisión suya

## Un hito del roadmap

- [ ] Todas sus tareas terminadas según su lista correspondiente
- [ ] Suite completa verde en las cuatro combinaciones de CI
- [ ] Validación directa contra el Fortran ejecutada y su resultado registrado
- [ ] `traceability.md` sin filas críticas huérfanas
- [ ] Documentación afectada actualizada
- [ ] Etiqueta de versión creada, para que la memoria pueda citarla

---

# Revisión

Con dos autores, la revisión cruzada es el mecanismo principal de control de
calidad. La metodología del anteproyecto (Etapa 4) ya la establece para la
documentación; conviene extenderla al código.

**Qué se revisa siempre:**

- Cualquier cambio a una fórmula numérica, sin excepción.
- Cualquier cambio a un invariante de `numerical_invariants.md`.
- Cualquier desviación deliberada del comportamiento de referencia.

**Qué se puede integrar sin revisión:** documentación, formato, pruebas nuevas
que no cambian código de producción.

**Preguntas útiles en la revisión de código numérico**, en orden de rendimiento:

1. ¿Se puede verificar esta fórmula leyéndola contra su fuente citada?
2. ¿Cambió el número de pasos temporales de algún caso de regresión?
3. ¿Hay alguna operación de punto flotante reordenada respecto a la referencia?
4. ¿Hay algún error que se pueda producir en silencio?
5. ¿Alguna prueba nueva se vio fallar antes de que existiera la implementación?

La cuarta es la que más ha rendido en este proyecto: los cuatro hallazgos más
graves de la auditoría del código de referencia eran, todos, fallos silenciosos.

---

# Gestión de las decisiones abiertas

`alcance.md` mantiene doce puntos abiertos (`A-1` a `C-5`). Mientras estén
abiertos:

- **No se bloquea trabajo que no dependa de ellos.** El diseño del 2D no depende
  de si se usa Catch2.
- El trabajo que sí depende se marca como bloqueado en el roadmap, con el punto
  abierto que lo bloquea.
- Cuando se resuelve uno, se registra en `decision_log.md` con la fecha y quién
  lo decidió, y se actualizan los documentos afectados en el mismo commit.

Los cuatro que más bloquean son `A-1` (qué documento de objetivos rige), `A-2`
(si WENO es entregable), `B-4` (alcance de la matriz de comparación) y `C-1` a
`C-4` (las alineaciones técnicas).
