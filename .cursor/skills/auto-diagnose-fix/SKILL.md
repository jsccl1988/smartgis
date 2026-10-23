---
name: auto-diagnose-fix
description: >-
  Use when diagnosing Windows native crashes, hangs, freezes, deadlocks,
  minidumps, access violations, or faulting stacks with WinDbg/cdb; also when
  the user says crash, 崩溃, 卡死, hang, freeze, deadlock, not responding,
  timeout, dmp, AV, !analyze, blue-screen usermode, 自动诊断, /auto-diagnose-fix,
  or hands over a .dmp / hung PID. For this repo: cdb preferred over windbg GUI;
  PDBs under out/; logs under out/crash/; CBM project smartgis; root-cause
  casebook in cases.md (read before fix, append after confirm); after root-cause
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
**REQUIRED CASEBOOK:** [cases.md](cases.md) — read before locate/fix; append after confirmed root cause.
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
  → read cases.md (match fingerprint) → CBM → fix → verify
  → append cases.md when root cause confirmed
```

## Hang loop

```
hang / freeze / timeout / not responding / live PID
  → find_cdb.ps1
  → have hang .dmp?  yes → analyze_dump.ps1 -Mode Hang
                    no  → dump_hang.ps1 (-ProcessId / -ProcessName)
                         OR run_and_hang.ps1 (launch, wait, dump if still alive)
  → parse log (!analyze -hang, ~*kv, !runaway, !locks)
  → read cases.md → CBM (blocked UI / wait / lock owners) → fix → verify
  → append cases.md when root cause confirmed
```

## Root-cause casebook (mandatory)

Path: **`.cursor/skills/auto-diagnose-fix/cases.md`** (living; commit with the fix).

| When | Action |
| --- | --- |
| After parse + digest | **Read** Index; if fingerprint / top frames / module match, reuse that hypothesis first |
| Root cause confirmed | **Append** a Case at the top of the Case log + one Index row (template inside `cases.md`) |
| Fix lands | Set `Status: fixed` on that case in the **same change** when practical |
| Still open / deferred | Leave `Status: open` and say so in the user summary |

Do **not** treat `out/crash/*-console-digest.txt` as the casebook (digests are raw breadcrumbs; `cases.md` is the distilled mechanism). Do **not** skip the append because “the chat already explained it”.

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
7. **Console digest (mandatory after analyze/catch):**  
   `powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/archive_console_diag.ps1 -InputLog <catch.log>,<analyze.log> -Label <tag>`  
   Writes `out/crash/<stamp>-<tag>-console-digest.txt` and refreshes `out/crash/LATEST.md`.  
   `run_and_catch.ps1` already calls this after a successful dump.

## Console log diagnosis (with dump)

Do **not** diagnose from stacks alone. Always cross-check **stderr / catch log product lines**:

| Source | What to mine |
| --- | --- |
| `*-catch.log` / process stderr | `LOG_INFO` / `LOG_ERROR` / `LOG_WARNING`, `[sync_inspectors]`, `[fill_attribute_rows]`, `startup:` |
| `*-analyze.log` | `ExceptionCode`, `FAULTING_IP`, `STACK_TEXT`, `FAILURE_BUCKET` |
| `out/crash/*-console-digest.txt` | Filtered union of the above (use for the next session) |
| `out/crash/LATEST.md` | Pointer to the newest digest |

**Agent must:**

1. After catch/analyze, run `archive_console_diag.ps1` (or rely on `run_and_catch` hook).
2. Read the digest + cite matching `LOG_*` / startup lines next to the faulting frame.
3. Prefer product breadcrumbs (`[fill_attribute_rows] SEH soft-fail`, ABI / lock notes) over guessing.

## Parse focus

**Crash log:** `ExceptionCode`, `FAULTING_IP`, `STACK_TEXT`, frames with `file.cc @ line`.

**Hang log:** `!analyze -v -hang` conclusion; threads blocked in `WaitFor*`, `NtWait*`, `MsgWait*`, lock wait; UI/main thread vs worker; `!runaway` (CPU hog vs idle wait); `!locks` owners when present.

**Console:** last `startup:` / `[sync_inspectors]` / `[fill_attribute_rows]` before the fault; SEH soft-fail lines mean the AV was swallowed — still treat as a defect until root cause is fixed.

## Verify

- Product path → `.\build.bat debug harness` then `.\build.bat debug te`.
- Otherwise re-run the same repro until no crash **and** no hang/timeout.
- If entered from `auto-bug-fix`, that skill’s done bar still applies.

**Done bar:** analyze report complete (exception **or** hang stacks + suspected symbol) **and** the repro path no longer fails **and** `cases.md` has an Index row + Case for this root cause (new or updated `Status`). If entered from `auto-bug-fix`, also satisfy harness + te exit 0.

## Evidence before success

Claim fixed **only** after a fresh analyze (or clean run) plus verification command exit 0. Show: mode (crash/hang), cdb path, dump/log paths, key analyze lines, **cases.md case id**, fix summary, verify command + exit code.

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
| "Skip cases.md — we will remember" | Next session will not. Append the case. |
| "Digest is enough" | Digest ≠ mechanism; casebook holds root cause. |

## Red flags — stop

- Declaring success from a partial stack with no hypothesis
- Editing tests to hide a crash/hang
- Shotgun edits without a stated hypothesis
- Opening a feature branch unprompted
- Closing the loop without updating `cases.md`

## Example commands

```powershell
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/find_cdb.ps1
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/analyze_dump.ps1 -DumpPath out\crash\foo.dmp
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/analyze_dump.ps1 -DumpPath out\crash\foo.dmp -Mode Hang
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/run_and_catch.ps1 -ExePath out\Debug\SmartGIS.exe -ExeArgs '--map2d-showcase-il=1'
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/dump_hang.ps1 -ProcessName SmartGIS
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/run_and_hang.ps1 -ExePath out\Debug\SmartGIS.exe -TimeoutSec 180
powershell -NoProfile -File .cursor/skills/auto-diagnose-fix/scripts/archive_console_diag.ps1 -InputLog 'out/crash/a-catch.log;out/crash/a-analyze.log' -Label map2d-av
# Redirected product stderr for live repro (no cdb):
#   SmartGIS.exe --map2d-showcase-il=1 2> out\crash\verify-console.log
.\build.bat debug harness
.\build.bat debug te
```

See [reference.md](reference.md) for symbol paths, exception codes, hang commands, and hard-coded cdb strings.  
See [cases.md](cases.md) for prior root causes (read first, append when done).
