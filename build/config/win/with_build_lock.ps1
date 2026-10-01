# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

# Compile locks for shared gen roots (Debug / Release / shared third_party).
# Fair ticket queue per scope; multi-target args release between targets.
# te / e2e runners run AFTER compile locks are released.
#
# Global on/off (default OFF — no ticket / mutex / multi-target split):
#   SMARTGIS_BUILD_LOCK=1|on|true|yes    → enable for this process
#   SMARTGIS_BUILD_LOCK=0|off|false|no   → force disable (overrides sentinel)
#   out/.build.lock.on                   → enable for all processes (delete to return to default off)
#
# Env:
#   SMARTGIS_BUILD_LOCK           Optional on/off (see above). Default: off unless sentinel/env on.
#   SMARTGIS_BUILD_LOCK_WAIT_SEC  Max seconds to wait (default 1800). 0 = fail now.
#   SMARTGIS_BUILD_OWNER          Label written into lock / waiter files.
#   SMARTGIS_BUILD_LOCK_HELD      Set while re-entering build.bat (skip outer wrap).
#   SMARTGIS_BUILD_PHASE          compile | tests | all — set by this wrapper.

param(
  [Parameter(ValueFromRemainingArguments = $true)]
  [string[]]$BuildArgs
)

$ErrorActionPreference = 'Stop'

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path
$outDir = Join-Path $repoRoot 'out'
$waitersDir = Join-Path $outDir '.build.waiters'
$summaryPath = Join-Path $outDir '.build.lock'
$stampPath = Join-Path $outDir '.build.last_ok'
$buildBat = Join-Path $repoRoot 'build.bat'

$waitSec = 1800
if ($null -ne $env:SMARTGIS_BUILD_LOCK_WAIT_SEC -and $env:SMARTGIS_BUILD_LOCK_WAIT_SEC -ne '') {
  $waitSec = [int]$env:SMARTGIS_BUILD_LOCK_WAIT_SEC
}

$owner = if ($env:SMARTGIS_BUILD_OWNER) { $env:SMARTGIS_BUILD_OWNER } else { 'unknown' }
$lockOnSentinel = Join-Path $outDir '.build.lock.on'

function Test-BuildLockEnabled {
  # Default OFF. Env force-off / force-on wins over the sentinel file.
  $raw = $env:SMARTGIS_BUILD_LOCK
  if ($null -ne $raw -and $raw -ne '') {
    if ($raw -match '^(?i)(0|off|false|no)$') {
      return $false
    }
    if ($raw -match '^(?i)(1|on|true|yes)$') {
      return $true
    }
  }
  if (Test-Path -LiteralPath $lockOnSentinel) {
    return $true
  }
  return $false
}

# Aliases that must not be split into per-target lock cycles.
$script:NoSplitAliases = @(
  'm', 'te', 'a', 'b', 'app', 'views', 'render', 'e2e',
  'legacy_app', 'ui_legacy', 'smartgis', 't', 'sln'
)

function Get-LockPath {
  param([ValidateSet('debug', 'release', 'shared')][string]$Scope)
  return (Join-Path $outDir ('.build.lock.' + $Scope))
}

