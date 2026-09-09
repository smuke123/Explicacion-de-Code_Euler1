# Validacion directa Fortran vs C++.
#
# Compila el proyecto Fortran original en una carpeta temporal (el arbol
# original NO se toca ni se ensucia con .o/.mod) y, para cada configuracion de
# la matriz, corre los dos codigos con EL MISMO input.par y compara los .xl
# celda a celda con euler1d_compare.
#
# Requisitos: MSYS2 UCRT64 con gfortran, g++, cmake y ninja.
#
# Uso:
#   powershell -ExecutionPolicy Bypass -File scripts\validate_against_fortran.ps1
#   powershell ... -Tolerance 1e-9 -KeepWork

param(
    # Si no se indica, se usa el proyecto Fortran original (carpeta hermana) y,
    # si no existe, la copia archivada en reference_fortran/ para que el repo
    # sea autocontenido.
    [string] $FortranSource = "",
    [string] $Toolchain     = "C:\msys64\ucrt64\bin",
    [double] $Tolerance     = 1e-9,
    [switch] $KeepWork
)

$ErrorActionPreference = "Stop"
$env:PATH = "$Toolchain;$env:PATH"

if (-not $FortranSource) {
    $candidates = @(
        "$PSScriptRoot\..\..\Code_Euler1D\Code_Euler1D",
        "$PSScriptRoot\..\reference_fortran"
    )
    $FortranSource = $candidates | Where-Object { Test-Path (Join-Path $_ "euler1d.f90") } | Select-Object -First 1
    if (-not $FortranSource) {
        throw "No se encontro el fuente Fortran. Indique -FortranSource <ruta>."
    }
}

$projectRoot = (Resolve-Path "$PSScriptRoot\..").Path
$cppExe      = Join-Path $projectRoot "build\xEuler1D.exe"
$compareExe  = Join-Path $projectRoot "build\euler1d_compare.exe"

foreach ($exe in @($cppExe, $compareExe)) {
    if (-not (Test-Path $exe)) {
        throw "Falta $exe. Compile primero:`n  cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`n  cmake --build build"
    }
}

$fortranSrc = (Resolve-Path $FortranSource).Path
Write-Host "fuente Fortran: $fortranSrc"
$work = Join-Path ([System.IO.Path]::GetTempPath()) "euler1d_validation"
Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $work | Out-Null

# ---------------------------------------------------------------- Fortran
Write-Host "== Compilando el Fortran original (copia temporal) =="
$fbuild = Join-Path $work "fortran"
New-Item -ItemType Directory -Force -Path $fbuild | Out-Null
foreach ($f in @("global_numbers.f90", "exact_riemann.f90", "euler1d.f90")) {
    Copy-Item (Join-Path $fortranSrc $f) $fbuild
}
Push-Location $fbuild
# Mismos flags que el Makefile del proyecto Fortran (-O2 -Wall).
& gfortran -O2 -Wall -c global_numbers.f90 exact_riemann.f90 euler1d.f90
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "fallo compilando el Fortran" }
& gfortran -O2 -Wall -o xEuler1D.exe global_numbers.o exact_riemann.o euler1d.o
if ($LASTEXITCODE -ne 0) { Pop-Location; throw "fallo enlazando el Fortran" }
Pop-Location
$fortranExe = Join-Path $fbuild "xEuler1D.exe"
Write-Host "   gfortran: $((& gfortran --version | Select-Object -First 1))"
Write-Host "   g++     : $((& g++ --version | Select-Object -First 1))"

# ------------------------------------------------------- matriz de casos
# Cruza los cinco tests de Toro, los dos solvers, los cinco limitadores y los
# tres casos con fisica adicional (entropia, atmosfera, sedov).
$cases = @()
foreach ($t in 1..5) {
    foreach ($s in @("hll", "hllc")) {
        $cases += [pscustomobject]@{ name = "toro${t}_${s}_vl"; test = $t; n = 200; solver = $s; limiter = "vl" }
    }
}
foreach ($lim in @("fo", "mm", "vl", "mc", "sb")) {
    $cases += [pscustomobject]@{ name = "toro1_hllc_$lim"; test = 1; n = 200; solver = "hllc"; limiter = $lim }
    $cases += [pscustomobject]@{ name = "toro3_hllc_$lim"; test = 3; n = 200; solver = "hllc"; limiter = $lim }
}
$cases += [pscustomobject]@{ name = "entropia_N100";  test = 6; n = 100; solver = "hllc"; limiter = "vl" }
$cases += [pscustomobject]@{ name = "entropia_N200";  test = 6; n = 200; solver = "hllc"; limiter = "mc" }
$cases += [pscustomobject]@{ name = "atmosfera_N100"; test = 7; n = 100; solver = "hllc"; limiter = "vl" }
$cases += [pscustomobject]@{ name = "atmosfera_N200"; test = 7; n = 200; solver = "hll";  limiter = "mm" }
$cases += [pscustomobject]@{ name = "sedov_N100";     test = 8; n = 100; solver = "hllc"; limiter = "vl" }
$cases += [pscustomobject]@{ name = "sedov_N200";     test = 8; n = 200; solver = "hllc"; limiter = "mm" }

