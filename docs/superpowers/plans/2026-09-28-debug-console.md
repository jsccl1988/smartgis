<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Debug Console + LogSink Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship process-wide `LogSink`, opt-in loopback `DebugAgent`, bottom-dock Debug Console, out-of-process Python worker (LSP stubs + DAP hooks), and built-in sdbd query — per [`../specs/2026-09-28-debug-console-design.md`](../specs/2026-09-27-views-desktop-shell-design.md).

**Architecture:** `LOGGING` → `base::LogSink`; `content/browser/debug` owns Agent + command table + sdbd/py bridge; `ui/gis/debug/debug_console_panel` + shell wiring for UI; `tools/debug` worker talks JSON-RPC to Agent.

**Tech Stack:** C++23, GN/Ninja `out/`, Winsock/ASIO loopback TCP, system Python 3, Pyright stubs, debugpy optional.

## Global Constraints

- Work on `master` only; parallel agents must use **non-overlapping paths**.
- No Qt; Views + Skia only for UI.
- Bind `127.0.0.1` only; opt-in (`--debug-console` / `SG_DEBUG=1` / View toggle).
- New-tree functions `snake_case`; namespaces ≤ two public layers (`base`, `content`, `ui::views`).
- Copyright year **2026**; English comments.
- Build via `build.bat` / `ninja -C out`; output root `out/` only.
- Do **not** merge with `base::trace` / `RenderTracePanel`.
- Reuse `gis::datasource::SdbdClient`; do not revive in-process sdbd HTTP handler.

## File map

| Path | Role |
| --- | --- |
| `src/base/log/log_sink.h`, `log_sink.cc`, `BUILD.gn`, `log_sink_test.cc` | Ring sink + subscribe |
| `src/base/core/log.h` | Route `LOGGING` through sink |
| `src/base/BUILD.gn` | `public_deps` on `:log` |
| `src/content/browser/debug/*` | Agent, protocol, commands, python spawn |
| `src/content/BUILD.gn` | sources + `debug_agent_test` |
| `src/ui/gis/debug/debug_console_panel.{h,cc}` | Bottom panel widget |
| `src/app/views/shell/ui/*`, `view_commands*`, `browser_view*` | Dock + menu toggle + enablement |
| `tools/debug/` | worker + `smartgis` RPC + `.pyi` |
| `.vscode/launch.json` (sample under `tools/debug/vscode/`) | DAP sample (do not clobber user `.vscode` blindly — ship under package) |

**Parallelism:** Task 1 ∥ Task 2; then Task 3; then Task 4; Task 5 docs can ∥ Task 4.

---

### Task 1: `base::LogSink` + `LOGGING` bridge

**Files:**
- Create: `src/base/log/log_sink.h`, `src/base/log/log_sink.cc`, `src/base/log/BUILD.gn`, `src/base/log/log_sink_test.cc`
- Modify: `src/base/core/log.h`, `src/base/BUILD.gn`
- Test: `out/log_sink_test.exe` via `ninja -C out log_sink_test`

**Interfaces:**
- Produces:
  - `enum class LogLevel { kFatal, kError, kWarning, kNotice, kInfo, kDebug, kTrace };`
  - `struct LogEntry { LogLevel level; std::string timestamp; int tid; std::string file; int line; std::string func; std::string message; };`
  - `class LogSink` with `append`, `set_min_level`, `min_level`, `subscribe`, `unsubscribe`, `snapshot_tail`, `clear`, `set_capacity`
  - `LogSink& log_sink();`
  - `LOGGING` still usable; each line also `log_sink().append(...)`

- [ ] **Step 1: Add `log_sink.h` / `log_sink.cc` and GN `source_set("log")` + `test("log_sink_test")`**

Header sketch (match `src/base/trace` header style, copyright 2026):

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_LOG_LOG_SINK_H_
#define BASE_LOG_LOG_SINK_H_

#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace base {

enum class LogLevel {
  kFatal,
  kError,
  kWarning,
  kNotice,
  kInfo,
  kDebug,
  kTrace,
};

struct LogEntry {
  LogLevel level = LogLevel::kInfo;
  std::string timestamp;
  int tid = 0;
  std::string file;
  int line = 0;
  std::string func;
  std::string message;
};

class LogSink {
 public:
  using Subscriber = std::function<void(const LogEntry&)>;

  void set_capacity(std::size_t n);
  void set_min_level(LogLevel level);
  LogLevel min_level() const;

  void append(LogEntry entry);
  void clear();
  std::vector<LogEntry> snapshot_tail(std::size_t n) const;

  // Returns subscription id; callback may run on the logging thread under mutex.
  std::uint64_t subscribe(Subscriber cb);
  void unsubscribe(std::uint64_t id);

