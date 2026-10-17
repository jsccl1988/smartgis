# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# Run DebugAgent done-bar gates from the smartgis repo root.
# Usage: pwsh -NoProfile -File .cursor/skills/auto-agent-fix/scripts/run_agent_gate.ps1
# Exit 0 only when debug_agent_test, content_console_coverage_test, and pytest all pass.

$ErrorActionPreference = "Stop"

function Find-RepoRoot {
  $dir = $PSScriptRoot
  while ($dir) {
    if (Test-Path (Join-Path $dir "build.bat")) {
      return $dir
    }
    $parent = Split-Path -Parent $dir
    if ($parent -eq $dir) {
      break
    }
    $dir = $parent
  }
  throw "build.bat not found above $PSScriptRoot"
}

$root = Find-RepoRoot
Set-Location $root
Write-Host "== auto-agent-fix gate (root=$root) =="

function Invoke-Step {
  param(
    [string]$Name,
    [scriptblock]$Action
  )
  Write-Host ""
  Write-Host "-- $Name --"
  & $Action
  if ($LASTEXITCODE -ne 0) {
    Write-Error "FAIL: $Name (exit $LASTEXITCODE)"
    exit $LASTEXITCODE
  }
  Write-Host "OK: $Name"
}

Invoke-Step "build.bat debug debug_agent_test" {
  cmd /c "build.bat debug debug_agent_test"
}

Invoke-Step "build.bat debug content_console_coverage_test" {
  cmd /c "build.bat debug content_console_coverage_test"
}

$env:PYTHONPATH = "tools"
$py = Get-Command py -ErrorAction SilentlyContinue
if ($py) {
  Invoke-Step "pytest tools/debug/tests" {
    & py -3 -m pytest tools\debug\tests -q
  }
} else {
  Invoke-Step "pytest tools/debug/tests" {
    & python -m pytest tools\debug\tests -q
  }
}

Write-Host ""
Write-Host "PASS auto-agent-fix done bar"
exit 0
