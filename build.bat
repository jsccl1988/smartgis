@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
setlocal EnableExtensions EnableDelayedExpansion

REM CMD cannot use a UNC path as the working directory; pushd maps it to a drive letter.
pushd "%~dp0" || (
  echo ERROR: failed to enter "%~dp0"
  exit /b 1
)

REM Per-config out\.build.lock.{debug,release,shared} for gn/ninja only (te/e2e unlocked after).
REM Wrapper: build\config\win\with_build_lock.ps1 sets SMARTGIS_BUILD_PHASE.
if not defined SMARTGIS_BUILD_LOCK_HELD (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\config\win\with_build_lock.ps1" %*
  set "LOCK_RC=!ERRORLEVEL!"
  popd
  exit /b !LOCK_RC!
)
if not defined SMARTGIS_BUILD_PHASE set "SMARTGIS_BUILD_PHASE=all"

REM tests phase: parse args and run te/e2e only (no gn/ninja).
if /I "!SMARTGIS_BUILD_PHASE!"=="tests" goto :parse_args_for_tests

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
if not exist ".\out\Debug" mkdir ".\out\Debug"
if not exist ".\out\Release" mkdir ".\out\Release"

REM Ninja MSVC wrapper env (no Python required). Shared file + per-config copies
REM because ninja -t msvc resolves environment.* under the -C out dir.
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0build\config\win\write_msvc_env.ps1" -OutFile "%~dp0out\environment.x64.x64" -Arch x64
if errorlevel 1 (
  echo ERROR: failed to write out\environment.x64.x64
  popd
  exit /b 1
)
copy /Y "%~dp0out\environment.x64.x64" "%~dp0out\Debug\environment.x64.x64" >nul
copy /Y "%~dp0out\environment.x64.x64" "%~dp0out\Release\environment.x64.x64" >nul

REM Optional first arg: debug|release (config filter), then mogu-style aliases
REM (m/te/a/b/app) or a raw ninja target. Default: build both configs.
REM `sln` is rejected: engineering management is GN only.
set "BUILD_DEBUG=1"
set "BUILD_RELEASE=1"
set "TARGET_ARG=%~1"
if /I "%~1"=="debug" (
  set "BUILD_RELEASE=0"
  set "TARGET_ARG=%~2"
) else if /I "%~1"=="release" (
  set "BUILD_DEBUG=0"
  set "TARGET_ARG=%~2"
)

set "NINJA_TARGET="
set "BUILD_APP=false"
set "BUILD_RENDER=false"
set "BUILD_VIEWS=false"
if /I "!TARGET_ARG!"=="sln" (
  echo ERROR: MSBuild/sln is not an engineering entry. Use build.bat ^(GN^).>&2
  echo vs2008\ and branches\ were removed; engineering entry is GN only.>&2
  popd
  exit /b 2
)

