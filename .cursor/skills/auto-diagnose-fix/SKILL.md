---
name: auto-diagnose-fix
description: >-
  Use when diagnosing Windows native crashes, hangs, freezes, deadlocks,
  minidumps, access violations, or faulting stacks with WinDbg/cdb; also when
  the user says crash, 崩溃, 卡死, hang, freeze, deadlock, not responding,
  timeout, dmp, AV, !analyze, blue-screen usermode, 自动诊断, /auto-diagnose-fix,
  or hands over a .dmp / hung PID. For this repo: cdb preferred over windbg GUI;
  PDBs under out/; logs under out/crash/; CBM project smartgis; after root-cause
  fix verify with build.bat harness / te (or the same repro) and hand back to
  auto-bug-fix when entered from that loop. Supersedes windbg-crash-diagnose.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto diagnose-fix (crash + hang)

**Windows 原生崩溃 / 卡死 → dump → cdb 分析 → CBM → 修根因 → 验证**

Prefer **`cdb.exe`** (non-interactive Debugging Tools for Windows). Do **not** open the WinDbg GUI unless cdb is unavailable and the user explicitly asks.

**REQUIRED BACKGROUND:** superpowers:systematic-debugging (hypothesis first; do not shotgun).
**REQUIRED SUB-SKILL:** codebase-memory for code lookup (project `smartgis`) before repo-wide Grep.
**SIBLING:** when entered from `auto-bug-fix`, return to that skill’s harness/te done bar after the failure no longer reproduces.

Former name **`windbg-crash-diagnose`** (removed). Prefer `/auto-diagnose-fix` / `@auto-diagnose-fix`.

## Authorization

When this skill is invoked, attached (`@auto-diagnose-fix` / `/auto-diagnose-fix`), or triggered by a crash / hang / `.dmp` / AV / timeout, the agent **MUST** classify → find cdb → dump/analyze → locate → fix → verify — do not hand the debugger session back to the user.

## Classify first

| Symptom | Mode | Script path |
|---------|------|-------------|
| AV, unhandled exception, crash exit, WER `.dmp` with exception | **crash** | `analyze_dump.ps1` / `run_and_catch.ps1` |
| UI freeze, not responding, deadlock, hang, harness `timeout` / `rc=124`, PE alive but stuck | **hang** | `dump_hang.ps1` / `run_and_hang.ps1` (+ `analyze_dump.ps1 -Mode Hang`) |
| Ambiguous (timeout then exit) | try **hang** dump if PID still alive; else **crash** catch / existing dump | |

Do not treat a hang as “test flaky” without all-thread stacks.

## Hard rules

1. **Tool:** `scripts/find_cdb.ps1` first. Missing cdb → **hard stop**; tell the user to install **Debugging Tools for Windows** (Windows SDK). Prefer `cdb.exe` over `windbg.exe`.
2. **Defaults (this repo):** `PdbDir=out`, `OutDir=out/crash`, Microsoft public symbol server. Do not invent a second PDB root.
3. **Scripts only for `-c`:** use `analyze_dump.ps1` / `run_and_catch.ps1` / `dump_hang.ps1` / `run_and_hang.ps1`; do not hand-roll fragile cdb command strings.
4. **CBM first** (`user-codebase-memory-mcp`, project `smartgis`, `root_path` `C:/Dev/src/gis/smartgis`). Grep only if CBM is down, the user named an exact path, or the search is already scoped (`path` required).
5. **Stay on `master`.** No new branches unless the user explicitly asks.
6. **Kill leftovers** (cdb / target PE) via non-sandbox / system terminal per `agent-terminal-kill` (gentle stop first, `/F` only if still alive).
7. Fix **root cause**. Not `#if 0`, not deleting / hiding tests, not stubbing asserts, not “increase timeout” as the only fix for a real deadlock.

## Crash loop

```
crash / .dmp / AV / failing PE
  → find_cdb.ps1
  → have .dmp?  yes → analyze_dump.ps1 (-Mode Crash, default)
                 no  → run_and_catch.ps1 (then analyze)
  → parse log (exception, module, !analyze, source frames)
  → CBM → fix → verify
```

## Hang loop

