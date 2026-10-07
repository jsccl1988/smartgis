# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Analyze a Windows minidump with cdb. Hard-coded -c strings (do not rewrite).
# -Mode Crash (default): !analyze -v + exception context
# -Mode Hang: !analyze -v -hang + all-thread stacks / runaway / locks
# Defaults: PdbDir=out, OutDir=out/crash, Microsoft public symbols.

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$DumpPath,

  [ValidateSet('Crash', 'Hang')]
  [string]$Mode = 'Crash',

  [string]$PdbDir = 'out',

  [string]$OutDir = 'out/crash',

  [string]$CdbPath,

  [string]$RepoRoot
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$skillRoot = Split-Path -Parent $scriptDir
if (-not $RepoRoot) {
  # scripts/ -> auto-diagnose-fix/ -> skills/ -> .cursor/ -> repo root
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

$DumpPath = $DumpPath.Trim()
if (-not [System.IO.Path]::IsPathRooted($DumpPath)) {
  $DumpPath = Join-Path $RepoRoot $DumpPath
}
$DumpPath = [System.IO.Path]::GetFullPath($DumpPath)
if (-not (Test-Path -LiteralPath $DumpPath -PathType Leaf)) {
  throw "Dump not found: $DumpPath"
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
$suffix = if ($Mode -eq 'Hang') { 'hang' } else { 'analyze' }
$logPath = Join-Path $OutDir "$stamp-$suffix.log"

# Inline `-c "a; b"` is unsafe: `.sympath+ path` swallows `;` as part of the
# path. Use a command file + `$$<` (same pattern as run_and_catch.ps1).
$publicSym = 'SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols'
$cmdFile = Join-Path $OutDir "$stamp-$suffix-cdb.txt"
$cmdLines = @(
  ".sympath+ $PdbDir"
  ".sympath+ $publicSym"
  '.reload'
)
if ($Mode -eq 'Hang') {
  $cmdLines += @(
    '!analyze -v -hang'
    '~*kv'
    '!runaway'
    '!locks'
    'lm vm'
    'q'
  )
} else {
  $cmdLines += @(
    '!analyze -v'
    '.ecxr'
    'kn'
    'kv'
    'lm vm'
    'q'
  )
}
$cmdLines | Set-Content -LiteralPath $cmdFile -Encoding ascii

# Avoid spaces in $$< path (cdb parser); OutDir is out/crash by default.
$cdbArgs = @(
  '-z', $DumpPath
  '-logo', $logPath
  '-c', "`$`$<$cmdFile"
)

Write-Host "cdb: $CdbPath"
Write-Host "mode: $Mode"
Write-Host "dump: $DumpPath"
Write-Host "pdb: $PdbDir"
Write-Host "log: $logPath"
Write-Host "cmdfile: $cmdFile"

& $CdbPath @cdbArgs
$exitCode = $LASTEXITCODE

Write-Host "analyze exit: $exitCode"
Write-Output $logPath
exit $exitCode
