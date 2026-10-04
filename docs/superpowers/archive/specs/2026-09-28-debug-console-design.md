<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-27-views-desktop-shell-design.md`](../../specs/2026-09-27-views-desktop-shell-design.md) — §Debug console / LogSink (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# Design: system LogSink + Views Debug Console + out-of-process Python + sdbd SQL

**Status:** superseded (2026-09-28 merge B)
**Date:** 2026-09-28  
**Updated:** 2026-09-28  
**Scope:** Process-wide debug logging sink; opt-in Debug Agent (loopback); bottom-dock Debug Console in Views; out-of-process Python worker with VS Code LSP (Pyright stubs) + DAP (debugpy); built-in sdbd query via existing `SdbdClient`.  
**Living considered:** [`2026-09-27-views-desktop-shell-design.md`](../../specs/2026-09-27-views-desktop-shell-design.md) (Console UI only — cross-ref § there); [`2026-09-19-sdbd-wsl-client-design.md`](2026-09-19-sdbd-wsl-client-design.md) (reuse client, no new sdbd contract). **Why new dated file:** logging sink + Debug Agent + Python worker are a new cross-layer subsystem (`base` / `content` / tooling), not a views menu/layout-only change; folding into views-shell would mis-own `base` and net protocol.

**Related:** `src/base/core/log.h` (today stderr-only macros); `src/base/trace/` + Diagnostic Tools CPU/Memory tabs; `gis/datasource/provider/impl/sdbd/client`.

---

## Locked decisions

| ID | Decision |
| --- | --- |
| **DBG-GOAL** | Log output + interactive console commands + executable Python scripts + built-in sdbd SQL/query. |
| **DBG-SCRIPT** | Out-of-process Python worker (not embedded CPython). Product APIs via JSON-RPC back to Debug Agent. |
| **DBG-TOOLING** | VS Code **LSP + DAP**: ship `.pyi` stubs + `pyrightconfig` sample; worker optional `--debugpy <port>`; sample `.vscode/launch.json`. Do not embed a language server binary in the product. |
| **DBG-TRANSPORT** | Default **loopback TCP** (`127.0.0.1`); optional Windows named pipe later. No `0.0.0.0` in v1. True remote = SSH port forward. |
| **DBG-UI** | Bottom dock **Diagnostic Tools** (`DiagnosticToolsPanel`): tabs Output \| Console \| CPU \| Memory. View menu Toggle. |
| **DBG-OPTIN** | Default **off**. Enable via `--debug-console`, `SG_DEBUG=1`, or View → Toggle Diagnostic Tools (starts Agent). |
| **DBG-ARCH** | Layered: `base::LogSink` → `content` DebugAgent → Views panel + Python worker. Panel does not call `SdbdClient` directly. |
| **DBG-TRACE** | CPU/Memory share `base::process_trace` (+ memory counters). LogSink stays separate from timing/memory samples. |
| **DBG-GIT** | Work on `master`. Partition parallel agents by non-overlapping paths. |

---

## 1. Architecture

```
LOGGING → base::LogSink (ring + level) → stderr + subscribers
                │
SmartGisViews ──┤── DebugAgent  127.0.0.1:<port>
                │     log.* / cmd.exec / sdbd.* / py.*
                │     → Browser / MapScene / SdbdClient
                │     → spawn Python worker
                └── DebugConsolePanel (bottom dock)
                         ↑ subscribe LogSink; input → Agent

