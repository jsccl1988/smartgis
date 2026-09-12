@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
setlocal EnableExtensions

REM CMD cannot use a UNC path as the working directory; pushd maps it to a drive letter.
pushd "%~dp0" || (
  echo ERROR: failed to enter "%~dp0"
  exit /b 1
)

if exist "D:\Dev\depot_tools" set "PATH=%PATH%;D:\Dev\depot_tools"

if not defined BUILDTOOLS_PATH (
  if exist "%~dp0build\bin\gn.exe" (
    set "GN_PATH=%~dp0build\bin\"
  ) else if exist "D:\Dev\buildtools\win\gn.exe" (
    set "BUILDTOOLS_PATH=D:\Dev\buildtools"
    set "GN_PATH=%BUILDTOOLS_PATH%\win\"
  ) else (
    echo gn.exe not found; fetching into build\bin ...
    python "%~dp0build\fetch_binaries.py"
    if errorlevel 1 (
      echo ERROR: failed to fetch build binaries. Install Python, or set BUILDTOOLS_PATH.
      popd
      exit /b 1
    )
    set "GN_PATH=%~dp0build\bin\"
  )
) else (
  set "GN_PATH=%BUILDTOOLS_PATH%\win\"
)

if exist "%~dp0build\bin" set "PATH=%PATH%;%~dp0build\bin"

if not exist "%GN_PATH%gn.exe" (
  echo ERROR: gn.exe not found at "%GN_PATH%gn.exe"
  popd
  exit /b 1
)

if not exist ".\out" mkdir ".\out"

REM Ninja MSVC wrapper env (no Python required).
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\config\win\write_msvc_env.ps1" -OutFile "%~dp0out\environment.x64.x64" -Arch x64
if errorlevel 1 (
  echo ERROR: failed to write out\environment.x64.x64
  popd
  exit /b 1
)

"%GN_PATH%gn.exe" gen out --root=./ --ide=vs2019 --args="is_debug=true is_build_third_party=false smt_run_vs_env_script=false vs_version=180 msvc_installed=true"
if errorlevel 1 (
  popd
  exit /b 1
)

where ninja >nul 2>&1
if errorlevel 1 (
  echo ERROR: ninja not found on PATH. Put it in build\bin or D:\Dev\depot_tools.
  popd
  exit /b 1
)

REM Optional first arg: mogu-style aliases (m/te/a/b) or a raw ninja target.
REM `sln` is rejected: engineering management is GN only.
set "NINJA_TARGET="
if /I "%~1"=="sln" (
  echo ERROR: MSBuild/sln is not an engineering entry. Use build.bat ^(GN^).>&2
  echo vs2008\ and branches\ were removed; engineering entry is GN only.>&2
  popd
  exit /b 2
)
if not "%~1"=="" (
  if /I "%~1"=="m" (
    set "NINJA_TARGET=all"
  ) else if /I "%~1"=="te" (
    set "NINJA_TARGET=test_all"
  ) else if /I "%~1"=="a" (
    set "NINJA_TARGET=all_with_tests"
  ) else if /I "%~1"=="b" (
    set "NINJA_TARGET=benchmark_all"
  ) else (
    set "NINJA_TARGET=%~1"
  )
)

if defined NINJA_TARGET (
  ninja -j 16 -C ./out %NINJA_TARGET% > ./out/build.log
) else (
  ninja -j 16 -C ./out > ./out/build.log
)
set "ERR=%ERRORLEVEL%"
echo Exit code: %ERR%
echo Log: %~dp0out\build.log
popd
exit /b %ERR%
