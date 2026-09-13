@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
setlocal EnableExtensions EnableDelayedExpansion

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

REM Prefer a real CPython over the Windows Store python.exe stub (error 9009).
if exist "%LocalAppData%\Programs\Python\Python312\python.exe" (
  set "PATH=%LocalAppData%\Programs\Python\Python312;%PATH%"
) else if exist "%LocalAppData%\Programs\Python\Launcher\py.exe" (
  set "PATH=%LocalAppData%\Programs\Python\Launcher;%PATH%"
)

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

REM Optional first arg: mogu-style aliases (m/te/a/b/app) or a raw ninja target.
REM `sln` is rejected: engineering management is GN only.
set "NINJA_TARGET="
set "BUILD_APP=false"
set "BUILD_WINUI=false"
set "BUILD_RENDER=false"
set "BUILD_WEBVIEW2=false"
set "BUILD_VIEWS=false"
if /I "%~1"=="sln" (
  echo ERROR: MSBuild/sln is not an engineering entry. Use build.bat ^(GN^).>&2
  echo vs2008\ and branches\ were removed; engineering entry is GN only.>&2
  popd
  exit /b 2
)

REM mogu build.sh / mgis build.bat t: fetch manifest sources (vendored skip).
REM CMake prefix stays out/third_party. Do not install into third_party/.install.
if /I "%~1"=="t" (
  if exist "%LocalAppData%\Programs\Python\Python312\python.exe" (
    set "TP_PY=%LocalAppData%\Programs\Python\Python312\python.exe"
  ) else (
    set "TP_PY=python"
  )
  if "%~2"=="" (
    "%TP_PY%" "%~dp0third_party\tools\fetch.py" --all
  ) else (
    "%TP_PY%" "%~dp0third_party\tools\fetch.py" --package "%~2"
  )
  set "ERR=!ERRORLEVEL!"
  popd
  exit /b !ERR!
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
  ) else if /I "%~1"=="app" (
    set "NINJA_TARGET=smartgis"
    set "BUILD_APP=true"
  ) else if /I "%~1"=="smartgis" (
    set "NINJA_TARGET=smartgis"
    set "BUILD_APP=true"
  ) else if /I "%~1"=="views" (
    set "NINJA_TARGET=views"
    set "BUILD_VIEWS=true"
  ) else if /I "%~1"=="web" (
    set "NINJA_TARGET=webview2"
    set "BUILD_WEBVIEW2=true"
  ) else if /I "%~1"=="webview2" (
    set "NINJA_TARGET=webview2"
    set "BUILD_WEBVIEW2=true"
  ) else if /I "%~1"=="render" (
    set "NINJA_TARGET=render"
    set "BUILD_RENDER=true"
  ) else if /I "%~1"=="winui" (
    set "NINJA_TARGET=winui"
    set "BUILD_WINUI=true"
  ) else if /I "%~1"=="e2e" (
    set "NINJA_TARGET=e2e"
    set "BUILD_APP=true"
    set "BUILD_VIEWS=true"
    set "BUILD_WEBVIEW2=true"
    set "BUILD_RENDER=true"
    set "BUILD_WINUI=true"
  ) else (
    set "NINJA_TARGET=%~1"
  )
)

if /I "!BUILD_WINUI!"=="true" (
  call "%~dp0src\app\winui\ensure_wasdk.bat"
  if errorlevel 1 (
    popd
    exit /b 1
  )
)

"%GN_PATH%gn.exe" gen out --root=./ --ide=vs2019 --args="is_debug=true is_build_third_party=false smt_run_vs_env_script=false vs_version=180 msvc_installed=true smt_build_app=!BUILD_APP! smt_build_views=!BUILD_VIEWS! smt_build_webview2=!BUILD_WEBVIEW2! smt_build_render=!BUILD_RENDER! smt_build_winui=!BUILD_WINUI!"
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

if defined NINJA_TARGET (
  ninja -j 16 -C ./out !NINJA_TARGET! > ./out/build.log
) else (
  ninja -j 16 -C ./out > ./out/build.log
)
set "ERR=%ERRORLEVEL%"
if !ERR! EQU 0 (
  if /I "!NINJA_TARGET!"=="e2e" (
    echo Running out\exe_smoke.exe --require-all
    ".\out\exe_smoke.exe" --require-all
    set "ERR=!ERRORLEVEL!"
  ) else if /I "!NINJA_TARGET!"=="test_all" (
    set "UNIT_ERR=0"
    for %%T in (rhi_test.exe model_test.exe scene_test.exe scene_gpu_test.exe sde_gdal_test.exe proj_test.exe net_test.exe tool_dispatch_test.exe) do (
      if exist ".\out\%%T" (
        echo Running out\%%T
        ".\out\%%T"
        if !ERRORLEVEL! NEQ 0 set "UNIT_ERR=!ERRORLEVEL!"
      )
    )
    if exist ".\out\exe_smoke.exe" (
      echo Running out\exe_smoke.exe
      ".\out\exe_smoke.exe"
      set "ERR=!ERRORLEVEL!"
    )
    if !UNIT_ERR! NEQ 0 set "ERR=!UNIT_ERR!"
  )
)
echo Exit code: %ERR%
echo Log: %~dp0out\build.log
popd
exit /b %ERR%
