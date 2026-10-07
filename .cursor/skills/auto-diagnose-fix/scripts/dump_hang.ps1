# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Attach to a hung process, write a full-memory dump, then run hang analysis
# (analyze_dump.ps1 -Mode Hang). Prefer this over killing the PE first.
# Defaults: PdbDir=out, OutDir=out/crash, Microsoft public symbols.

[CmdletBinding()]
param(
  [Parameter(ParameterSetName = 'ById', Mandatory = $true)]
  [int]$ProcessId,

  [Parameter(ParameterSetName = 'ByName', Mandatory = $true)]
  [string]$ProcessName,

  [string]$PdbDir = 'out',

  [string]$OutDir = 'out/crash',

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

if ($PSCmdlet.ParameterSetName -eq 'ByName') {
  $name = $ProcessName.Trim()
  if ($name.EndsWith('.exe', [System.StringComparison]::OrdinalIgnoreCase)) {
    $name = $name.Substring(0, $name.Length - 4)
  }
  $matches = @(Get-Process -Name $name -ErrorAction SilentlyContinue |
    Sort-Object StartTime -Descending)
  if ($matches.Count -eq 0) {
    throw "No process named '$ProcessName' is running."
  }
  $ProcessId = $matches[0].Id
  Write-Host "resolved ProcessName=$ProcessName -> PID $ProcessId ($($matches.Count) match(es))"
}

$proc = Get-Process -Id $ProcessId -ErrorAction SilentlyContinue
if (-not $proc) {
  throw "ProcessId $ProcessId is not running."
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
$dumpPath = Join-Path $OutDir "$stamp-hang.dmp"
$attachLog = Join-Path $OutDir "$stamp-hang-attach.log"
$cmdFile = Join-Path $OutDir "$stamp-hang-dump-cdb.txt"
$publicSym = 'SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols'

# Dump only while attached; hang !analyze runs offline so we do not hold the
# frozen process under the debugger longer than needed.
@(
  ".sympath+ $PdbDir"
  ".sympath+ $publicSym"
  '.reload'
  ".dump /ma $dumpPath"
  'q'
) | Set-Content -LiteralPath $cmdFile -Encoding ascii

function Quote-CdbArg([string]$s) {
  if ($s -notmatch '[\s"]') {
    return $s
  }
  return '"' + ($s -replace '"', '\"') + '"'
}

$argList = @(
  '-p', "$ProcessId"
  '-logo', $attachLog
  '-c', "`$`$<$cmdFile"
)
$argString = ($argList | ForEach-Object { Quote-CdbArg $_ }) -join ' '

Write-Host "cdb: $CdbPath"
Write-Host "attach PID: $ProcessId ($($proc.ProcessName))"
Write-Host "dump target: $dumpPath"
Write-Host "attach log: $attachLog"

$cdbProc = Start-Process -FilePath $CdbPath `
  -ArgumentList $argString `
  -WorkingDirectory $RepoRoot `
  -PassThru `
  -NoNewWindow

$exited = $cdbProc.WaitForExit(180 * 1000)
if (-not $exited) {
  Write-Warning 'cdb attach/dump timed out after 180s; stopping cdb.'
  try { Stop-Process -Id $cdbProc.Id -ErrorAction SilentlyContinue } catch {}
  Start-Sleep -Seconds 2
  if (-not $cdbProc.HasExited) {
    try { Stop-Process -Id $cdbProc.Id -Force -ErrorAction SilentlyContinue } catch {}
  }
  throw "dump_hang timed out attaching to PID $ProcessId. attach log: $attachLog"
}

if (-not (Test-Path -LiteralPath $dumpPath -PathType Leaf)) {
  throw @"
No hang dump written at $dumpPath (attach may have been denied).
Inspect attach log: $attachLog
"@
}

Write-Host "dump written: $dumpPath"

$analyzeScript = Join-Path $scriptDir 'analyze_dump.ps1'
& $analyzeScript -DumpPath $dumpPath -Mode Hang -PdbDir $PdbDir -OutDir $OutDir -CdbPath $CdbPath -RepoRoot $RepoRoot
$analyzeExit = $LASTEXITCODE

Write-Output $dumpPath
exit $analyzeExit
