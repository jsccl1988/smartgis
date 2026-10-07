# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Launch a PE without a debugger, wait TimeoutSec, and if it is still alive
# treat it as a hang: dump_hang.ps1 + hang analyze. If it exits early, report
# exit code (caller may switch to run_and_catch for crash).
# Defaults: PdbDir=out, OutDir=out/crash.

[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [string]$ExePath,

  [string]$ExeArgs = '',

  [string]$PdbDir = 'out',

  [string]$OutDir = 'out/crash',

  [int]$TimeoutSec = 180,

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

$ExePath = $ExePath.Trim()
if (-not [System.IO.Path]::IsPathRooted($ExePath)) {
  $ExePath = Join-Path $RepoRoot $ExePath
}
$ExePath = [System.IO.Path]::GetFullPath($ExePath)
if (-not (Test-Path -LiteralPath $ExePath -PathType Leaf)) {
  throw "Exe not found: $ExePath"
}

$exeDir = Split-Path -Parent $ExePath
$argList = @()
if (-not [string]::IsNullOrWhiteSpace($ExeArgs)) {
  $argList = @($ExeArgs -split '\s+' | Where-Object { $_ })
}

Write-Host "exe: $ExePath $ExeArgs"
Write-Host "timeout: ${TimeoutSec}s (hang if still alive)"

$proc = if ($argList.Count -gt 0) {
  Start-Process -FilePath $ExePath `
    -ArgumentList $argList `
    -WorkingDirectory $exeDir `
    -PassThru `
    -NoNewWindow
} else {
  Start-Process -FilePath $ExePath `
    -WorkingDirectory $exeDir `
    -PassThru `
    -NoNewWindow
}

$finished = $proc.WaitForExit($TimeoutSec * 1000)
if ($finished) {
  Write-Warning @"
Process exited before hang timeout (exit=$($proc.ExitCode)).
Not a live hang. If this was a crash, use run_and_catch.ps1 or analyze an existing .dmp.
"@
  Write-Output "exited:$($proc.ExitCode)"
  exit 2
}

Write-Host "still alive after ${TimeoutSec}s — dumping hang (PID $($proc.Id))"
$dumpHang = Join-Path $scriptDir 'dump_hang.ps1'
$dumpParams = @{
  ProcessId = $proc.Id
  PdbDir    = $PdbDir
  OutDir    = $OutDir
  RepoRoot  = $RepoRoot
}
if (-not [string]::IsNullOrWhiteSpace($CdbPath)) {
  $dumpParams.CdbPath = $CdbPath
}

& $dumpHang @dumpParams
$dumpExit = $LASTEXITCODE

# Leave the hung process running unless analyze failed hard — agent decides
# whether to kill after evidence is collected (agent-terminal-kill).
Write-Host "target still running: PID $($proc.Id) ($($proc.ProcessName))"
exit $dumpExit