Python worker ←── JSON-RPC ──→ Agent
VS Code DAP ──→ debugpy on worker
VS Code LSP ──→ Pyright + smartgis/*.pyi (edit-time only)
```

Endpoint discovery file: `%TEMP%/smartgis-debug.json`  
`{ "host":"127.0.0.1", "port":N, "pid":P }` written when Agent starts; deleted on shutdown.

---

## 2. Components

### 2.1 `base::LogSink` (`src/base/log/`)

- Process-wide singleton `log_sink()`.
- Ring buffer default capacity **4096** entries; each entry: level, timestamp, tid, file, line, func, message.
- `LOGGING` macros route through sink (still fprintf stderr).
- API: `append`, `set_min_level`, `min_level`, `subscribe` / `unsubscribe`, `snapshot_tail(n)`, `clear`.
- Thread-safe with `std::mutex`. Subscriber callbacks run under the lock **briefly** — callers must not re-enter sink; UI posts to UI thread.

### 2.2 `content::browser::debug::DebugAgent` (`src/content/browser/debug/`)

- Opt-in listen on `127.0.0.1:0` (ephemeral) or `SG_DEBUG_PORT`.
- Protocol: **newline-delimited JSON** request/response + server push events.
- Request: `{ "id":1, "method":"…", "params":{…} }`  
- Response: `{ "id":1, "ok":true, "result":{…} }` or `{ "id":1, "ok":false, "error":"…" }`  
- Event: `{ "event":"log", "entry":{…} }`

| method | params | result |
| --- | --- | --- |
| `ping` | — | `{ "pong": true }` |
| `log.tail` | `{ "n": 200 }` | `{ "entries":[…] }` |
| `log.subscribe` | — | ok; then push `log` events |
| `log.set_level` | `{ "level":"DEBUG" }` | ok |
| `cmd.exec` | `{ "line":"…" }` | `{ "output":"…" }` |
| `sdbd.capabilities` | — | raw client body / status |
| `sdbd.collections` | — | raw |
| `sdbd.query` | `{ "body": "<json or sql wrapper>" }` | raw `SdbdCallResult` fields |
| `py.eval` | `{ "code":"…" }` | worker stdout/stderr/value |
| `py.run_file` | `{ "path":"…" }` | same |
| `shutdown` | — | stops agent (process keeps running) |

**Builtin console lines** (panel / `cmd.exec`):

| input | behavior |
| --- | --- |
| `:help` | list commands |
| `:clear` | clear panel buffer (and optional sink clear) |
| `:log.level <L>` | set min level |
| `:refresh` | map refresh via Browser |
| `:extent` | print current 2D extent |
| `:layers` | list layer names |
| `:sdbd capabilities` | Agent `sdbd.capabilities` |
| `:sdbd sql <text>` | wrap/post via `sdbd.query` |
| `:py <code>` | `py.eval` |
| `:run <path>` | `py.run_file` |
| other | if worker up → `py.eval` one-liner; else error hint |

Command allowlist only — **no** arbitrary `system()` / shell.

### 2.3 Python worker (`tools/debug/`)

- Entry: `python -m debug.worker --agent tcp://127.0.0.1:PORT [--debugpy 5678]`
- Package `smartgis` thin RPC client: `log`, `cmd`, `sdbd.query`, etc.
- Stubs: `tools/debug/smartgis/*.pyi` + `pyrightconfig.json` sample.
- Host finds `python` on PATH (`SG_PYTHON` override). Missing Python → Agent still serves log/cmd/sdbd; `:py` reports clear error.

### 2.4 `DebugConsolePanel`

- Preferred toolkit home: `src/ui/gis/debug/debug_console_panel.*` (peer of `RenderTracePanel`).
- Wired from `app/views/shell/ui` as bottom dock splitter child; View menu toggle.
- Output: subscribe `LogSink` + Agent command output lines.
- Input: history (↑/↓), Enter submits.

### 2.5 Enablement

| trigger | effect |
| --- | --- |
| `--debug-console` | start Agent at BrowserMain |
| `SG_DEBUG=1` | same |
| View → Toggle Debug Console | show panel; start Agent if needed |
| closing panel | hide UI; Agent stays until `:shutdown` or process exit |

---

## 3. Errors, security, testing

### 3.1 Errors

- Agent bind failure → log ERROR, panel shows banner; product continues.
- sdbd unreachable → `ok:false` with client error string; no crash.
- Python missing / worker exit → `py.*` returns error; Agent remains up.
- Malformed JSON line → per-connection error response; connection kept.

### 3.2 Security

- Bind **127.0.0.1 only** in v1.
- Opt-in only; release builds may still enable via flag (no secret debugger always-on).
- `cmd.exec` allowlist; `py.run_file` reads path as UTF-8 file content sent to worker (worker runs with user privileges — documented).
- Discovery file is user-temp only.

### 3.3 Testing

| test | covers |
| --- | --- |
| `log_sink_test` | append, ring wrap, level filter, subscribe |
| `debug_agent_test` | ping, log.tail, cmd help, refuse unknown cmd (loopback) |
| `tools/debug` pytest | RPC client encode/decode against mock agent |
| manual / e2e later | Toggle panel + `:sdbd capabilities` when sdbd up |

No new `--self-test` scene required in v1.

---

## 4. Non-goals (v1)

- Embed CPython / bundle full stdlib.
- Public remote TCP + token auth.
- Merge Chrome Trace UI into Console.
- Full product Python bindings beyond Agent RPC surface.
- Qt / second widget kit.

---

## 5. Cross-refs to update when landing

- `docs/superpowers/README.md` Active table — add this row.
- `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` — short § Debug Console pointer.
- `src/app/views/README.md` — mention Toggle Debug Console when UI lands.
- `docs/superpowers/src-layout.md` — `base/log`, `content/browser/debug` if layout table lists peers.
