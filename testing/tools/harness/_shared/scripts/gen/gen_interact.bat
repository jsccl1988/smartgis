@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
REM Manual regen into out/Debug/gen (same as GN action). Prefer: build.bat debug.
setlocal
REM harness/_shared/scripts/gen -> repo root (6 levels up)
set ROOT=%~dp0..\..\..\..\..\..
set JAR=%ROOT%\third_party\.tools\antlr-4.13.2-complete.jar
set OUT_CPP=%ROOT%\out\Debug\gen\app\views\shell\runtime\dsl\interact
set OUT_PY=%ROOT%\out\Debug\gen\testing\tools\loop\interact\interact_gen
py -3 "%~dp0gen_interact_gn.py" --g4 "%ROOT%\testing\tools\harness\_shared\scripts\grammar\Interact.g4" --jar "%JAR%" --out-cpp "%OUT_CPP%" --out-py "%OUT_PY%"
if errorlevel 1 exit /b 1
echo Generated C++ -^> %OUT_CPP%
echo Generated Python -^> %OUT_PY%
endlocal
