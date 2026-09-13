# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Sync Shining Light OpenSSL Dev (Win64) into third_party/.install for GN.
# Install once: winget install --id ShiningLight.OpenSSL.Dev -e
# Then: powershell -File third_party/tools/sync_openssl_win.ps1

param(
  [string]$OpenSslRoot = "C:\Program Files\OpenSSL-Win64",
  [string]$InstallPrefix = ""
)

$ErrorActionPreference = "Stop"
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
if (-not $InstallPrefix) {
  $InstallPrefix = Join-Path $repo "third_party\.install"
}
if (-not (Test-Path (Join-Path $OpenSslRoot "include\openssl\ssl.h"))) {
  Write-Error "OpenSSL headers not found under $OpenSslRoot. Install ShiningLight.OpenSSL.Dev first."
}

New-Item -ItemType Directory -Force -Path `
  (Join-Path $InstallPrefix "include"), `
  (Join-Path $InstallPrefix "lib"), `
  (Join-Path $InstallPrefix "bin") | Out-Null

& cmd /c "xcopy /E /I /Y `"$OpenSslRoot\include\openssl`" `"$InstallPrefix\include\openssl`" >nul"
Copy-Item "$OpenSslRoot\lib\VC\x64\MD\libssl.lib" "$InstallPrefix\lib\libssl.lib" -Force
Copy-Item "$OpenSslRoot\lib\VC\x64\MD\libcrypto.lib" "$InstallPrefix\lib\libcrypto.lib" -Force
Copy-Item "$OpenSslRoot\lib\VC\x64\MDd\libssl.lib" "$InstallPrefix\lib\libssld.lib" -Force
Copy-Item "$OpenSslRoot\lib\VC\x64\MDd\libcrypto.lib" "$InstallPrefix\lib\libcryptod.lib" -Force
Copy-Item "$OpenSslRoot\bin\libssl-4-x64.dll" "$InstallPrefix\bin\" -Force
Copy-Item "$OpenSslRoot\bin\libcrypto-4-x64.dll" "$InstallPrefix\bin\" -Force
Write-Host "[sync_openssl_win] installed headers+libs+dlls into $InstallPrefix"
