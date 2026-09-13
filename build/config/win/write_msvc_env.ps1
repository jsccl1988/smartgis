# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
# Write ninja-msvc environment file (INCLUDE/LIB/PATH) without Python.

param(
  [string]$OutFile,
  [string]$Arch = "x64"
)

$ErrorActionPreference = "Stop"
$pf86 = ${env:ProgramFiles(x86)}
if (-not $pf86) { $pf86 = "C:\Program Files (x86)" }
$vswhere = Join-Path $pf86 "Microsoft Visual Studio\Installer\vswhere.exe"
$vcvars = $null
if (Test-Path $vswhere) {
  $vsPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
  if ($vsPath) {
    $vcvars = Join-Path $vsPath "VC\Auxiliary\Build\vcvarsall.bat"
  }
}
if (-not $vcvars -or -not (Test-Path $vcvars)) {
  $vcvars = "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat"
}
if (-not (Test-Path $vcvars)) {
  throw "vcvarsall.bat not found"
}

$cmd = "`"$vcvars`" $Arch >nul && set"
$raw = & cmd.exe /c $cmd
$want = @("INCLUDE", "LIB", "LIBPATH", "PATH", "PATHEXT", "SYSTEMROOT", "TEMP", "TMP")
$pairs = @()
foreach ($line in $raw) {
  $eq = $line.IndexOf("=")
  if ($eq -lt 1) { continue }
  $k = $line.Substring(0, $eq).ToUpperInvariant()
  if ($want -contains $k) {
    $pairs += "$k=$($line.Substring($eq + 1))"
  }
}
$dir = Split-Path -Parent $OutFile
if (-not (Test-Path $dir)) {
  New-Item -ItemType Directory -Path $dir | Out-Null
}
$bytes = New-Object System.Collections.Generic.List[byte]
foreach ($p in $pairs) {
  $bytes.AddRange([System.Text.Encoding]::ASCII.GetBytes($p))
  $bytes.Add(0)
}
$bytes.Add(0)
[System.IO.File]::WriteAllBytes($OutFile, $bytes.ToArray())
Write-Host "Wrote $OutFile"