function Read-LockInfo {
  param([string]$Path)
  try {
    return (Get-Content -LiteralPath $Path -Raw -ErrorAction Stop).TrimEnd()
  } catch {
    try {
      $rs = [System.IO.File]::Open(
        $Path,
        [System.IO.FileMode]::Open,
        [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::ReadWrite)
      try {
        $sr = New-Object System.IO.StreamReader($rs)
        return $sr.ReadToEnd().TrimEnd()
      } finally {
        $rs.Dispose()
      }
    } catch {
      return '(lock held; contents unreadable)'
    }
  }
}

function Try-OpenBuildLock {
  param([string]$Path)
  try {
    return [System.IO.File]::Open(
      $Path,
      [System.IO.FileMode]::OpenOrCreate,
      [System.IO.FileAccess]::ReadWrite,
      [System.IO.FileShare]::Read)
  } catch {
    return $null
  }
}

function Test-NeedsPostCompileTests {
  param([string[]]$ArgsIn)
  if (-not $ArgsIn -or $ArgsIn.Count -eq 0) {
    return $false
  }
  foreach ($a in $ArgsIn) {
    if ($a -match '^(?i)(te|e2e)$') {
      return $true
    }
  }
  return $false
}

function Invoke-BuildBat {
  param(
    [string[]]$ArgsIn,
    [string]$Phase
  )
  $env:SMARTGIS_BUILD_LOCK_HELD = '1'
  $env:SMARTGIS_BUILD_PHASE = $Phase
  # Empty build.bat (both configs): $ArgsIn may be $null; @($null) becomes
  # a one-element null array and Start-Process -ArgumentList rejects it.
  $extra = @()
  if ($null -ne $ArgsIn) {
    $extra = @($ArgsIn | Where-Object { $null -ne $_ -and "$_" -ne '' })
  }
  $argList = @('/d', '/c', 'call', "`"$buildBat`"") + $extra
  $p = Start-Process -FilePath 'cmd.exe' `
    -ArgumentList $argList `
    -WorkingDirectory $repoRoot `
    -Wait -PassThru -NoNewWindow
  return $p.ExitCode
}

function Write-BuildStamp {
  param(
    [string]$Path,
    [string]$Owner,
    [string]$CmdLine,
    [int]$ExitCode,
    [string]$Scope
  )
  $body = @(
    "finished=$([DateTime]::Now.ToString('o'))"
    "owner=$Owner"
    "exit=$ExitCode"
    "scope=$Scope"
    "cmdline=build.bat $CmdLine"
    "phase=compile"
  ) -join "`n"
  Set-Content -LiteralPath $Path -Value ($body + "`n") -Encoding utf8
}

function Write-SummaryLock {
  $lines = New-Object System.Collections.Generic.List[string]
  $lines.Add("updated=$([DateTime]::Now.ToString('o'))")
  foreach ($scope in @('debug', 'release', 'shared')) {
    $lp = Get-LockPath -Scope $scope
    if (Test-Path -LiteralPath $lp) {
      $info = (Read-LockInfo -Path $lp) -replace "`r?`n", ' | '
      $lines.Add("[$scope] $info")
    }
  }
  $waiterCount = 0
  if (Test-Path -LiteralPath $waitersDir) {
    $waiterCount = @(Get-ChildItem -LiteralPath $waitersDir -Filter '*.wait' -ErrorAction SilentlyContinue).Count
  }
  $lines.Add("waiters=$waiterCount")
  Set-Content -LiteralPath $summaryPath -Value (($lines -join "`n") + "`n") -Encoding utf8
}

function Remove-SummaryIfIdle {
  $any = $false
  foreach ($scope in @('debug', 'release', 'shared')) {
    if (Test-Path -LiteralPath (Get-LockPath -Scope $scope)) {
      $any = $true
      break
    }
  }
  if (-not $any) {
    Remove-Item -LiteralPath $summaryPath -Force -ErrorAction SilentlyContinue
  } else {
    Write-SummaryLock
  }
}

function Get-ConflictingScopes {
  param([ValidateSet('debug', 'release', 'shared')][string]$Scope)
  switch ($Scope) {
    'debug' { return @('debug', 'shared') }
    'release' { return @('release', 'shared') }
    'shared' { return @('debug', 'release', 'shared') }
  }
}

function Clear-DeadWaiters {
  if (-not (Test-Path -LiteralPath $waitersDir)) {
    return
  }
  Get-ChildItem -LiteralPath $waitersDir -Filter '*.wait' -ErrorAction SilentlyContinue | ForEach-Object {
    $m = [regex]::Match($_.Name, '_pid(\d+)_')
    if (-not $m.Success) {
      return
    }
    $wpid = [int]$m.Groups[1].Value
    if (-not (Get-Process -Id $wpid -ErrorAction SilentlyContinue)) {
      Remove-Item -LiteralPath $_.FullName -Force -ErrorAction SilentlyContinue
    }
  }
}

function Register-Waiter {
  param(
    [ValidateSet('debug', 'release', 'shared')][string]$Scope,
    [string]$Ticket
  )
  if (-not (Test-Path -LiteralPath $waitersDir)) {
    New-Item -ItemType Directory -Path $waitersDir | Out-Null
  }
  $safeOwner = ($owner -replace '[^\w\-.]+', '_')
  $name = "${Ticket}_pid${PID}_${safeOwner}_${Scope}.wait"
  $path = Join-Path $waitersDir $name
  $body = @(
    "ticket=$Ticket"
    "pid=$PID"
    "owner=$owner"
    "scope=$Scope"
    "started=$([DateTime]::Now.ToString('o'))"
  ) -join "`n"
  Set-Content -LiteralPath $path -Value ($body + "`n") -Encoding utf8
  return $path
}

function Get-WaiterRank {
  param(
    [ValidateSet('debug', 'release', 'shared')][string]$Scope,
    [string]$Ticket
  )
  Clear-DeadWaiters
  $files = @(Get-ChildItem -LiteralPath $waitersDir -Filter "*_${Scope}.wait" -ErrorAction SilentlyContinue |
    Sort-Object Name)
  $rank = 1
  foreach ($f in $files) {
    if ($f.Name.StartsWith($Ticket + '_')) {
      return @{ Rank = $rank; Total = $files.Count }
    }
    $rank++
  }
  return @{ Rank = $files.Count + 1; Total = $files.Count }
}

function Test-IsHeadWaiter {
  param(
    [ValidateSet('debug', 'release', 'shared')][string]$Scope,
    [string]$Ticket
  )
  Clear-DeadWaiters
  $files = @(Get-ChildItem -LiteralPath $waitersDir -Filter "*_${Scope}.wait" -ErrorAction SilentlyContinue |
    Sort-Object Name)
  if ($files.Count -eq 0) {
    return $true
  }
  return $files[0].Name.StartsWith($Ticket + '_')
}

function Acquire-ScopeLock {
  param(
    [ValidateSet('debug', 'release', 'shared')][string]$Scope,
    [string]$Ticket,
    [string]$CmdLine,
    [string]$WaiterPath
  )
  $lockPath = Get-LockPath -Scope $Scope
  $deadline = [DateTime]::UtcNow.AddSeconds([Math]::Max(0, $waitSec))
  $fs = $null

  while ($true) {
    Clear-DeadWaiters
    $isHead = Test-IsHeadWaiter -Scope $Scope -Ticket $Ticket
    if ($isHead) {
      # Also require conflicting scopes free (shared vs debug/release).
      $conflictHeld = $false
      $conflictInfo = @()
      foreach ($c in (Get-ConflictingScopes -Scope $Scope)) {
        if ($c -eq $Scope) {
          continue
        }
        $cp = Get-LockPath -Scope $c
        if (Test-Path -LiteralPath $cp) {
          $probe = Try-OpenBuildLock -Path $cp
          if ($null -eq $probe) {
            $conflictHeld = $true
            $conflictInfo += "--- $c ---`n$(Read-LockInfo -Path $cp)"
          } else {
            $probe.Dispose()
            Remove-Item -LiteralPath $cp -Force -ErrorAction SilentlyContinue
          }
        }
      }
      if (-not $conflictHeld) {
        $fs = Try-OpenBuildLock -Path $lockPath
        if ($null -ne $fs) {
          break
        }
      }
    }

    $infoBlocks = New-Object System.Collections.Generic.List[string]
    foreach ($c in (Get-ConflictingScopes -Scope $Scope)) {
      $cp = Get-LockPath -Scope $c
      if (Test-Path -LiteralPath $cp) {
        $probe = Try-OpenBuildLock -Path $cp
        if ($null -eq $probe) {
          $infoBlocks.Add("--- holder [$c] ---`n$(Read-LockInfo -Path $cp)")
        } else {
          $probe.Dispose()
        }
      }
    }
    $rank = Get-WaiterRank -Scope $Scope -Ticket $Ticket
    $queueLine = "queue[$Scope]: rank $($rank.Rank)/$($rank.Total) ticket=$Ticket owner=$owner"

    if ($waitSec -le 0) {
      Write-Host "ERROR: build busy (compile lock scope=$Scope)."
      Write-Host $queueLine
      foreach ($b in $infoBlocks) { Write-Host $b }
      if ($WaiterPath) {
        Remove-Item -LiteralPath $WaiterPath -Force -ErrorAction SilentlyContinue
      }
      throw "SMARTGIS_BUILD_LOCK_BUSY:$Scope"
    }
    if ([DateTime]::UtcNow -ge $deadline) {
      Write-Host "ERROR: timed out after ${waitSec}s waiting for scope=$Scope."
      Write-Host $queueLine
      foreach ($b in $infoBlocks) { Write-Host $b }
      if ($WaiterPath) {
        Remove-Item -LiteralPath $WaiterPath -Force -ErrorAction SilentlyContinue
      }
      throw "SMARTGIS_BUILD_LOCK_TIMEOUT:$Scope"
    }

    Write-Host "build busy (scope=$Scope); waiting ..."
    Write-Host $queueLine
    foreach ($b in $infoBlocks) { Write-Host $b }
    Start-Sleep -Seconds 2
  }

  $fs.SetLength(0)
  $payload = @(
    "pid=$PID"
    "owner=$owner"
    "scope=$Scope"
    "ticket=$Ticket"
    "started=$([DateTime]::Now.ToString('o'))"
    "phase=compile"
    "cmdline=build.bat $CmdLine"
  ) -join "`n"
  $bytes = [System.Text.Encoding]::UTF8.GetBytes($payload + "`n")
  $fs.Write($bytes, 0, $bytes.Length)
  $fs.Flush()
  Write-SummaryLock
  Write-Host "=== build lock acquired (scope=$Scope pid=$PID owner=$owner ticket=$Ticket) ==="
  return $fs
}

function Release-ScopeLock {
  param(
    $FileStream,
    [ValidateSet('debug', 'release', 'shared')][string]$Scope
  )
  # Only delete the lock file if we actually held it. A failed acquire must
  # not Remove-Item another process's lock (PowerShell still runs caller's finally after exit).
  if ($null -eq $FileStream) {
    return
  }
  $FileStream.Dispose()
  Remove-Item -LiteralPath (Get-LockPath -Scope $Scope) -Force -ErrorAction SilentlyContinue
  Write-Host "=== build lock released (scope=$Scope) ==="
  Remove-SummaryIfIdle
}

function Get-ConfigPrefix {
  param([string[]]$ArgsIn)
  if ($ArgsIn -and $ArgsIn.Count -gt 0) {
    if ($ArgsIn[0] -match '^(?i)debug$') { return 'debug' }
    if ($ArgsIn[0] -match '^(?i)release$') { return 'release' }
  }
  return $null
}

function Get-TargetsAfterConfig {
  param([string[]]$ArgsIn)
  if (-not $ArgsIn -or $ArgsIn.Count -eq 0) {
    return @()
  }
  $i = 0
  if ($ArgsIn[0] -match '^(?i)(debug|release)$') {
    $i = 1
  }
  if ($i -ge $ArgsIn.Count) {
    return @()
  }
  return @($ArgsIn[$i..($ArgsIn.Count - 1)])
}

function Test-IsThirdPartyInstall {
  param([string[]]$ArgsIn)
  $targets = Get-TargetsAfterConfig -ArgsIn $ArgsIn
  if ($targets.Count -ge 1 -and $targets[0] -match '^(?i)t$') {
    return $true
  }
  return $false
}

function Test-ShouldSplitTargets {
  param([string[]]$ArgsIn)
  if (Test-IsThirdPartyInstall -ArgsIn $ArgsIn) {
    return $false
  }
  $targets = Get-TargetsAfterConfig -ArgsIn $ArgsIn
  if ($targets.Count -lt 2) {
    return $false
  }
  foreach ($t in $targets) {
    if ($script:NoSplitAliases -contains $t.ToLowerInvariant()) {
      return $false
    }
  }
  return $true
}

function Get-CompileJobs {
  param([string[]]$ArgsIn)
  # Returns list of @{ Scope=...; Args=string[] }
  $jobs = New-Object System.Collections.Generic.List[object]

  if (Test-IsThirdPartyInstall -ArgsIn $ArgsIn) {
    $jobs.Add([pscustomobject]@{ Scope = 'shared'; Args = @($ArgsIn) })
    return $jobs
  }

  $prefix = Get-ConfigPrefix -ArgsIn $ArgsIn
  $targets = Get-TargetsAfterConfig -ArgsIn $ArgsIn

  if (Test-ShouldSplitTargets -ArgsIn $ArgsIn) {
    foreach ($t in $targets) {
      if ($null -ne $prefix) {
        $jobs.Add([pscustomobject]@{ Scope = $prefix; Args = @($prefix, $t) })
      } else {
        # Bare multi-target: each target on debug then release.
        $jobs.Add([pscustomobject]@{ Scope = 'debug'; Args = @('debug', $t) })
        $jobs.Add([pscustomobject]@{ Scope = 'release'; Args = @('release', $t) })
      }
    }
    return $jobs
  }

  if ($null -ne $prefix) {
    $jobs.Add([pscustomobject]@{ Scope = $prefix; Args = @($ArgsIn) })
    return $jobs
  }

  # No config prefix: sequential debug then release (finer than one shared hold).
  if ($targets.Count -eq 0) {
    $jobs.Add([pscustomobject]@{ Scope = 'debug'; Args = @('debug') })
    $jobs.Add([pscustomobject]@{ Scope = 'release'; Args = @('release') })
  } else {
    $jobs.Add([pscustomobject]@{ Scope = 'debug'; Args = @('debug') + $targets })
    $jobs.Add([pscustomobject]@{ Scope = 'release'; Args = @('release') + $targets })
  }
  return $jobs
}

# --- main ---

if (-not (Test-Path -LiteralPath $outDir)) {
  New-Item -ItemType Directory -Path $outDir | Out-Null
}

$needTests = Test-NeedsPostCompileTests -ArgsIn $BuildArgs
$cmdLine = if ($BuildArgs -and $BuildArgs.Count -gt 0) { ($BuildArgs -join ' ') } else { '(default both configs)' }

# Fast path: lock disabled → one build.bat call (PHASE=all). No tickets / split / mutex.
if (-not (Test-BuildLockEnabled)) {
  $why = if ($env:SMARTGIS_BUILD_LOCK -match '^(?i)(0|off|false|no)$') {
    'SMARTGIS_BUILD_LOCK=' + $env:SMARTGIS_BUILD_LOCK
  } else {
    'default off (set SMARTGIS_BUILD_LOCK=1 or create out\.build.lock.on to enable)'
  }
  Write-Host "=== build lock DISABLED ($why); direct build.bat $cmdLine ==="
  $exitCode = Invoke-BuildBat -ArgsIn $BuildArgs -Phase 'all'
  Write-BuildStamp -Path $stampPath -Owner $owner -CmdLine $cmdLine -ExitCode $exitCode -Scope 'off'
  exit $exitCode
}

if (-not (Test-Path -LiteralPath $waitersDir)) {
  New-Item -ItemType Directory -Path $waitersDir | Out-Null
}

# Ticket sorts lexicographically for FIFO; include PID to avoid same-ms collisions.
$ticket = [DateTime]::UtcNow.ToString('yyyyMMddHHmmssfff') + '_' + $PID

$jobs = @(Get-CompileJobs -ArgsIn $BuildArgs)
Write-Host "=== build lock plan: $($jobs.Count) job(s); ticket=$ticket owner=$owner ==="
foreach ($j in $jobs) {
  Write-Host ("  - [{0}] build.bat {1}" -f $j.Scope, ($j.Args -join ' '))
}

$exitCode = 0
$activeWaiters = @{}
$lockBusy = $false

try {
  for ($ji = 0; $ji -lt $jobs.Count; $ji++) {
    $job = $jobs[$ji]
    $scope = $job.Scope
    $jobArgs = @($job.Args)
    $jobCmd = $jobArgs -join ' '

    if (-not $activeWaiters.ContainsKey($scope)) {
      $activeWaiters[$scope] = Register-Waiter -Scope $scope -Ticket $ticket
    }
    $waiterPath = $activeWaiters[$scope]

    $fs = $null
    try {
      try {
        $fs = Acquire-ScopeLock -Scope $scope -Ticket $ticket -CmdLine $jobCmd -WaiterPath $waiterPath
      } catch {
        if ("$_" -match '^SMARTGIS_BUILD_LOCK_(BUSY|TIMEOUT):') {
          $lockBusy = $true
          if ($activeWaiters.ContainsKey($scope)) {
            $activeWaiters.Remove($scope)
          }
          break
        }
        throw
      }
      $exitCode = Invoke-BuildBat -ArgsIn $jobArgs -Phase 'compile'
      Write-BuildStamp -Path $stampPath -Owner $owner -CmdLine $jobCmd -ExitCode $exitCode -Scope $scope
    } finally {
      Release-ScopeLock -FileStream $fs -Scope $scope
    }

    # Drop waiter when no later job needs this scope (do not block peers while on another config).
    $scopeNeededLater = $false
    for ($k = $ji + 1; $k -lt $jobs.Count; $k++) {
      if ($jobs[$k].Scope -eq $scope) {
        $scopeNeededLater = $true
        break
      }
    }
    if (-not $scopeNeededLater -and $activeWaiters.ContainsKey($scope)) {
      Remove-Item -LiteralPath $activeWaiters[$scope] -Force -ErrorAction SilentlyContinue
      $activeWaiters.Remove($scope)
    }

    if ($exitCode -ne 0) {
      break
    }
  }
} finally {
  foreach ($kv in @($activeWaiters.GetEnumerator())) {
    Remove-Item -LiteralPath $kv.Value -Force -ErrorAction SilentlyContinue
  }
  Clear-DeadWaiters
  Remove-SummaryIfIdle
}

if ($lockBusy) {
  exit 3
}

if ($exitCode -ne 0) {
  exit $exitCode
}

if ($needTests) {
  Write-Host "=== post-compile tests (unlocked; te/e2e) ==="
  $testRc = Invoke-BuildBat -ArgsIn $BuildArgs -Phase 'tests'
  exit $testRc
}

exit 0