 private:
  mutable std::mutex mu_;
  std::size_t capacity_ = 4096;
  LogLevel min_level_ = LogLevel::kTrace;
  std::deque<LogEntry> entries_;
  std::uint64_t next_id_ = 1;
  std::vector<std::pair<std::uint64_t, Subscriber>> subs_;
};

LogSink& log_sink();
const char* log_level_name(LogLevel level);
bool parse_log_level(const std::string& name, LogLevel* out);

}  // namespace base

#endif
```

- [ ] **Step 2: Write `log_sink_test.cc`** covering append order, capacity wrap, min_level filter (dropped below min not stored), subscribe receives append.

- [ ] **Step 3: Wire `LOGGING` in `log.h` to call `log_sink().append` after fprintf** (include `base/log/log_sink.h`; map `LOG_*` color macros to `LogLevel`). Keep stderr behavior.

- [ ] **Step 4: `public_deps` `//src/base/log:log` from `foundation` in `src/base/BUILD.gn`.**

- [ ] **Step 5: Build and run**

```bat
ninja -C out log_sink_test
out\log_sink_test.exe
```

Expected: PASS.

- [ ] **Step 6: Commit** (if landing commits authorized)

```bat
git add src/base/log src/base/core/log.h src/base/BUILD.gn
git commit -m "feat(base): add LogSink ring buffer and route LOGGING"
```

---

### Task 2: Python worker + stubs (no C++ product deps)

**Files:**
- Create: `tools/debug/__init__.py`, `tools/debug/worker.py`, `tools/debug/smartgis/__init__.py`, `tools/debug/smartgis/client.py`, `tools/debug/smartgis/__init__.pyi` / `client.pyi`, `tools/debug/pyrightconfig.json`, `tools/debug/vscode/launch.json`, `tools/debug/README.md`, `tools/debug/tests/test_client.py`

**Interfaces:**
- Consumes: Agent NDJSON over TCP (`method`/`params`/`id`)
- Produces: `smartgis.client.AgentClient` with `call(method, params) -> dict`; `worker` main loop handling `eval` / `run_file` requests **from Agent** (Agent is server; worker is client that also accepts reverse `py.eval` via a second channel **or** Agent opens connection and sends jobs).

**Locked worker model (v1):** Worker connects to Agent as a client and registers with `{ "method":"py.register" }`. Agent then sends `{ "method":"py.job", "params":{ "id", "code"|"path" } }` on that connection; worker replies with job results. Simpler alternative if faster: **Agent spawns worker per `py.eval`** with code on stdin and reads stdout — allowed for v1 if documented in README; prefer persistent worker if spawn cost is OK on Windows.

Prefer **persistent worker** with register + job messages.

- [ ] **Step 1: Implement `AgentClient` + NDJSON framing**

- [ ] **Step 2: Implement `worker.py` register/job loop; optional `--debugpy PORT` calls `debugpy.listen` + `wait_for_client` only if flag set**

- [ ] **Step 3: Add `.pyi` + `pyrightconfig.json` + `vscode/launch.json` sample attaching to debugpy port 5678**

- [ ] **Step 4: `python -m pytest tools/debug/tests -q`** (mock socket or local echo server)

- [ ] **Step 5: Commit**

```bat
git add tools/debug
git commit -m "feat(debug): add out-of-process Python tools/debug worker"
```

---

### Task 3: `DebugAgent` + builtin commands + sdbd bridge

**Files:**
- Create: `src/content/browser/debug/debug_agent.h`, `debug_agent.cc`, `debug_protocol.h`, `debug_commands.h`, `debug_commands.cc`, `debug_agent_test.cc`
- Modify: `src/content/BUILD.gn`, enablement hooks in `src/app/views/shell/app/` cmdline (`views_launch_options`) and/or `browser_main`

**Interfaces:**
- Consumes: `base::log_sink()`, `gis::datasource::SdbdClient`, Browser callbacks injected via `DebugAgentHost` interface (narrow: `refresh_map`, `extent_string`, `layer_names`)
- Produces: `class DebugAgent { bool start(); void stop(); int port() const; bool handle_line(...); };`

- [ ] **Step 1: Implement NDJSON TCP accept loop on background thread (Winsock or ASIO already in `net`)** — loopback only.

- [ ] **Step 2: Implement methods table from spec §2.2; write discovery `%TEMP%/smartgis-debug.json`**

- [ ] **Step 3: Spawn/attach Python worker using `SG_PYTHON` or `python`; implement `py.eval` / `py.run_file`**

- [ ] **Step 4: `debug_agent_test` — start agent, connect, `ping`, `log.tail`, `cmd.exec` `:help`, unknown cmd fails**

