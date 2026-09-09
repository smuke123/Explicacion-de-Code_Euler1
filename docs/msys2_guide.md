# Guía de uso con MSYS2 UCRT64

Todo lo de esta guía está verificado en la máquina del proyecto
(MSYS2 en `C:\msys64`, GCC 16.1.0, CMake 4.3.3, Ninja).

## 1. Cuál de las shells de MSYS2 usar

MSYS2 instala varios lanzadores en `C:\msys64`, y **no son equivalentes**:

| Lanzador | Para qué | ¿Sirve aquí? |
|---|---|---|
| `ucrt64.exe` | GCC contra la **UCRT** (la CRT moderna de Windows) | **Sí, es el que hay que usar** |
| `mingw64.exe` | GCC contra `msvcrt.dll` (antigua) | Compilaría, pero mezclar runtimes da problemas |
| `clang64.exe` | Clang + UCRT | Alternativa válida, no probada aquí |
| `msys2.exe` | Entorno POSIX de emulación | **No.** Produce binarios que dependen de `msys-2.0.dll` |

Abre siempre **`C:\msys64\ucrt64.exe`**. Se reconoce porque el prompt dice
`UCRT64`. Si dice `MSYS`, has abierto la equivocada.

## 2. Comprobar e instalar el toolchain

Dentro de la shell UCRT64:

```bash
gcc --version && g++ --version && gfortran --version && cmake --version && ninja --version
```

En esta máquina ya están los cinco. Si te falta alguno:

```bash
pacman -S --needed \
  mingw-w64-ucrt-x86_64-gcc \
  mingw-w64-ucrt-x86_64-gcc-fortran \
  mingw-w64-ucrt-x86_64-cmake \
  mingw-w64-ucrt-x86_64-ninja
```

`gfortran` **solo** es necesario para `scripts/validate_against_fortran.ps1`
(compilar el código original y comparar). Para compilar y usar el C++ no hace
falta.

Recomendables pero no instalados ahora mismo:

```bash
pacman -S --needed mingw-w64-ucrt-x86_64-gdb        # depurador
pacman -S --needed mingw-w64-ucrt-x86_64-python-matplotlib \
                   mingw-w64-ucrt-x86_64-python-numpy   # graficar resultados
```

> Los paquetes de UCRT64 llevan el prefijo `mingw-w64-ucrt-x86_64-`. Si instalas
> `mingw-w64-x86_64-gcc` (sin `ucrt`) estarás instalando el toolchain del
> entorno MINGW64, que es otro.

## 3. Compilar

Dentro de la shell UCRT64, las rutas de Windows se escriben en estilo Unix:
`C:\Users\...` es `/c/Users/...`.

```bash
cd "/c/Users/USER/OneDrive/Documentos/Tesis/Euler1D_Cpp"

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Eso produce en `build/`:

| Ejecutable | Qué es |
|---|---|
| `xEuler1D.exe` | El solver |
| `euler1d_compare.exe` | Comparador cuantitativo de dos corridas |
| `tests/test_*.exe` | Los seis ejecutables de prueba |

Para recompilar tras editar código basta `cmake --build build`; Ninja recompila
solo lo que cambió. Solo hace falta repetir el `cmake -S . -B build` si añades o
quitas archivos de `CMakeLists.txt`.

### Compilación en paralelo

Ninja ya usa todos los núcleos. Si quieres limitarlo:

```bash
cmake --build build -- -j4
```

## 4. Ejecutar

```bash
./build/xEuler1D.exe                                          # lee ./input.par
./build/xEuler1D.exe -i examples/toro1_hllc_vl.par -o out/toro1
./build/xEuler1D.exe --help
```

Salida esperada del segundo:

```
 config: test= 1  N=  400  solver=hllc  limiter=vl  cfl= 0.900
 exacto: p* =  4.662936E-01   u* =  1.360906E+00
 pasos:    245   t final:   0.200000
```

Y en `out/toro1/` quedan `rho_1.xl`, `vel_1.xl`, `press_1.xl`, los tres
`*_exact.xl` y `run_info.txt`.

> **Cuidado con el directorio de salida.** Sin `-o`, el programa escribe en el
> directorio actual, igual que hacía el Fortran. Si lo lanzas desde la raíz del
> proyecto te dejará los `.xl` ahí (están en `.gitignore`, así que no ensucian
> el repositorio, pero conviene usar `-o`).

## 5. Ejecutar los tests

```bash
ctest --test-dir build --output-on-failure
```

Toma ~11 s. Para ejecutar solo uno:

```bash
ctest --test-dir build -R test_numerics --output-on-failure
```

O directamente el binario, que da la lista caso por caso:

```bash
./build/tests/test_numerics.exe
```

## 6. Compilación en Debug

La versión Debug activa los `assert` de `CellField::operator[]`, que detectan
accesos fuera del rango `[-1, N+2]`. Merece la pena usarla mientras desarrollas.

```bash
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
```

Los dos directorios de build coexisten sin estorbarse. La suite en Debug tarda
~41 s en lugar de ~11 s.

Con `gdb` instalado:

```bash
gdb ./build-debug/tests/test_numerics.exe
(gdb) run
(gdb) bt
```

## 7. Validar contra el Fortran

El script está en PowerShell, así que se lanza desde **PowerShell**, no desde la
shell de MSYS2:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cd "C:\Users\USER\OneDrive\Documentos\Tesis\Euler1D_Cpp"
powershell -ExecutionPolicy Bypass -File scripts\validate_against_fortran.ps1
```

