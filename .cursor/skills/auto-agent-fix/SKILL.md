---
name: auto-agent-fix
description: >-
  Use when fixing SmartGIS DebugAgent / debug console / tools/debug worker
  failures, hand-written :cmd / NDJSON RPC gates, content_console coverage,
  or invoking /auto-agent-fix; also when the user says Agent 调试, DebugAgent,
  调试控制台, :gis test, :diag, log.tail, :ui capture, tools/debug, vscode
  debugpy attach, 手写测试 cmd, 修到门禁绿, 截图取证, or 循环修到成功.
  Not compile-only (auto-build-fix). Not product harness/te (auto-bug-fix).
  Not native crash dump (auto-diagnose-fix first).
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto agent-fix until DebugAgent gates green

**用 DebugAgent + 手写测试 cmd，运行中取证，证据不够就加日志或截图，构建后再修，循环到门禁通过**

Stack under test:

| Lane | Path |
| --- | --- |
| In-process Agent | `src/content/browser/debug/` (`DebugAgent`, `cmd/*`, `schema/`, `policy/`, `wire/`) |
| OOP Python + VS Code | `tools/debug/` (`worker.py`, `smartgis/*`, `mcp/`, `vscode-ext/`, `tests/`, `vscode/launch.json`) |
| Hand cmds | console `:…` lines + NDJSON RPC (`cmd.exec`, `log.*`, `diag.pack`, …) |

**REQUIRED BACKGROUND:** superpowers:systematic-debugging (hypothesis first; do not shotgun).
**REQUIRED SUB-SKILL:** codebase-memory (project `smartgis`) before repo-wide Grep.
**SIBLINGS:** compile-only → `auto-build-fix`. Product `harness`/`te` → `auto-bug-fix`. Native crash/hang/`.dmp` → `auto-diagnose-fix` then return here.

## Authorization

When this skill is invoked, attached (`@auto-agent-fix` / `/auto-agent-fix`), or triggered by DebugAgent / console-cmd / `tools/debug` failures, the agent **MUST** stay in the evidence loop below until the **done bar** exits 0. Do not hand the loop back to the user. Do not stop after one failed gate, one hypothesis, or one rebuild.

## Hard rules

1. **Entry:** from the **smartgis** repo root. Prefer `.\build.bat debug <target>`.
2. **Compile lock OFF.** Do not create `out/.build.lock.on` or set `SMARTGIS_BUILD_LOCK=1`.
3. **Do not** use `SmartGIS.sln` / MSBuild / `build.bat sln`.
4. **CBM first** (`user-codebase-memory-mcp`, project `smartgis`, `root_path` `C:/Dev/src/gis/smartgis`). Grep only if CBM is down, the user named an exact path, or the search is already scoped (`path` required).
5. **Stay on `master`.** No new branches unless the user explicitly asks.
6. Fix **root cause**. Not `#if 0`, not deleting / hiding tests, not stubbing asserts, not weakening `AgentPolicy` / dangerous-op gates to “make green”.
7. Keep `agent_methods.json` / `agent_schema.cc` / console prefixes **in sync** when adding or renaming methods/cmds.
8. Agent binds **`127.0.0.1` only**. Do not open external listen addresses.

## Done bar (mandatory)

All of the following must exit **0** on a fresh run:

```bat
.\build.bat debug debug_agent_test
.\build.bat debug content_console_coverage_test
set PYTHONPATH=tools
py -3 -m pytest tools\debug\tests -q
```

Optional helper (same gate):

```powershell
pwsh -NoProfile -File .cursor\skills\auto-agent-fix\scripts\run_agent_gate.ps1
```

**Not enough alone:** ninja exit 0 without running the three gates above.

**Live / product attach** (when the bug needs a running shell, or the user asked for live cmd repro):

1. Start product with Agent enabled: `SG_DEBUG=1` or `--debug-console` (discovery → `%TEMP%\smartgis-debug.json`).
2. Hand cmds via console or RPC — see [reference.md](reference.md).
3. Optional VS Code: merge/attach `tools/debug/vscode/launch.json` (debugpy **5678**); run `python -m debug.worker --agent tcp://127.0.0.1:PORT --debugpy 5678`.
4. Optional smoke: `py -3 tools\debug\scripts\ui_smoke.py` (tolerates unbound UI hooks).

Live steps **supplement** the done bar; they do not replace `debug_agent_test` + coverage + pytest.

## Evidence loop (until the done bar is green)

Stay in this loop. One iteration = one repro while the process is alive, one evidence pack, at most one instrumentation change, one rebuild, then either another repro or one root-cause fix. Empty evidence → no behavior edit.

```
while any done-bar command exits non-zero:
  A. Run the same repro. While it is alive, collect the pack (below).
  B. Pack cannot name the failing layer?
       add ONE log site OR ONE screenshot capture
       .\build.bat debug <target that contains that change>
       go back to A and quote the new line or image path
  C. Hypothesis = one sentence tied to that quote
  D. Root-cause fix in that layer only
  E. .\build.bat debug <affected test or debug_agent>
  F. Re-run the same repro, then the three done-bar commands
```

Copy and fill **every** iteration before the next edit:

```
Agent iteration N:
- [ ] Repro (same exe / :cmd / RPC; process was running)
- [ ] Evidence quoted from this run (log line and/or screenshot path)
- [ ] Failing layer named, or "thin" → instrumentation only
- [ ] Hypothesis (one sentence) — only after the quote exists
- [ ] Root-cause fix, or instrumentation-only this round
- [ ] build.bat debug <target> exit 0
- [ ] Same repro re-run
- [ ] Done bar (only claim green when all three exit 0)
```

