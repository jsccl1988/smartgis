@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
REM Windows-friendly hook entry: always run from this script's directory.
setlocal
cd /d "%~dp0\..\.."
py -3 "%~dp0build_lock_gate.py"
exit /b %ERRORLEVEL%
