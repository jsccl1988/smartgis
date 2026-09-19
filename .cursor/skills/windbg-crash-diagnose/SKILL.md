---
name: windbg-crash-diagnose
description: >-
  Use when diagnosing Windows native crashes, minidumps, access violations, or
  faulting stacks with WinDbg/cdb; also when the user says crash, 崩溃, dmp,
  AV, !analyze, blue-screen usermode, 自动诊断 crash, or hands over a .dmp.
  For this repo: cdb preferred over windbg GUI; PDBs under out/; logs under
  out/crash/; CBM project smartgis; after root-cause fix verify with
  build.bat e2e / te (or the same repro) and hand back to auto-bug-fix when
  entered from that loop.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# WinDbg / cdb crash diagnose (smartgis)

**Windows native crash → dump → `!analyze -v` → CBM → 修根因 → 验证**

Prefer **`cdb.exe`** (non-interactive Debugging Tools for Windows). Do **not** open the WinDbg GUI unless cdb is unavailable and the user explicitly asks.

**REQUIRED BACKGROUND:** superpowers:systematic-debugging (hypothesis first; do not shotgun).
**REQUIRED SUB-SKILL:** codebase-memory for code lookup (project `smartgis`) before repo-wide Grep.
**SIBLING:** when entered from `auto-bug-fix`, return to that skill’s e2e/te done bar after the crash no longer reproduces.

## Authorization

When this skill is invoked, attached (`@windbg-crash-diagnose`), or triggered by a crash / `.dmp` / AV, the agent **MUST** run find → dump/catch → analyze → locate → fix → verify — do not hand the debugger session back to the user.

## Hard rules

1. **Tool:** `scripts/find_cdb.ps1` first. Missing cdb → **hard stop**; tell the user to install **Debugging Tools for Windows** (Windows SDK). Prefer `cdb.exe` over `windbg.exe`.
2. **Defaults (this repo):** `PdbDir=out`, `OutDir=out/crash`, Microsoft public symbol server. Do not invent a second PDB root.
3. **Scripts only for `-c`:** use `analyze_dump.ps1` / `run_and_catch.ps1`; do not hand-roll fragile cdb command strings.
4. **CBM first** (`user-codebase-memory-mcp`, project `smartgis`, `root_path` `C:/Dev/src/gis/smartgis`). Grep only if CBM is down, the user named an exact path, or the search is already scoped (`path` required).
5. **Stay on `master`.** No new branches unless the user explicitly asks.
6. **Kill leftovers** (cdb / target PE) via non-sandbox / system terminal per `agent-terminal-kill` (gentle stop first, `/F` only if still alive).
7. Fix **root cause**. Not `#if 0`, not deleting / hiding tests, not stubbing asserts.

## Diagnose loop

```
crash / .dmp / AV / failing PE
  → find_cdb.ps1
  → have .dmp?  yes → analyze_dump.ps1
                 no  → run_and_catch.ps1 (then analyze)
  → parse log (exception, module, !analyze, source frames)
  → CBM search_graph / get_code_snippet (project smartgis)
  → systematic-debugging fix
  → verify: .\build.bat e2e + .\build.bat te  OR  same repro until no crash
```

1. **Find tool:** from repo root,
   `pwsh -NoProfile -File .cursor/skills/windbg-crash-diagnose/scripts/find_cdb.ps1`
2. **Have dump:**  
   `pwsh -NoProfile -File .cursor/skills/windbg-crash-diagnose/scripts/analyze_dump.ps1 -DumpPath <path.dmp>`  
   (defaults: `-PdbDir out` `-OutDir out/crash`)
3. **No dump:** catch under cdb, e.g.  
   `pwsh -NoProfile -File .cursor/skills/windbg-crash-diagnose/scripts/run_and_catch.ps1 -ExePath out\SmartGisViews.exe -ExeArgs '--self-test'`  
   Timeout / cannot reproduce → **hard stop**.
4. **Parse** `out/crash/*-analyze.log`: exception code, faulting module, `!analyze` conclusion, stack frames with source file:line when present.
5. **CBM** → open only the cited frames; form a hypothesis; fix root cause.
6. **Verify:** product path → `.\build.bat e2e` then `.\build.bat te`. Otherwise re-run the same crash command until it no longer crashes. If this skill was entered from `auto-bug-fix`, that skill’s done bar still applies.

**Done bar:** analyze report complete (exception + stack + suspected symbol) **and** the repro path no longer crashes. If entered from `auto-bug-fix`, also satisfy e2e + te exit 0.

## Evidence before success

Claim fixed **only** after a fresh analyze (or clean run) plus verification command exit 0. Show: cdb path, dump/log paths, key `!analyze` lines, fix summary, verify command + exit code.

## Hard stops

- `cdb.exe` not found (install Debugging Tools for Windows)
- Cannot reproduce / catch timeout with no dump
- Same failure unchanged **3+** times after hypothesized fixes
- Missing PDBs under `out/` for the faulting binary (rebuild first via `.\build.bat` / `auto-build-fix`)

## Communication

- Progress in **简体中文**
- Code / identifiers / cdb commands in **English**

## Rationalizations (do not)

| Excuse | Reality |
|--------|---------|
| "Open WinDbg GUI for the user" | Use `cdb` + scripts; GUI is last resort. |
| "Ask the user to paste !analyze" | Agent runs `analyze_dump.ps1`. |
| "Grep the whole repo first" | CBM `search_graph` first. |
| "Quick `#if 0` around the AV" | Symptom patch. Find root cause. |
| "e2e green without re-running crash" | Repro / e2e+te must prove the crash is gone. |

## Red flags — stop

- Declaring success from a partial stack with no hypothesis
- Editing tests to hide a crash
- Shotgun edits without a stated hypothesis
- Opening a feature branch unprompted

## Example commands

```powershell
pwsh -NoProfile -File .cursor/skills/windbg-crash-diagnose/scripts/find_cdb.ps1
pwsh -NoProfile -File .cursor/skills/windbg-crash-diagnose/scripts/analyze_dump.ps1 -DumpPath out\crash\foo.dmp
pwsh -NoProfile -File .cursor/skills/windbg-crash-diagnose/scripts/run_and_catch.ps1 -ExePath out\SmartGisViews.exe -ExeArgs '--self-test'
.\build.bat e2e
.\build.bat te
```

See [reference.md](reference.md) for symbol paths, exception codes, and the hard-coded cdb `-c` string.