REM mogu build.sh / mgis build.bat t: batch install to third_party/.install
if /I "!TARGET_ARG!"=="t" (
  if exist "%LocalAppData%\Programs\Python\Python312\python.exe" (
    set "TP_PY=%LocalAppData%\Programs\Python\Python312\python.exe"
  ) else if exist "%LocalAppData%\Programs\Python\Launcher\py.exe" (
    set "TP_PY=%LocalAppData%\Programs\Python\Launcher\py.exe"
  ) else (
    set "TP_PY=python"
  )
  REM VS-bundled CMake is often absent from PATH outside a Developer Prompt.
  if exist "%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe" (
    set "PATH=%ProgramFiles%\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;!PATH!"
  ) else if exist "%ProgramFiles%\CMake\bin\cmake.exe" (
    set "PATH=%ProgramFiles%\CMake\bin;!PATH!"
  )
  REM Config filter: `build.bat release t` installs Release DLLs (gdal.dll);
  REM bare `build.bat t` / `debug t` keep Debug (gdald.dll). Both can coexist
  REM under the same .install prefix.
  set "TP_BUILD_TYPE=Debug"
  if "!BUILD_RELEASE!"=="1" if "!BUILD_DEBUG!"=="0" set "TP_BUILD_TYPE=Release"
  REM For `build.bat t <pkg>` the package is %~2 when no config prefix, else %~3.
  set "TP_PKG="
  if /I "%~1"=="debug" (
    set "TP_PKG=%~3"
  ) else if /I "%~1"=="release" (
    set "TP_PKG=%~3"
  ) else (
    set "TP_PKG=%~2"
  )
  echo === third_party batch install ^(build-type=!TP_BUILD_TYPE!^) ===
  if "!TP_PKG!"=="" (
    "!TP_PY!" "%~dp0third_party\tools\batch.py" --manifest "%~dp0third_party\manifest.json" --install-prefix "%~dp0third_party\.install" --build-type "!TP_BUILD_TYPE!"
  ) else (
    "!TP_PY!" "%~dp0third_party\tools\batch.py" --manifest "%~dp0third_party\manifest.json" --install-prefix "%~dp0third_party\.install" --build-type "!TP_BUILD_TYPE!" --package "!TP_PKG!"
  )
  set "ERR=!ERRORLEVEL!"
  if !ERR! EQU 0 (
    if not exist "%~dp0out" mkdir "%~dp0out"
    if not exist "%~dp0out\third_party" (
      mklink /J "%~dp0out\third_party" "%~dp0third_party\.install"
    )
    if not exist "%~dp0out\Debug\third_party" (
      mklink /J "%~dp0out\Debug\third_party" "%~dp0third_party\.install"
    )
    if not exist "%~dp0out\Release\third_party" (
      mklink /J "%~dp0out\Release\third_party" "%~dp0third_party\.install"
    )
  )
  popd
  exit /b !ERR!
)

if not "!TARGET_ARG!"=="" (
  if /I "!TARGET_ARG!"=="m" (
    set "NINJA_TARGET=all"
  ) else if /I "!TARGET_ARG!"=="te" (
    set "NINJA_TARGET=test_all"
  ) else if /I "!TARGET_ARG!"=="a" (
    set "NINJA_TARGET=all_with_tests"
  ) else if /I "!TARGET_ARG!"=="b" (
    set "NINJA_TARGET=benchmark_all"
  ) else if /I "!TARGET_ARG!"=="app" (
    REM Product entry is Views.
    set "NINJA_TARGET=views"
    set "BUILD_VIEWS=true"
  ) else if /I "!TARGET_ARG!"=="views" (
    set "NINJA_TARGET=views"
    set "BUILD_VIEWS=true"
  ) else if /I "!TARGET_ARG!"=="render" (
    set "NINJA_TARGET=render"
    set "BUILD_RENDER=true"
  ) else if /I "!TARGET_ARG!"=="e2e" (
    set "NINJA_TARGET=e2e"
    set "BUILD_VIEWS=true"
    set "BUILD_RENDER=true"
  ) else (
    set "NINJA_TARGET=!TARGET_ARG!"
  )
)

where ninja >nul 2>&1
if errorlevel 1 (
  echo ERROR: ninja not found on PATH. Put it in build\bin or D:\Dev\depot_tools.
  popd
  exit /b 1
)

set "ERR=0"
set "LAST_LOG="

if "!BUILD_DEBUG!"=="1" (
  call :build_config Debug true
  set "CFG_RC=!ERRORLEVEL!"
  if !CFG_RC! NEQ 0 (
    set "ERR=!CFG_RC!"
    goto :finish
  )
)

if "!BUILD_RELEASE!"=="1" (
  call :build_config Release false
  set "CFG_RC=!ERRORLEVEL!"
  if !CFG_RC! NEQ 0 (
    set "ERR=!CFG_RC!"
    goto :finish
  )
)

