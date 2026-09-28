# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# Optional OpenCppCoverage for Debug Console / DebugAgent sources.
# Prefers OpenCppCoverage.exe on PATH. Missing tool → exit 0 skip (does not
# block build.bat te / CI). HTML + cobertura under out/Debug/coverage/console/.

[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string]$Config = 'Debug',
  [string]$OutDir = '',
  [switch]$IncludeDebugAgent
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Continue'

$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
if (-not $OutDir) {
  $OutDir = Join-Path $RepoRoot "out\$Config"
} elseif (-not [System.IO.Path]::IsPathRooted($OutDir)) {
  $OutDir = Join-Path $RepoRoot $OutDir
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)

$CovDir = Join-Path $OutDir 'coverage\console'
$Sources = @(
  'src\content\browser\debug'
  'src\base\log'
)

$occ = Get-Command 'OpenCppCoverage.exe' -ErrorAction SilentlyContinue
if (-not $occ) {
  Write-Host 'SKIP  OpenCppCoverage.exe not on PATH; console coverage optional (does not block te).'
  exit 0
}

New-Item -ItemType Directory -Force -Path $CovDir | Out-Null

$exes = @('content_console_coverage_test')
if ($IncludeDebugAgent) {
  $exes += 'debug_agent_test'
} else {
  # Run debug_agent_test when present without forcing the switch.
  $optional = Join-Path $OutDir 'debug_agent_test.exe'
  if (Test-Path -LiteralPath $optional) {
    $exes += 'debug_agent_test'
  }
}

$covBinary = Join-Path $CovDir 'console.cov'
$ran = $false
$failed = 0
$skipped = 0

Push-Location $OutDir
try {
  foreach ($name in $exes) {
    $exe = Join-Path $OutDir ($name + '.exe')
    if (-not (Test-Path -LiteralPath $exe)) {
      ++$skipped
      Write-Host "SKIP  $name (missing under $OutDir)"
      continue
    }

    $occArgs = New-Object System.Collections.Generic.List[string]
    foreach ($src in $Sources) {
      $occArgs.Add("--sources=$src")
    }
    $occArgs.Add("--export_type=binary:$covBinary")
    $occArgs.Add("--working_dir=$OutDir")
    if ($ran -and (Test-Path -LiteralPath $covBinary)) {
      $occArgs.Add("--input_coverage=$covBinary")
    }
    $occArgs.Add('--')
    $occArgs.Add($exe)

    & OpenCppCoverage.exe @occArgs
    $exitCode = $LASTEXITCODE
    $ran = $true
    if ($exitCode -ne 0) {
      ++$failed
      Write-Host "FAIL  $name exit=$exitCode"
    } else {
      Write-Host "PASS  $name"
    }
  }

  if ($ran -and (Test-Path -LiteralPath $covBinary)) {
    $exportArgs = New-Object System.Collections.Generic.List[string]
    foreach ($src in $Sources) {
      $exportArgs.Add("--sources=$src")
    }
    $exportArgs.Add("--export_type=html:$CovDir")
    $exportArgs.Add("--export_type=cobertura:$CovDir\cobertura.xml")
    $exportArgs.Add("--input_coverage=$covBinary")
    $exportArgs.Add('--')
    $exportArgs.Add('cmd')
    $exportArgs.Add('/c')
    $exportArgs.Add('exit 0')
    & OpenCppCoverage.exe @exportArgs | Out-Null
    Write-Host "HTML + cobertura: $CovDir"
  } elseif (-not $ran) {
    Write-Host "SKIP  no console coverage test exes found under $OutDir"
  }
} finally {
  Pop-Location
}

if ($failed -gt 0) {
  exit 1
}
exit 0
