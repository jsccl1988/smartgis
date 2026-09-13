# Copyright (c) 2026 The Mogu Authors.
# Pass 2: remaining snake_case in base/sys (exclude vendored xml.h TiXml).

$ErrorActionPreference = 'Stop'
$root = 'C:/Dev/src/gis/smartgis/src'

$snake = @{
    'GetStyle' = 'get_style'
    'GetPath' = 'get_path'
    'GetPluginVersion' = 'get_plugin_version'
    'StartPlugin' = 'start_plugin'
    'StopPlugin' = 'stop_plugin'
    'UnLoad' = 'unload'
    'LockExclusive' = 'lock_exclusive'
    'UnlockExclusive' = 'unlock_exclusive'
    'LockShared' = 'lock_shared'
    'UnlockShared' = 'unlock_shared'
    'Update' = 'update'
    'SetClock' = 'set_clock'
    'GetClock' = 'get_clock'
    'SetScale' = 'set_scale'
    'GetScale' = 'get_scale'
    'GetTimeStamp' = 'get_time_stamp'
    'GetElapsed' = 'get_elapsed'
    'GetFPS' = 'get_fps'
    'Start' = 'start'
    'Suspend' = 'suspend'
    'Resume' = 'resume'
    'Kill' = 'kill'
    'Proxy' = 'proxy'
    'Install' = 'install'
    'Uninstall' = 'uninstall'
    'Restart' = 'restart'
    'Init' = 'init'
    'Handle' = 'handle'
    'Wait' = 'wait'
    'Load' = 'load'
    'Run' = 'run'
    'New' = 'new_'
    'Delete' = 'delete_'
    'Stop' = 'stop'
    'Clear' = 'clear'
    'Lock' = 'lock'
    'Unlock' = 'unlock'
    'Signal' = 'signal'
    'Broadcast' = 'broadcast'
    'TimedWait' = 'timed_wait'
    'TryAcquire' = 'try_acquire'
    'Release' = 'release'
    'Acquire' = 'acquire'
}

$exclude = @('base/core/xml.h', 'base/core/xmlparser.cpp', 'base/core/xmlerror.cpp', 'base/core/xml.cpp')

function Apply-Renames($files, [hashtable]$map) {
    $sorted = $map.Keys | Sort-Object { $_.Length } -Descending
    foreach ($f in $files) {
        $rel = $f.FullName.Replace('\', '/').Split('src/')[1]
        if ($exclude -contains $rel) { continue }
        $text = [IO.File]::ReadAllText($f.FullName)
        $orig = $text
        foreach ($k in $sorted) {
            $v = [regex]::Escape($k)
            $text = [regex]::Replace($text, "\b$v\b", $map[$k])
        }
        if ($text -ne $orig) {
            [IO.File]::WriteAllText($f.FullName, $text)
            Write-Host "updated $rel"
        }
    }
}

foreach ($tree in @('base', 'sys')) {
    $files = Get-ChildItem -Path (Join-Path $root $tree) -Recurse -File |
        Where-Object { $_.Extension -match '^\.(h|hpp|c|cc|cpp)$' }
    Apply-Renames $files $snake
}

Write-Host 'pass2 done'
