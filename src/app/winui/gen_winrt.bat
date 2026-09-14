@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
REM Generate C++/WinRT projections into out/winui_winrt (no Python).
setlocal EnableExtensions EnableDelayedExpansion
pushd "%~dp0..\..\.." || exit /b 1

set "WASDK=third_party\windows_app_sdk\Microsoft.WindowsAppSDK.1.7.260224002"
set "WV2="
for /d %%D in (third_party\windows_app_sdk\Microsoft.Web.WebView2.*) do set "WV2=%%D"
set "OUT=out\winui_winrt"

set "CPPWINRT="
if defined WindowsSdkVerBinPath if exist "%WindowsSdkVerBinPath%x64\cppwinrt.exe" set "CPPWINRT=%WindowsSdkVerBinPath%x64\cppwinrt.exe"
if not defined CPPWINRT if exist "C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\cppwinrt.exe" set "CPPWINRT=C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\cppwinrt.exe"
if not defined CPPWINRT (
  echo ERROR: cppwinrt.exe not found.
  echo Expected: C:\Program Files ^(x86^)\Windows Kits\10\bin\10.0.26100.0\x64\cppwinrt.exe
  popd
  exit /b 2
)

if not exist "%WASDK%\include\MddBootstrap.h" (
  echo ERROR: Windows App SDK headers missing.
  echo Expected: %CD%\%WASDK%\include\MddBootstrap.h
  popd
  exit /b 2
)

if not exist "%OUT%" mkdir "%OUT%"

REM Build argv separately; do not wrap via `cmd /c "..."` — cmd strips outer
REM quotes when many -in paths are present, which breaks "Program Files".
set "ARGS="
for %%F in ("%WASDK%\lib\uap10.0\*.winmd") do set ARGS=!ARGS! -in "%%~fF"
for %%F in ("%WASDK%\lib\uap10.0.18362\*.winmd") do set ARGS=!ARGS! -in "%%~fF"
if defined WV2 if exist "%WV2%\lib\Microsoft.Web.WebView2.Core.winmd" set ARGS=!ARGS! -in "%CD%\%WV2%\lib\Microsoft.Web.WebView2.Core.winmd"
set ARGS=!ARGS! -in sdk -out "%CD%\%OUT%"
echo "%CPPWINRT%" !ARGS!
"%CPPWINRT%" !ARGS!
if errorlevel 1 (
  popd
  exit /b 1
)
echo ok> "%OUT%\winrt_projection.stamp"
popd
exit /b 0