- [ ] **Step 5: Wire `--debug-console` / `SG_DEBUG=1` to `start()`**

- [ ] **Step 6: Build/run tests; commit**

```bat
ninja -C out debug_agent_test
out\debug_agent_test.exe
git commit -m "feat(content): add DebugAgent loopback JSON-RPC and sdbd/py bridge"
```

---

### Task 4: `DebugConsolePanel` + shell bottom dock + View toggle

**Files:**
- Create: `src/ui/gis/debug/debug_console_panel.h`, `debug_console_panel.cc`
- Modify: `src/ui/views` BUILD for panel sources; `src/app/views/shell/ui/browser_view.*`, panels wiring, `view_commands` / menu for Toggle Debug Console
- Match peers: `render_trace_panel`, `atmosphere_panel`

- [ ] **Step 1: Panel UI — multiline log `Label`/custom paint or existing text view if any; `Textfield` input; history**

- [ ] **Step 2: Subscribe `log_sink` on show; post lines to UI thread**

- [ ] **Step 3: Enter → Agent `cmd.exec` (or local parse `:`); ensure Agent started on toggle**

- [ ] **Step 4: Bottom `Splitter` in `BrowserView` (console collapsed height 0 when hidden)**

- [ ] **Step 5: Build `SmartGisViews` / views target; smoke Toggle manually or unit-test panel construction if feasible**

- [ ] **Step 6: Commit**

```bat
git commit -m "feat(views): add bottom DebugConsolePanel and View menu toggle"
```

---

### Task 5: Docs cross-refs

**Files:**
- Modify: `docs/superpowers/README.md`, `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` (short § pointer), `src/app/views/README.md`, `docs/superpowers/src-layout.md` if it lists `base/` children

- [ ] **Step 1: Add Active row for debug-console spec + plan link**

- [ ] **Step 2: Views shell § Debug Console → link spec**

- [ ] **Step 3: Commit docs**

```bat
git commit -m "docs: index debug console living spec and shell cross-ref"
```

---

## Spec coverage check

| Spec item | Task |
| --- | --- |
| LogSink + LOGGING | 1 |
| DebugAgent + NDJSON + discovery file | 3 |
| Builtin commands + sdbd | 3 |
| Python worker + LSP/DAP samples | 2 |
| Bottom dock Console + opt-in | 4 |
| Docs / Active table | 5 |
| Separate from trace | all (non-goal) |
| Console coverage + performance (living §) | 6 |

---

### Task 6: Console coverage + performance (L0 / L1 / L2)

Living §: [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) **§Console coverage + performance**. As-built: [`../ui-testing.md`](../ui-testing.md), [`../../../testing/README.md`](../../../testing/README.md).

**Locked:** Scope C; Industry D (QGIS-like cmds; GDAL-like data + MapLibre-like viewport timings, no absolute cross-product); Run A+B+C; Data C+D (synthetic + `china_map_samples` + DEM/tile soft). OpenCppCoverage optional — does **not** block `te`.

- [x] **L0 — `content_console_coverage_test`:** headless Agent / command matrix; register in `//:test_all`; green under `build.bat te`
- [x] **L1 — `content_console_bench`:** register in `//:benchmark_all`; `build.bat b` writes `console_bench.json` (data + viewport soft timings)
- [x] **L2 — `SmartGisViews.exe --self-test-console`:** shell e2e / self-test path; Console-driven app smoke + JSON where applicable
- [x] **Optional coverage:** `testing/scripts/open_cpp_coverage_console.ps1` — OpenCppCoverage on PATH → HTML/cobertura under `out/Debug/coverage/console/` (sources: `src/content/browser/debug`, `src/base/log`); missing tool → exit 0 skip (do not fail CI)
- [x] **Docs:** keep `ui-testing.md` + `testing/README.md` in sync with L0/L1/L2 entry points

### Task 7: Always-on Diagnostic Tools (startup / Gantt / memory)

Living §: [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) **§Diagnostic Tools → Always-on auto-collect**.

- [x] `base::trace::start_always_on_diagnostics` + 500ms memory sampler + AllocationTracker
- [x] `base::trace::set_tracing_enabled` clears only on off→on
- [x] Startup `BASE_TRACE_EVENT` + `LOGGING` in `wWinMain` / `BrowserMain` / `Browser::init`
- [x] Diagnostic Tools auto-refresh; CPU Startup swimlane filter; Memory always-on samples

## Execution

User directed: **parallel land, no further confirmation.** Run Task 1 ∥ Task 2, then 3, then 4 ∥ 5; Task 6 when coverage/bench targets land.