Compila el Fortran en una carpeta temporal, corre los dos códigos con el mismo
`input.par` sobre 26 configuraciones y compara. Con `-KeepWork` conserva los
directorios temporales para inspeccionar una discrepancia.

## 8. Trabajar desde PowerShell en lugar de la shell de MSYS2

Funciona igual; solo hay que poner el toolchain en el `PATH` **al principio** de
cada sesión:

```powershell
$env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
cd "C:\Users\USER\OneDrive\Documentos\Tesis\Euler1D_Cpp"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
.\build\xEuler1D.exe -i examples\toro1_hllc_vl.par -o out\toro1
```

Ese `$env:PATH` dura solo lo que dure la ventana. Para no repetirlo, añádelo a
tu perfil de PowerShell:

```powershell
notepad $PROFILE
```

y pega la línea del `$env:PATH`.

> **Por qué va delante y no detrás.** Si `C:\msys64\ucrt64\bin` queda al final
> del `PATH`, otro `cmake` o `python` instalado en el sistema puede ganar la
> resolución y acabarás compilando con un toolchain distinto del que crees.

## 9. Graficar los resultados

El formato `.xl` es texto plano en bloques, así que cualquier herramienta sirve.
Con Python (requiere los paquetes del punto 2):

```python
import matplotlib.pyplot as plt

def leer_ultimo_bloque(ruta):
    bloques, actual = [], None
    with open(ruta) as f:
        for linea in f:
            if "#Time" in linea:
                actual = ([], [])
                bloques.append((float(linea.split("=")[1]), actual))
            elif linea.strip() and actual is not None:
                x, v = linea.split()
                actual[0].append(float(x))
                actual[1].append(float(v))
    return bloques[-1]

t, (x, rho) = leer_ultimo_bloque("out/toro1/rho_1.xl")
_, (xe, rho_e) = leer_ultimo_bloque("out/toro1/rho_exact.xl")

plt.plot(xe, rho_e, "k-", lw=1, label="exacta")
plt.plot(x, rho, "r.", ms=3, label="HLLC + van Leer")
plt.xlabel("x"); plt.ylabel(r"$\rho$"); plt.title(f"Toro 1, t = {t}")
plt.legend(); plt.show()
```

El proyecto Fortran original tiene scripts en `tools/` que no se migraron. Como
el formato de salida es idéntico, deberían funcionar con estos `.xl` sin
cambios, aunque no se ha comprobado.

## 10. Problemas frecuentes

| Síntoma | Causa y solución |
|---|---|
| `cmake: command not found` en la shell | Abriste `msys2.exe` en vez de `ucrt64.exe`, o falta el paquete |
| `'cmake' no se reconoce` en PowerShell | Falta el `$env:PATH` del punto 8 |
| `CMake Error: could not find CMAKE_CXX_COMPILER` | Igual que arriba: CMake no ve `g++` |
| El ejecutable no arranca al hacer doble clic fuera de la shell | Le faltan las DLL de UCRT64. Ejecútalo desde la shell UCRT64, o añade `C:\msys64\ucrt64\bin` al `PATH` del sistema |
| `The system cannot find the path specified` con rutas de OneDrive | Pon la ruta entre comillas: tiene espacios (`Documentos`, `OneDrive`) |
| `test_regression` falla tras clonar el repo | Suele ser conversión de fin de línea en los `.xl`. Comprueba `git check-attr -a tests/reference/toro1/rho_1.xl`: debe decir `text: unset` |
| Cambié `input.par` y no pasa nada | Estás leyendo otro archivo. `xEuler1D` lee `./input.par` salvo que uses `-i` |
| `ERROR de configuracion: clave desconocida` | Errata en el namelist. Es deliberado que aborte en lugar de ignorar la clave |
| Compilar tarda mucho la primera vez | Normal: 32 objetos, incluidos los seis ejecutables de test. Después Ninja es incremental |

## 11. Nota sobre OneDrive

El proyecto vive dentro de OneDrive, que funciona pero tiene dos efectos:

- **Sincroniza `build/`** si no lo excluyes, y son cientos de MB de objetos.
  `.gitignore` protege el repositorio, pero no a OneDrive. Puedes excluir la
  carpeta desde el panel de OneDrive, o compilar fuera del árbol:
  `cmake -S . -B C:\builds\euler1d -G Ninja`.
- **Puede bloquear archivos** mientras los sincroniza, lo que ocasionalmente da
  un error de escritura al recompilar. Reintentar suele bastar.