# ------------------------------------------------------------- ejecucion
$results = @()
foreach ($case in $cases) {
    $caseDir    = Join-Path $work $case.name
    $fortranDir = Join-Path $caseDir "fortran"
    $cppDir     = Join-Path $caseDir "cpp"
    New-Item -ItemType Directory -Force -Path $fortranDir, $cppDir | Out-Null

    $namelist = @(
        "&euler"
        " test = $($case.test)"
        " N = $($case.n)"
        " cfl = 0.9"
        " gamma = 1.4"
        " limiter = '$($case.limiter)'"
        " solver = '$($case.solver)'"
        "/"
    ) -join "`n"
    Set-Content -Path (Join-Path $fortranDir "input.par") -Value $namelist -NoNewline:$false
    Copy-Item (Join-Path $fortranDir "input.par") (Join-Path $cppDir "input.par")

    Push-Location $fortranDir
    $fortranOut = & $fortranExe 2>&1
    $fortranCode = $LASTEXITCODE
    Pop-Location
    Set-Content -Path (Join-Path $caseDir "fortran_stdout.txt") -Value $fortranOut

    $cppOut = & $cppExe --input (Join-Path $cppDir "input.par") --output $cppDir 2>&1
    $cppCode = $LASTEXITCODE
    Set-Content -Path (Join-Path $caseDir "cpp_stdout.txt") -Value $cppOut

    if ($fortranCode -ne 0 -or $cppCode -ne 0) {
        Write-Host ("{0,-22} ERROR de ejecucion (fortran={1}, cpp={2})" -f $case.name, $fortranCode, $cppCode) -ForegroundColor Red
        $results += [pscustomobject]@{ case = $case.name; status = "ERROR"; detail = "codigos $fortranCode/$cppCode" }
        continue
    }

    # Numero de pasos: firma muy sensible, solo coincide si toda la secuencia
    # de dt coincidio.
    $fSteps = ($fortranOut | Select-String -Pattern "pasos:\s+(\d+)").Matches.Groups[1].Value
    $cSteps = ($cppOut     | Select-String -Pattern "pasos:\s+(\d+)").Matches.Groups[1].Value

    $compareOut = & $compareExe $fortranDir $cppDir $Tolerance 2>&1
    $compareCode = $LASTEXITCODE
    Set-Content -Path (Join-Path $caseDir "compare.txt") -Value $compareOut

    $worst = 0.0
    foreach ($m in ($compareOut | Select-String -Pattern "norm=([0-9.eE+-]+)").Matches) {
        $v = [double] $m.Groups[1].Value
        if ($v -gt $worst) { $worst = $v }
    }

    $stepsOk = ($fSteps -eq $cSteps)
    $ok = ($compareCode -eq 0) -and $stepsOk
    $color = if ($ok) { "Green" } else { "Red" }
    Write-Host ("{0,-22} pasos F/C++ = {1}/{2}  peor error normalizado = {3:e3}  {4}" -f `
        $case.name, $fSteps, $cSteps, $worst, $(if ($ok) { "OK" } else { "FALLA" })) -ForegroundColor $color

    $results += [pscustomobject]@{
        case = $case.name; status = $(if ($ok) { "OK" } else { "FALLA" })
        fortran_steps = $fSteps; cpp_steps = $cSteps; worst_normalized = $worst
    }
}

# --------------------------------------------------------------- resumen
$failed = @($results | Where-Object { $_.status -ne "OK" })
$maxWorst = ($results | Where-Object { $_.worst_normalized -ne $null } |
             Measure-Object -Property worst_normalized -Maximum).Maximum

Write-Host ""
Write-Host "== Resumen =="
Write-Host "casos            : $($results.Count)"
Write-Host "dentro de tol.   : $($results.Count - $failed.Count)"
Write-Host "fuera de tol.    : $($failed.Count)"
Write-Host "peor error norm. : $maxWorst  (tolerancia $Tolerance)"

$csv = Join-Path $projectRoot "docs\validation_results.csv"
$results | Export-Csv -Path $csv -NoTypeInformation
Write-Host "detalle          : $csv"

if ($KeepWork) {
    Write-Host "trabajo          : $work (conservado)"
} else {
    Remove-Item -Recurse -Force $work -ErrorAction SilentlyContinue
}

if ($failed.Count -gt 0) { exit 1 }
exit 0