REM te / e2e runners use Debug binaries only (unlocked when PHASE=compile+tests split).
if /I "!SMARTGIS_BUILD_PHASE!"=="compile" goto :finish
if "!BUILD_DEBUG!"=="1" (
  if /I "!NINJA_TARGET!"=="e2e" (
    echo Running out\Debug\exe_smoke.exe --require-all
    ".\out\Debug\exe_smoke.exe" --require-all
    set "ERR=!ERRORLEVEL!"
  ) else if /I "!NINJA_TARGET!"=="test_all" (
    set "UNIT_ERR=0"
    REM leftover_session_test / leftover_record_test / leftover rhi2d-gdi/gl/dem_stereo
    REM PEs removed with src/legacy; run scenic_* twins instead.
    for %%T in (rhi_test.exe model_test.exe scene_test.exe scene_gpu_test.exe unified_draw_test.exe leftover_mesh_test.exe ogr_text_encoding_test.exe sdbd_client_test.exe sdbd_live_test.exe sde_gdal_test.exe geo_ogr_test.exe proj_test.exe stat_expr_test.exe tin_delaunay_test.exe orthogrid_laplace_test.exe net_test.exe tool_dispatch_test.exe draft_test.exe camera_nav_test.exe content_view_host_test.exe content_feature_attrs_test.exe content_catalog_layers_test.exe content_embed_sample_test.exe land_mask_test.exe views_unittests.exe markup_unittests.exe views_pixel_tests.exe ipc_test.exe render_backend_test.exe tile_test.exe style_test.exe map2d_test.exe map2d_pass_test.exe map_scene_test.exe scene3d_presenter_test.exe dem_raster_test.exe scenic_gdi_map_paint_test.exe scenic_map_carto2d_test.exe scenic_gl_map_paint_test.exe scenic_dem_stereo_test.exe menu_test.exe select_query_test.exe plugin_host_test.exe processing_ops_test.exe) do (
      if exist ".\out\Debug\%%T" (
        echo Running out\Debug\%%T
        ".\out\Debug\%%T"
        if !ERRORLEVEL! NEQ 0 set "UNIT_ERR=!ERRORLEVEL!"
      )
    )
    if exist ".\out\Debug\exe_smoke.exe" (
      echo Running out\Debug\exe_smoke.exe
      ".\out\Debug\exe_smoke.exe"
      set "ERR=!ERRORLEVEL!"
    )
    if !UNIT_ERR! NEQ 0 set "ERR=!UNIT_ERR!"
  )
)

:finish
echo Exit code: !ERR!
if defined LAST_LOG (
  echo Log: !LAST_LOG!
) else (
  echo Log: %~dp0out\Debug\build.log / %~dp0out\Release\build.log
)
popd
exit /b !ERR!

