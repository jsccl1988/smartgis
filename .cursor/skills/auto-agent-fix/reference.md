<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# auto-agent-fix — reference

Read from `SKILL.md` when you need cmd catalogs, connect recipes, or VS Code DAP notes.

## Layout (who owns what)

| Path | Role |
| --- | --- |
| `src/content/browser/debug/debug_agent.*` | Facade: listen, dispatch, host hooks, record ring |
| `src/content/browser/debug/cmd/*` | Domain handlers (`agent_log`, `agent_ui`, `agent_harness`, …) |
| `src/content/browser/debug/schema/agent_schema.*` | `:help`, `rpc.methods` JSON, Tab prefixes |
| `src/content/browser/debug/policy/agent_policy.*` | Dangerous-op gate |
| `src/content/browser/debug/wire/agent_json.*` | NDJSON helpers |
| `tools/debug/worker.py` | OOP Python worker (`python -m debug.worker`) |
| `tools/debug/smartgis/*` | `AgentClient`, discovery, `cmd_exec` / `log_tail` / `ui_*` |
| `tools/debug/agent_methods.json` | Editor catalog (keep in sync with schema) |
| `tools/debug/vscode/launch.json` | Sample **debugpy attach** (port 5678) |
| `tools/debug/vscode-ext/` | VS Code / Cursor VSIX (catalog / console / SDK chat) |
| `tools/debug/mcp/` | Stdio MCP (`python -m debug.mcp`) for Cursor SDK tools |
| `tools/debug/console_prefixes.json` | `:cmd` catalog (sync with `agent_schema.cc`) |
| `tools/debug/tests/` | pytest (mock agent; no live product required) |

## Hand-written console cmds

Prefixes live in `agent_schema.cc` (`kConsolePrefixes`). Common repro lines:

```
:help
:help json
:clear
:confirm
:log.level debug
:refresh
:extent
:layers
:diag
:diag capture
:ask <text>
:ui find <name>
:ui click <x> <y>
:ui type <text>
:ui tree
:ui overlay
:ui capture [path]
:script <path>
:record on|off|poll|clear
:gis test|bench
:rhi test|bench
:sdbd capabilities
:sdbd sql <text>
:py <code>
:run <path>
```

`:gis test` / `:rhi test` spawn curated `*_test.exe` beside the running PE (see `cmd/agent_harness.cc`). Missing exes are skipped; `fail>0` is a real gate signal when those exes exist.

## NDJSON RPC (loopback)

One JSON object per line. Typical methods (see `agent_methods_schema_json` / `agent_methods.json`):

| Method | Notes |
| --- | --- |
| `ping` | Smoke |
| `rpc.methods` | Schema object |
| `rpc.confirm` | Unlock dangerous ops for session |
| `log.tail` / `log.set_level` / `log.subscribe` | Log ring |
| `cmd.exec` | `params.line` = console cmd including leading `:` |
| `diag.pack` | Bundle for forensics |
| `ui.*` / `py.*` / `sdbd.*` / `record.*` / `script.run` | Domains |
| `shutdown` | Stop agent |

Dangerous (need confirm / `SG_DEBUG_ALLOW=1`): `py.eval`, `py.run_file`, `sdbd.query`, `ui.click`, `ui.capture_shell`.

### Minimal TCP ping (PowerShell sketch)

```powershell
# PORT from %TEMP%\smartgis-debug.json or debug_agent_test stdout
$client = New-Object System.Net.Sockets.TcpClient('127.0.0.1', $PORT)
$stream = $client.GetStream()
$bytes = [Text.Encoding]::UTF8.GetBytes('{"id":1,"method":"ping","params":{}}' + "`n")
$stream.Write($bytes, 0, $bytes.Length)
$buf = New-Object byte[] 4096
$n = $stream.Read($buf, 0, $buf.Length)
[Text.Encoding]::UTF8.GetString($buf, 0, $n)
$client.Close()
```

Prefer `tools/debug` helpers when Python is available:

```python
from smartgis import connect_from_discovery, bind_client, ping, log_tail, cmd_exec
with connect_from_discovery() as client:
    bind_client(client)
    print(ping())
    print(cmd_exec(":help"))
    print(log_tail(100))
```

## Discovery / enablement

- Opt-in: `--debug-console`, `SG_DEBUG=1`, or Views console toggle.
- Discovery file: `%TEMP%\smartgis-debug.json` (written by Agent `start()`).
- Port: ephemeral or `SG_DEBUG_PORT`.

## VS Code / Cursor extension

In-tree VSIX source: `tools/debug/vscode-ext/` (see its README).

1. `cd tools\debug\vscode-ext && npm install` then **Install from Location**.
2. Product Agent listening (`%TEMP%\smartgis-debug.json`).
3. Activity bar **SmartGIS Debug**: Console (P1) / Chat (P2, `@cursor/sdk`).
4. Optional worker + DAP:

```bat
set PYTHONPATH=tools;tools\debug
python -m debug.worker --agent tcp://127.0.0.1:PORT --debugpy 5678
```

5. Attach using `tools/debug/vscode/launch.json` → **Attach debug (debugpy 5678)**,
   or extension command **Merge Sample launch.json** (merge by `name` only).
6. MCP smoke: `set PYTHONPATH=tools;tools\debug` then `python -m debug.mcp`.

Edit-time IntelliSense: `tools/debug/smartgis/*.pyi` + `pyrightconfig.json`.

## Unit gate targets

| Target / cmd | Proves |
| --- | --- |
| `debug_agent_test` | Socket ping, `log.tail`, `cmd.exec :help` |
| `content_console_coverage_test` | Local cmds + socket RPC coverage (hard-fail path) |
| `pytest tools/debug/tests` | Client, discovery, worker, helpers (mock agent) |

Build examples:

```bat
.\build.bat debug debug_agent
.\build.bat debug debug_agent_test
.\build.bat debug content_console_coverage_test
```

## Evidence loop cheat-sheet

Do this **while the process is still up**, then build, then repeat until the done bar is green. Details: `SKILL.md` “Evidence loop”.

| Step | Action |
| --- | --- |
| 1 | Same repro. `:log.level debug` or `log_set_level("debug")` if the ring is quiet |
| 2 | `log_tail(200)` / `:diag` / `diag.pack` / RPC error / test stdout — quote it |
| 3 | Text still cannot name the layer → **one** `LOGGING` (or worker `logging`) at that seam |
| 3b | Screen/layout/overlay wrong, or text looks healthy → `:ui capture out/Debug/captures/_scratch/<name>.bmp` and/or `:diag capture` (BMP bytes; empty path → `out/ui_forensics/capture.bmp`) |
| 4 | `.\build.bat debug <target>` so the new log or capture site is in the binary |
| 5 | Re-run the same cmd; cite the new log line or screenshot path |
| 6 | Only then root-cause fix → `build.bat` again → same repro → three gates |

`:ui capture` needs a bound UI host (`ui host not bound` means the shell hook is down — say so, do not invent a path). Worker jobs: `--job-timeout` plus stdout/stderr on the `py.*` result.

## When to hand off

| Symptom | Skill |
| --- | --- |
| Compile / link / LNK1168 only | `auto-build-fix` |
| Product `harness` / `te` / Views smoke | `auto-bug-fix` |
| AV / hang / `.dmp` / timeout with frozen PE | `auto-diagnose-fix` → back here |
| Agent cmd / schema / worker / coverage | **this skill** |
