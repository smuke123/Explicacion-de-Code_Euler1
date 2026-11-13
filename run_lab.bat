@echo off
cd /d "%~dp0"
if exist C:\msys64\ucrt64\bin set "PATH=C:\msys64\ucrt64\bin;%PATH%"
if not defined EULER1D_BIN (
  if exist "%~dp0build\xEuler1D.exe" set "EULER1D_BIN=%~dp0build\xEuler1D.exe"
)
python tools\fvlab_lab\app.py
if errorlevel 1 pause
