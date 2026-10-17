# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Persist console / catch / analyze breadcrumbs under out/crash for the next
# diagnose pass. Extracts product LOG_* lines and AV/hang markers from one or
# more input logs into a stamped digest + updates LATEST.md index.

[CmdletBinding()]
param(
  # Prefer a single string with ';' separators when invoking via -File
  # (PowerShell -File splits arrays across unbound parameters).
  # Example: -InputLog 'out/crash/a.log;out/crash/b.log'
  [Parameter(Mandatory = $true)]
  [string]$InputLog,

  [string]$OutDir = 'out/crash',

  [string]$Label = '',

  [string]$RepoRoot
)

$ErrorActionPreference = 'Stop'

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$skillRoot = Split-Path -Parent $scriptDir
if (-not $RepoRoot -or -not (Test-Path -LiteralPath $RepoRoot -PathType Container)) {
  $RepoRoot = (Resolve-Path (Join-Path $skillRoot '..\..\..')).Path
}
Set-Location -LiteralPath $RepoRoot

$InputLogList = @()
foreach ($part in ($InputLog -split '[;,]')) {
  $t = $part.Trim()
  if ($t) { $InputLogList += $t }
}
if ($InputLogList.Count -eq 0) {
  throw 'archive_console_diag: InputLog empty after split'
}

if (-not [System.IO.Path]::IsPathRooted($OutDir)) {
  $OutDir = Join-Path $RepoRoot $OutDir
}
$OutDir = [System.IO.Path]::GetFullPath($OutDir)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$safeLabel = if ([string]::IsNullOrWhiteSpace($Label)) { 'console' } else {
  ($Label -replace '[^\w\.-]+', '_').Trim('_')
}
$digestPath = Join-Path $OutDir "$stamp-$safeLabel-console-digest.txt"
$indexPath = Join-Path $OutDir 'LATEST.md'

# Product LOG_* + debugger / fault breadcrumbs worth keeping across sessions.
$patterns = @(
  '\[LOG_',
  'LOG_INFO',
  'LOG_ERROR',
  'LOG_WARNING',
  'LOG_FATAL',
  '\[sync_inspectors\]',
  '\[fill_attribute_rows\]',
  'startup:',
  'Access violation',
  'c0000005',
  'STATUS_',
  'EXCEPTION_',
  'FAULTING_',
  'STACK_TEXT',
  'SYMBOL_NAME',
  'FAILURE_BUCKET',
  'Second chance',
  'First chance',
  'fill_attribute_rows',
  'sync_inspectors',
  'LNK1168',
  'check_out_artifacts',
  'ninja: build stopped',
  'FAILED:'
)

$resolved = @()
foreach ($p in $InputLogList) {
  if ([string]::IsNullOrWhiteSpace($p)) { continue }
  $full = $p
  if (-not [System.IO.Path]::IsPathRooted($full)) {
    $full = Join-Path $RepoRoot $p
  }
  if (Test-Path -LiteralPath $full -PathType Leaf) {
    $resolved += (Resolve-Path -LiteralPath $full).Path
  } else {
    Write-Warning "skip missing log: $p"
  }
}

if ($resolved.Count -eq 0) {
  throw 'archive_console_diag: no input logs found'
}

$sb = New-Object System.Text.StringBuilder
[void]$sb.AppendLine("# console diagnose digest")
[void]$sb.AppendLine("stamp: $stamp")
[void]$sb.AppendLine("label: $safeLabel")
[void]$sb.AppendLine("repo: $RepoRoot")
[void]$sb.AppendLine('')

$hitCount = 0
foreach ($path in $resolved) {
  [void]$sb.AppendLine('## source')
  [void]$sb.AppendLine($path)
  [void]$sb.AppendLine('')
  $lines = Get-Content -LiteralPath $path -ErrorAction SilentlyContinue
  if (-not $lines) {
    [void]$sb.AppendLine('(empty)')
    [void]$sb.AppendLine('')
    continue
  }
  $n = 0
  for ($i = 0; $i -lt $lines.Count; $i++) {
    $line = $lines[$i]
    foreach ($pat in $patterns) {
      if ($line -match $pat) {
        $n++
        $hitCount++
        [void]$sb.AppendLine(('{0,6}: {1}' -f ($i + 1), $line))
        break
      }
    }
  }
  if ($n -eq 0) {
    [void]$sb.AppendLine('(no matching console/diagnose lines; keeping last 40)')
    $start = [Math]::Max(0, $lines.Count - 40)
    for ($i = $start; $i -lt $lines.Count; $i++) {
      [void]$sb.AppendLine(('{0,6}: {1}' -f ($i + 1), $lines[$i]))
    }
  }
  [void]$sb.AppendLine('')
}

[void]$sb.AppendLine("## summary")
[void]$sb.AppendLine("matched_lines: $hitCount")
[void]$sb.AppendLine("sources: $($resolved.Count)")

$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($digestPath, $sb.ToString(), $utf8NoBom)

$indexBody = @"
<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Latest crash/hang console digest

Updated: $stamp

| Field | Value |
| --- | --- |
| Digest | ``$([System.IO.Path]::GetFileName($digestPath))`` |
| Label | $safeLabel |
| Matched lines | $hitCount |
| Sources | $($resolved.Count) |

## Source logs

$($resolved | ForEach-Object { '- `' + $_ + '`' } | Out-String)

Open the digest next to ``*-analyze.log`` / ``*-catch.log`` when diagnosing.
"@
[System.IO.File]::WriteAllText($indexPath, $indexBody, $utf8NoBom)

Write-Host "console digest: $digestPath"
Write-Host "index: $indexPath (hits=$hitCount)"
Write-Output $digestPath
