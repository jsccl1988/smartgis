# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# Run fixed-list GIS unit-test exes under out/<Config>. When OpenCppCoverage.exe
# is on PATH, wrap runs and export HTML under <OutDir>/coverage/gis; otherwise
# write functional_summary.txt. Exit non-zero if any present test fails.

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

$CovDir = Join-Path $OutDir 'coverage\gis'
$SourcesFilter = 'src\gis'

# Fixed list of gis *_test.exe (skip missing). Keep in sync with src/gis test().
$GisTests = @(
  'datasource_session_test'
  'feature_load_pipeline_test'
  'ogr_text_encoding_test'
  'sde_gdal_test'
  'sdbd_client_test'
  'sdbd_live_test'
  'feature_test'
  'select_query_test'
  'edit_conflict_test'
  'style_test'
  'tile_test'
  'geo_ogr_test'
  'proj_test'
  'stat_expr_test'
  'tin_delaunay_test'
  'geo_grid_laplace_test'
  'model_test'
  'tileset_test'
  'world_test'
  'land_mask_test'
  'dem_raster_test'
  'tessellate_style_test'
  'frame_test'
  'field_store_test'
  'procedural_test'
  'field_ingest_test'
  'cloud_system_test'
  'ocean_system_test'
  'environment_test'
)

function Test-OpenCppCoverage {
  $cmd = Get-Command 'OpenCppCoverage.exe' -ErrorAction SilentlyContinue
  return [bool]$cmd
}

New-Item -ItemType Directory -Force -Path $CovDir | Out-Null

$hasOcc = Test-OpenCppCoverage
$passed = 0
$failed = 0
$skipped = 0
$lines = New-Object System.Collections.Generic.List[string]
$lines.Add("gis_coverage Config=$Config OutDir=$OutDir")
$lines.Add("OpenCppCoverage=$hasOcc")
$lines.Add('')

$covBinary = Join-Path $CovDir 'gis.cov'
$ranWithOcc = $false

Push-Location $OutDir
try {
  foreach ($name in $GisTests) {
    $exe = Join-Path $OutDir ($name + '.exe')
    if (-not (Test-Path -LiteralPath $exe)) {
      ++$skipped
      $lines.Add("SKIP  $name (missing)")
      Write-Host "SKIP  $name (missing)"
      continue
    }

    $exitCode = 0
    if ($hasOcc) {
      $occArgs = @(
        "--sources=$SourcesFilter"
        "--export_type=binary:$covBinary"
        "--working_dir=$OutDir"
      )
      if ($ranWithOcc -and (Test-Path -LiteralPath $covBinary)) {
        $occArgs += "--input_coverage=$covBinary"
      }
      $occArgs += '--'
      $occArgs += $exe
      & OpenCppCoverage.exe @occArgs
      $exitCode = $LASTEXITCODE
      $ranWithOcc = $true
    } else {
      & $exe
      $exitCode = $LASTEXITCODE
    }

    if ($exitCode -eq 0) {
      ++$passed
      $lines.Add("PASS  $name")
      Write-Host "PASS  $name"
    } else {
      ++$failed
      $lines.Add("FAIL  $name exit=$exitCode")
      Write-Host "FAIL  $name exit=$exitCode"
    }
  }

  if ($hasOcc -and $ranWithOcc -and (Test-Path -LiteralPath $covBinary)) {
    $htmlDir = $CovDir
    & OpenCppCoverage.exe `
      "--sources=$SourcesFilter" `
      "--export_type=html:$htmlDir" `
      "--input_coverage=$covBinary" `
      -- cmd /c exit 0 | Out-Null
    $lines.Add('')
    $lines.Add("HTML coverage: $htmlDir")
  }
} finally {
  Pop-Location
}

$lines.Add('')
$lines.Add("passed=$passed failed=$failed skipped=$skipped")
if (-not $hasOcc) {
  $lines.Add('Note: OpenCppCoverage.exe was not found on PATH; ran functional tests only.')
}

$summaryPath = Join-Path $CovDir 'functional_summary.txt'
$lines | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Host ""
Write-Host "Summary: passed=$passed failed=$failed skipped=$skipped"
Write-Host "Wrote $summaryPath"

if ($failed -gt 0) {
  exit 1
}
exit 0