```
hang / freeze / timeout / not responding / live PID
  → find_cdb.ps1
  → have hang .dmp?  yes → analyze_dump.ps1 -Mode Hang
                    no  → dump_hang.ps1 (-ProcessId / -ProcessName)
                         OR run_and_hang.ps1 (launch, wait, dump if still alive)
  → parse log (!analyze -hang, ~*kv, !runaway, !locks)
  → CBM (blocked UI / wait / lock owners) → fix → verify
```

## Commands (repo root)

Use `pwsh -NoProfile -File …` when available; otherwise `powershell -NoProfile -File …`.

1. **Find tool:**  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/find_cdb.ps1`
2. **Crash dump:**  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/analyze_dump.ps1 -DumpPath <path.dmp>`
3. **Crash catch:**  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/run_and_catch.ps1 -ExePath out\Debug\SmartGIS.exe -ExeArgs '--self-test'`
4. **Hang attach (live PID / name):**  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/dump_hang.ps1 -ProcessId <pid>`  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/dump_hang.ps1 -ProcessName SmartGIS`
5. **Hang repro (launch + wait):**  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/run_and_hang.ps1 -ExePath out\Debug\SmartGIS.exe -ExeArgs '--harness' -TimeoutSec 180`
6. **Hang dump offline:**  
   `pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/analyze_dump.ps1 -DumpPath <path.dmp> -Mode Hang`

## Parse focus

**Crash log:** `ExceptionCode`, `FAULTING_IP`, `STACK_TEXT`, frames with `file.cc @ line`.

**Hang log:** `!analyze -v -hang` conclusion; threads blocked in `WaitFor*`, `NtWait*`, `MsgWait*`, lock wait; UI/main thread vs worker; `!runaway` (CPU hog vs idle wait); `!locks` owners when present.

## Verify

- Product path → `.\build.bat debug harness` then `.\build.bat debug te`.
- Otherwise re-run the same repro until no crash **and** no hang/timeout.
- If entered from `auto-bug-fix`, that skill’s done bar still applies.

**Done bar:** analyze report complete (exception **or** hang stacks + suspected symbol) **and** the repro path no longer fails. If entered from `auto-bug-fix`, also satisfy harness + te exit 0.

## Evidence before success

Claim fixed **only** after a fresh analyze (or clean run) plus verification command exit 0. Show: mode (crash/hang), cdb path, dump/log paths, key analyze lines, fix summary, verify command + exit code.

## Hard stops

- `cdb.exe` not found (install Debugging Tools for Windows)
- Cannot reproduce / catch timeout with no dump **and** no attachable PID
- Same failure unchanged **3+** times after hypothesized fixes
- Missing PDBs under `out/` for the faulting binary (rebuild first via `.\build.bat` / `auto-build-fix`)
- Hang attach denied (elevation / process exited) with no dump — ask for a better repro window

## Communication

- Progress in **简体中文**
- Code / identifiers / cdb commands in **English**

## Rationalizations (do not)

| Excuse | Reality |
|--------|---------|
| "Open WinDbg GUI for the user" | Use `cdb` + scripts; GUI is last resort. |
| "Ask the user to paste !analyze" | Agent runs analyze / dump_hang scripts. |
| "Grep the whole repo first" | CBM `search_graph` first. |
| "Quick `#if 0` around the AV" | Symptom patch. Find root cause. |
| "Just bump the timeout" | Only OK after proving no deadlock/livelock; prefer root cause. |
| "harness green without re-running crash/hang" | Repro / harness+te must prove the failure is gone. |

## Red flags — stop

- Declaring success from a partial stack with no hypothesis
- Editing tests to hide a crash/hang
- Shotgun edits without a stated hypothesis
- Opening a feature branch unprompted

## Example commands

```powershell
pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/find_cdb.ps1
pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/analyze_dump.ps1 -DumpPath out\crash\foo.dmp
pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/analyze_dump.ps1 -DumpPath out\crash\foo.dmp -Mode Hang
pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/run_and_catch.ps1 -ExePath out\Debug\SmartGIS.exe -ExeArgs '--self-test'
pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/dump_hang.ps1 -ProcessName SmartGIS
pwsh -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/run_and_hang.ps1 -ExePath out\Debug\SmartGIS.exe -TimeoutSec 180
.\build.bat debug harness
.\build.bat debug te
```

See [reference.md](reference.md) for symbol paths, exception codes, hang commands, and hard-coded cdb strings.
