# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# Optional OpenCppCoverage for Views toolkit unit tests (views_unittests).
# Prefers OpenCppCoverage.exe on PATH. Missing tool → exit 0 skip (does not
# block build.bat te / CI). HTML + cobertura under out/Debug/coverage/views/.

[CmdletBinding()]
param(
  [ValidateSet('Debug', 'Release')]
  [string]$Config = 'Debug',
  [string]$OutDir = ''
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

$CovDir = Join-Path $OutDir 'coverage\views'
$Sources = @(
  'src\ui\views'
)

$occ = Get-Command 'OpenCppCoverage.exe' -ErrorAction SilentlyContinue
if (-not $occ) {
  Write-Host 'SKIP  OpenCppCoverage.exe not on PATH; views coverage optional (does not block te).'
  exit 0
}

$exe = Join-Path $OutDir 'views_unittests.exe'
if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host "SKIP  views_unittests.exe missing under $OutDir"
  exit 0
}

New-Item -ItemType Directory -Force -Path $CovDir | Out-Null

$covBinary = Join-Path $CovDir 'views.cov'

Push-Location $OutDir
try {
  $occArgs = New-Object System.Collections.Generic.List[string]
  foreach ($src in $Sources) {
    $occArgs.Add("--sources=$src")
  }
  $occArgs.Add("--export_type=binary:$covBinary")
  $occArgs.Add("--working_dir=$OutDir")
  $occArgs.Add('--')
  $occArgs.Add($exe)

  & OpenCppCoverage.exe @occArgs
  $exitCode = $LASTEXITCODE
  if ($exitCode -ne 0) {
    Write-Host "FAIL  views_unittests exit=$exitCode"
    exit 1
  }
  Write-Host 'PASS  views_unittests'

  if (Test-Path -LiteralPath $covBinary) {
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
  }
} finally {
  Pop-Location
}

exit 0
