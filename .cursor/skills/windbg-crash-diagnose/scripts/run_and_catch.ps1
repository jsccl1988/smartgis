# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Launch a PE under cdb (-g -G), write a full-memory dump on second-chance
# exception, then run the same analyze sequence as analyze_dump.ps1.
# Defaults: PdbDir=out, OutDir=out/crash, Microsoft public symbols.

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$ExePath,

  [string]$ExeArgs = '',

  [string]$PdbDir = 'out',

  [string]$OutDir = 'out/crash',

  [int]$TimeoutSec = 120,

  [string]$CdbPath,

  [string]$RepoRoot
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$skillRoot = Split-Path -Parent $scriptDir
if (-not $RepoRoot) {
  $RepoRoot = (Resolve-Path (Join-Path $skillRoot '..\..\..')).Path
}

Set-Location -LiteralPath $RepoRoot

if (-not $CdbPath) {
  $findScript = Join-Path $scriptDir 'find_cdb.ps1'
  $CdbPath = & $findScript
  if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($CdbPath)) {
    throw 'cdb.exe not found. Run find_cdb.ps1 or install Debugging Tools for Windows.'
  }
}

$ExePath = $ExePath.Trim()
if (-not [System.IO.Path]::IsPathRooted($ExePath)) {
  $ExePath = Join-Path $RepoRoot $ExePath
}
$ExePath = [System.IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
  throw "Exe not found: $ExePath"
}

if (-not [System.IO.Path]::IsPathRooted($PdbDir)) {
  $PdbDir = Join-Path $RepoRoot $PdbDir
}
$PdbDir = [System.IO.Path]::GetFullPath($PdbDir)

if (-not [System.IO.Path]::IsPathRooted($OutDir)) {
  $OutDir = Join-Path $RepoRoot $OutDir
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$dumpPath = Join-Path $OutDir "$stamp-catch.dmp"
$catchLog = Join-Path $OutDir "$stamp-catch.log"

$pdbForCdb = $PdbDir -replace '\\', '\\'
$dumpForCdb = $dumpPath -replace '\\', '\\'
$publicSym = 'SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols'

# On second-chance exception: dump then quit. Analyze runs separately.
$catchCommands = @(
  ".sympath+ $pdbForCdb"
  ".sympath+ $publicSym"
  '.reload'
  "sxe -c `".dump /ma $dumpForCdb; q`" *"
) -join '; '

$argList = New-Object System.Collections.Generic.List[string]
$argList.Add('-g')
$argList.Add('-G')
$argList.Add('-o')
$argList.Add('-logo')
$argList.Add($catchLog)
$argList.Add('-c')
$argList.Add($catchCommands)
$argList.Add($ExePath)
if (-not [string]::IsNullOrWhiteSpace($ExeArgs)) {
  foreach ($a in ($ExeArgs -split '\s+' | Where-Object { $_ })) {
    $argList.Add($a)
  }
}

Write-Host "cdb: $CdbPath"
Write-Host "exe: $ExePath $ExeArgs"
Write-Host "dump target: $dumpPath"
Write-Host "catch log: $catchLog"
Write-Host "timeout: ${TimeoutSec}s"

$proc = Start-Process -FilePath $CdbPath `
  -ArgumentList $argList.ToArray() `
  -WorkingDirectory $RepoRoot `
  -PassThru `
  -NoNewWindow

$exited = $proc.WaitForExit($TimeoutSec * 1000)
if (-not $exited) {
  Write-Warning "cdb timed out after ${TimeoutSec}s; stopping process tree."
  # Prefer gentle stop; force only if still alive (agent-terminal-kill).
  try { Stop-Process -Id $proc.Id -ErrorAction SilentlyContinue } catch {}
  Start-Sleep -Seconds 2
  if (-not $proc.HasExited) {
    try { Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue } catch {}
  }
  throw "run_and_catch timed out; no reliable dump. catch log: $catchLog"
}

if (-not (Test-Path -LiteralPath $dumpPath -PathType Leaf)) {
  throw @"
No dump written at $dumpPath (process may have exited without a second-chance exception).
Inspect catch log: $catchLog
"@
}

Write-Host "dump written: $dumpPath"

$analyzeScript = Join-Path $scriptDir 'analyze_dump.ps1'
& $analyzeScript -DumpPath $dumpPath -PdbDir $PdbDir -OutDir $OutDir -CdbPath $CdbPath -RepoRoot $RepoRoot
$analyzeExit = $LASTEXITCODE

Write-Output $dumpPath
exit $analyzeExit
