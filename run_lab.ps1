$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root
if (Test-Path "C:\msys64\ucrt64\bin") {
    $env:PATH = "C:\msys64\ucrt64\bin;" + $env:PATH
}

if (-not $env:EULER1D_BIN) {
    if (Test-Path "$root\build\xEuler1D.exe") {
        $env:EULER1D_BIN = "$root\build\xEuler1D.exe"
    } elseif (Test-Path "$root\build\xEuler1D") {
        $env:EULER1D_BIN = "$root\build\xEuler1D"
    }
}

python -c "import matplotlib, numpy" 2>$null
if ($LASTEXITCODE -ne 0) {
    python -m pip install -r "$root\tools\requirements-lab.txt"
}

python "$root\tools\fvlab_lab\app.py"
