@echo off
REM Copyright (c) 2026 The Mogu Authors.
REM All rights reserved.
REM Obtain Windows App SDK headers (user-level NuGet extract) and C++/WinRT
REM projections. Hard-stop with the exact missing path if that fails.
setlocal EnableExtensions EnableDelayedExpansion
pushd "%~dp0..\..\.." || exit /b 1

set "WASDK_DIR=third_party\windows_app_sdk\Microsoft.WindowsAppSDK.1.7.260224002"
set "WASDK_HDR=%WASDK_DIR%\include\MddBootstrap.h"
set "WASDK_VER=1.7.260224002"
set "NUPKG=third_party\windows_app_sdk\Microsoft.WindowsAppSDK.%WASDK_VER%.nupkg"
set "URL=https://api.nuget.org/v3-flatcontainer/microsoft.windowsappsdk/%WASDK_VER%/microsoft.windowsappsdk.%WASDK_VER%.nupkg"

if not exist "%WASDK_HDR%" (
  echo Windows App SDK headers missing. Trying user-level NuGet extract...
  if not exist "third_party\windows_app_sdk" mkdir "third_party\windows_app_sdk"
  powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "try { Invoke-WebRequest -Uri '%URL%' -OutFile '%NUPKG%' -UseBasicParsing; Expand-Archive -LiteralPath '%NUPKG%' -DestinationPath '%WASDK_DIR%' -Force } catch { Write-Error $_; exit 1 }"
  if errorlevel 1 (
    echo ERROR: failed to download Microsoft.WindowsAppSDK %WASDK_VER%.
    echo Expected header: %CD%\%WASDK_HDR%
    echo Manual: download the nupkg from nuget.org and unzip into that folder
    echo ^(not third_party/.install^).
    popd
    exit /b 2
  )
)

if not exist "%WASDK_HDR%" (
  echo ERROR: Windows App SDK still missing after NuGet extract.
  echo Expected: %CD%\%WASDK_HDR%
  popd
  exit /b 2
)

if not exist "out" mkdir "out"
copy /Y "%WASDK_DIR%\runtimes\win-x64\native\Microsoft.WindowsAppRuntime.Bootstrap.dll" "out\Microsoft.WindowsAppRuntime.Bootstrap.dll" >nul

if not exist "out\winui_winrt\winrt\Microsoft.UI.Xaml.h" (
  echo Generating C++/WinRT projections into out\winui_winrt ...
  call "%~dp0gen_winrt.bat"
  if errorlevel 1 (
    echo ERROR: cppwinrt projection failed.
    echo Expected output: %CD%\out\winui_winrt\winrt\Microsoft.UI.Xaml.h
    echo Expected tool: C:\Program Files ^(x86^)\Windows Kits\10\bin\10.0.26100.0\x64\cppwinrt.exe
    popd
    exit /b 2
  )
)

if not exist "out\winui_winrt\winrt\Microsoft.UI.Xaml.h" (
  echo ERROR: C++/WinRT headers missing.
  echo Expected: %CD%\out\winui_winrt\winrt\Microsoft.UI.Xaml.h
  popd
  exit /b 2
)

popd
exit /b 0
