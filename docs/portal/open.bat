@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
REM Launch the local docs portal (MD preview + HTML pages under docs/).
setlocal
cd /d "%~dp0"

where py >nul 2>&1
if not errorlevel 1 (
  py -3 "%~dp0serve.py" %*
  goto :done
)
where python >nul 2>&1
if not errorlevel 1 (
  python "%~dp0serve.py" %*
  goto :done
)

echo Failed to start docs portal. Install Python 3 or ensure "py" / "python" is on PATH.
exit /b 1

:done
if errorlevel 1 (
  echo.
  echo Docs portal exited with error %ERRORLEVEL%.
  exit /b %ERRORLEVEL%
)
endlocal