### A) Collect while it runs

Launch the failing surface and **pull evidence before it exits** when the bug is live (shell, worker, socket). Unit exes that exit immediately: capture their stdout/stderr in the same command.

- Unit first when the gate already fails:
  - `out\Debug\debug_agent_test.exe`
  - `out\Debug\content_console_coverage_test.exe`
  - `py -3 -m pytest tools\debug\tests -q` (`PYTHONPATH=tools`)
- Same product cmd the console uses: `:help` / `:diag` / `:layers` / `:extent` / `:log.level debug` / `:gis test` / `:rhi test` / `:ui …` / `:sdbd …` / `:py …` / `:record …`
- RPC: `cmd.exec` with `{"line":":…"}` via `smartgis.cmd_exec` or a one-shot NDJSON client.

**Enough** means all three:

1. A quoted symptom from **this** run (log line, RPC `error`, test assert, or screenshot path plus what is wrong in the image).
2. A named layer: **wire/schema**, **policy**, **cmd handler**, **host hooks**, **Python client/worker**, or **VS Code/DAP**.
3. The quote distinguishes that layer from the neighbors (dispatch vs handler vs hook).

Anything less is **thin**. Do not change behavior yet.

### B) Thin pack → one log or one screenshot, then build

Pick **one** seam. Do not sprinkle both across unrelated modules in the same iteration.

| What you still cannot see | Add this | Then |
| --- | --- | --- |
| Branch, args, or RPC result unknown | One `LOGGING(LOG_INFO/LOG_DEBUG, …)` or one Python `logging` line in that handler / `debug_agent.cc` dispatch / worker | `.\build.bat debug` the target that compiles it |
| Pixels, layout, overlay, or shell wrong while text logs look fine | `:ui capture <path.bmp>` and/or `:diag capture` while the process is up | Writer emits BMP bytes. Prefer `out/Debug/captures/_scratch/<name>.bmp` (empty path defaults to `out/ui_forensics/capture.bmp`). Rebuild only if you added a capture call site |

Also raise the sink first when the ring is empty: `:log.level debug` or `log.set_level` / `log.subscribe`, then `log.tail` (n≥200). That is not a code edit. If the ring is still silent, add the log site and build.

Comments on new logs stay **English**. After the build, run **A** again and paste the new line or the image path into the iteration record. Only then go to C.

### C–E) Hypothesis, fix, rebuild

- Fix that named layer only. Public surface moves stay in sync (`schema/agent_schema.cc`, `tools/debug/agent_methods.json`, `tools/debug/console_prefixes.json`).
- Dangerous ops still require `:confirm` / `rpc.confirm` / `SG_DEBUG_ALLOW=1`. Do not weaken `AgentPolicy` to get a green run.
- End the iteration with `.\build.bat debug <target>`, then the **same** repro.

### F) Re-gate

Re-run the **done bar**. Non-zero → next iteration (new pack; do not reuse last round’s quotes). Native crash/hang/timeout of a child PE: `auto-diagnose-fix`, then return here.

Claim green **only** after a fresh done-bar run in this iteration. Show commands, exit 0, and key PASS lines (`debug_agent_test` asserts, `content_console_coverage_test: ok`, pytest summary).

## LNK1168 / locked PE

Stop locker gently (`Stop-Process` / `taskkill` without `/F` first; `/F` only if still alive) via non-sandbox terminal. Relink. Not a source bug.

## Hard stops

- Same failure unchanged **3+** times after each of those rounds added a log or a screenshot **and** rebuilt
- Missing Python 3.10+ / pytest for `tools/debug/tests`
- Missing VS C++ / SDK / gn/ninja env
- Unanalyzable native crash after `auto-diagnose-fix`

## Communication

- Progress in **简体中文**
- Code / identifiers / cmd names in **English**

## Rationalizations (do not)

| Excuse | Reality |
|--------|---------|
| "ninja already green" | Done bar is agent tests + pytest. |
| "One red gate is enough to report" | Stay in the loop until all three exit 0. |
| "I already know the bug" | No behavior edit until this iteration quotes a live log line or screenshot. |
| "Logs look fine, skip the capture" | Visual / layout / overlay failure needs `:ui capture` or `:diag capture` before the next edit. |
| "I'll rebuild after a few more theories" | Instrumentation and the root-cause fix each end with `build.bat`, then the same repro. |
| "Ask the user to continue the loop" | This skill owns the loop. |
| "Skip pytest; C++ tests passed" | Worker/client regressions hide there. |
| "Weaken AgentPolicy / auto-confirm forever" | Forbidden. |
| "Delete failing coverage assert" | Hide tests forbidden. |
| "Grep whole repo first" | CBM `search_graph` first. |
| "New branch for the fix" | Stay on `master` unless asked. |

## Red flags — stop

- Declaring success without pytest + both C++ agent exes
- Behavior edit while this iteration’s evidence line is empty
- Stopping after a single non-zero gate or handing the next rebuild to the user
- Visual failure with no screenshot path in the iteration record
- Shotgun edits without a hypothesis tied to a new log line or image
- Desyncing `agent_schema.cc` from `tools/debug/agent_methods.json`
- Opening external bind addresses or shipping secrets in discovery JSON

## Additional resources

- Cmd / RPC / discovery / VS Code attach details: [reference.md](reference.md)
- Module map: `src/content/browser/debug/README.md`
- Worker package: `tools/debug/README.md`
