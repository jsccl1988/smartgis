<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/content/browser/debug` — DebugAgent (loopback console)

NDJSON debug agent for Diagnostic Tools / `tools/debug` workers /
VS Code extension (`tools/debug/vscode-ext`) + MCP (`tools/debug/mcp`).
Debug builds (`!NDEBUG` / `out/Debug`) start it by default so discovery
(`%TEMP%/smartgis-debug.json`) is written without `SG_DEBUG=1`; set
`SG_DEBUG=0` to disable. Release stays opt-in (`--debug-console` /
`SG_DEBUG=1`).
Public include surface stays at the module root; internals split by
responsibility (same colocation style as `../present/`).

## Layout

```
debug/
  debug_agent.*              # Facade: listen / dispatch / host hooks / record ring
  debug_agent_test.cc
  content_console_*          # Coverage + benchmark TUs
  wire/                      # NDJSON / RapidJSON helpers (ok_result, extract_*)
  policy/                    # Dangerous-op gate (confirm / SG_DEBUG_ALLOW)
  schema/                    # rpc.methods catalog + :help text + Tab prefixes
  cmd/                       # Domain handlers (console :cmds + matching RPC)
    agent_ask / agent_diag / agent_harness / agent_log / agent_gis_cmd
    agent_py / agent_record / agent_sdbd / agent_ui
```

| Layer | Role | Who includes |
| --- | --- | --- |
| Facade (`debug_agent.h`) | Start/stop, `exec_line`, record ring, policy session | Shell / MFC console / tests |
| `wire/` | Shared JSON escape + RPC envelope | `cmd/*`, facade |
| `policy/` | Dangerous method/line classification | Facade (embedded `AgentPolicy`) |
| `schema/` | Method catalog + help | Facade, `:ask` |
| `cmd/` | One domain per pair (log / ui / py / …) | Facade dispatch only |

Namespaces stay `content` (handlers in `content::detail`). Do **not** add a
third public namespace under `debug`.

## GN

- `:debug_agent` — facade + wire + policy + schema + cmd
- `:debug_agent_test` / `:content_console_coverage_test` / `:content_console_bench`

## Verify

```bat
build.bat debug debug_agent
build.bat debug debug_agent_test
build.bat debug content_console_coverage_test
```
