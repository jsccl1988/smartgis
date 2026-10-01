@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
REM Install in-repo SmartGIS UI Markup preview into Cursor / VS Code.
setlocal EnableExtensions EnableDelayedExpansion
set "EXT_SRC=%~dp0vscode-ui-markup"
set "EXT_ID=mogu.ui-markup-0.1.0"
if not exist "%EXT_SRC%\package.json" (
  echo missing %EXT_SRC%\package.json
  exit /b 1
)

set INSTALLED=0

if exist "%USERPROFILE%\.cursor\extensions\" (
  set "DEST=%USERPROFILE%\.cursor\extensions\%EXT_ID%"
  echo Installing to !DEST!
  if exist "!DEST!" rmdir /S /Q "!DEST!"
  mkdir "!DEST!" 2>nul
  robocopy "%EXT_SRC%" "!DEST!" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
  if errorlevel 8 (
    echo Failed copying into Cursor extensions
    exit /b 1
  )
  set INSTALLED=1
  echo OK Cursor: !DEST!
)

if exist "%USERPROFILE%\.vscode\extensions\" (
  set "DEST=%USERPROFILE%\.vscode\extensions\%EXT_ID%"
  echo Installing to !DEST!
  if exist "!DEST!" rmdir /S /Q "!DEST!"
  mkdir "!DEST!" 2>nul
  robocopy "%EXT_SRC%" "!DEST!" /E /NFL /NDL /NJH /NJS /nc /ns /np >nul
  if errorlevel 8 (
    echo Failed copying into VS Code extensions
    exit /b 1
  )
  set INSTALLED=1
  echo OK VS Code: !DEST!
)

if "!INSTALLED!"=="0" (
  echo Neither %%USERPROFILE%%\.cursor\extensions nor .vscode\extensions exists.
  echo Create one by launching Cursor/VS Code once, then re-run this script.
  exit /b 1
)

echo.
echo Reload the window: Command Palette -^> Developer: Reload Window
echo Open a *.ui.xml ^(e.g. src\plugin\product\flood\resources\inundate.ui.xml^) for Webview preview.
echo Command: UI Markup: Open in UiDesigner ^(needs out\Debug\UiDesigner.exe^)
exit /b 0