REM ---------------------------------------------------------------------------
REM tests-only entry (SMARTGIS_BUILD_PHASE=tests): no gn/ninja
REM ---------------------------------------------------------------------------
:parse_args_for_tests
set "BUILD_DEBUG=1"
set "BUILD_RELEASE=1"
set "TARGET_ARG=%~1"
if /I "%~1"=="debug" (
  set "BUILD_RELEASE=0"
  set "TARGET_ARG=%~2"
) else if /I "%~1"=="release" (
  set "BUILD_DEBUG=0"
  set "TARGET_ARG=%~2"
)
set "NINJA_TARGET="
if /I "!TARGET_ARG!"=="te" (
  set "NINJA_TARGET=test_all"
) else if /I "!TARGET_ARG!"=="e2e" (
  set "NINJA_TARGET=e2e"
)
set "ERR=0"
set "LAST_LOG="
if "!BUILD_DEBUG!"=="0" (
  echo WARNING: te/e2e runners only use Debug binaries; nothing to run.
  goto :finish
)
if /I "!NINJA_TARGET!"=="e2e" (
  echo Running out\Debug\exe_smoke.exe --require-all
  ".\out\Debug\exe_smoke.exe" --require-all
  set "ERR=!ERRORLEVEL!"
  goto :finish
)
if /I "!NINJA_TARGET!"=="test_all" (
  set "UNIT_ERR=0"
  REM leftover_session_test / leftover_record_test / leftover rhi2d-gdi/gl/dem_stereo
  REM PEs removed with src/legacy; run scenic_* twins instead.
  for %%T in (rhi_test.exe model_test.exe scene_test.exe scene_gpu_test.exe unified_draw_test.exe leftover_mesh_test.exe ogr_text_encoding_test.exe sdbd_client_test.exe sdbd_live_test.exe sde_gdal_test.exe geo_ogr_test.exe proj_test.exe stat_expr_test.exe tin_delaunay_test.exe orthogrid_laplace_test.exe net_test.exe tool_dispatch_test.exe draft_test.exe camera_nav_test.exe content_view_host_test.exe content_feature_attrs_test.exe content_catalog_layers_test.exe content_embed_sample_test.exe land_mask_test.exe views_unittests.exe markup_unittests.exe views_pixel_tests.exe ipc_test.exe render_backend_test.exe tile_test.exe style_test.exe map2d_test.exe map2d_pass_test.exe map_scene_test.exe scene3d_presenter_test.exe dem_raster_test.exe scenic_gdi_map_paint_test.exe scenic_map_carto2d_test.exe scenic_gl_map_paint_test.exe scenic_dem_stereo_test.exe menu_test.exe select_query_test.exe plugin_host_test.exe processing_ops_test.exe) do (
    if exist ".\out\Debug\%%T" (
      echo Running out\Debug\%%T
      ".\out\Debug\%%T"
      if !ERRORLEVEL! NEQ 0 set "UNIT_ERR=!ERRORLEVEL!"
    )
  )
  if exist ".\out\Debug\exe_smoke.exe" (
    echo Running out\Debug\exe_smoke.exe
    ".\out\Debug\exe_smoke.exe"
    set "ERR=!ERRORLEVEL!"
  )
  if !UNIT_ERR! NEQ 0 set "ERR=!UNIT_ERR!"
  goto :finish
)
echo WARNING: SMARTGIS_BUILD_PHASE=tests but target is not te/e2e; nothing to run.
goto :finish

REM ---------------------------------------------------------------------------
REM :build_config <OutDirName> <is_debug true|false>
REM ---------------------------------------------------------------------------
:build_config
set "OUT_NAME=%~1"
set "IS_DEBUG=%~2"
set "OUT_DIR=out\%OUT_NAME%"
set "GN_ARGS=is_debug=%IS_DEBUG% is_build_third_party=false smt_run_vs_env_script=false vs_version=180 msvc_installed=true smt_build_app=!BUILD_APP! smt_build_views=!BUILD_VIEWS! smt_build_render=!BUILD_RENDER!"

REM Scenic present is content-hosted (SMT_*_ENGINE=scenic). Pre-gen hook keeps
REM map_present/scene3d_present wired to //src/scenic:scenic (idempotent).
if exist "%LocalAppData%\Programs\Python\Python312\python.exe" (
  "%LocalAppData%\Programs\Python\Python312\python.exe" "%~dp0build\tools\action\apply_scenic_exclusion.py"
) else (
  py -3 "%~dp0build\tools\action\apply_scenic_exclusion.py"
)
if errorlevel 1 (
  echo ERROR: apply_scenic_exclusion.py failed
  exit /b 1
)

echo === gn gen %OUT_DIR% ^(is_debug=%IS_DEBUG%^) ===
"%GN_PATH%gn.exe" gen "%OUT_DIR%" --root=./ --ide=vs2019 --args="!GN_ARGS!"
if errorlevel 1 exit /b 1

echo === ninja -C %OUT_DIR% !NINJA_TARGET! ===
if defined NINJA_TARGET (
  ninja -j 16 -C "./%OUT_DIR%" !NINJA_TARGET! > "./%OUT_DIR%/build.log"
) else (
  ninja -j 16 -C "./%OUT_DIR%" > "./%OUT_DIR%/build.log"
)
set "CFG_ERR=!ERRORLEVEL!"
set "LAST_LOG=%~dp0%OUT_DIR%\build.log"
if !CFG_ERR! NEQ 0 (
  echo ERROR: ninja failed for %OUT_DIR% ^(exit !CFG_ERR!^). See !LAST_LOG!
  exit /b !CFG_ERR!
)
exit /b 0
