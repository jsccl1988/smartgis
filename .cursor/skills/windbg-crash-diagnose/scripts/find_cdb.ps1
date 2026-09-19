# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Locate cdb.exe (Debugging Tools for Windows). Prefers cdb over windbg GUI.
# Prints the absolute path to stdout on success; exits 1 if not found.

[CmdletBinding()]
param(
  [switch]$Quiet
)

$ErrorActionPreference = 'Stop'

function Test-CdbPath {
  param([string]$Path)
  if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
  return (Test-Path -LiteralPath $Path -PathType Leaf)
}

$candidates = @(
  "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\cdb.exe"
  "$env:ProgramFiles\Windows Kits\10\Debuggers\x64\cdb.exe"
)

$kitRoots = @(
  "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers"
  "$env:ProgramFiles\Windows Kits\10\Debuggers"
)

foreach ($root in $kitRoots) {
  if (-not (Test-Path -LiteralPath $root)) { continue }
  Get-ChildItem -LiteralPath $root -Directory -ErrorAction SilentlyContinue |
    Sort-Object Name -Descending |
    ForEach-Object {
      $candidates += (Join-Path $_.FullName 'x64\cdb.exe')
    }
}

$onPath = Get-Command cdb.exe -ErrorAction SilentlyContinue
if ($onPath) {
  $candidates += $onPath.Source
}

$seen = @{}
foreach ($c in $candidates) {
  if (-not $c) { continue }
  $full = [System.IO.Path]::GetFullPath($c)
  if ($seen.ContainsKey($full)) { continue }
  $seen[$full] = $true
  if (Test-CdbPath $full) {
    if (-not $Quiet) {
      Write-Output $full
    }
    exit 0
  }
}

if (-not $Quiet) {
  Write-Error @"
cdb.exe not found. Install Debugging Tools for Windows (Windows SDK), then retry.
Expected under Windows Kits\10\Debuggers\x64\cdb.exe or on PATH.
"@
}
exit 1
