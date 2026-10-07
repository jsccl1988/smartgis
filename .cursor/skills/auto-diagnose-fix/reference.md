<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# cdb reference (auto-diagnose-fix / smartgis)

Companion to [SKILL.md](SKILL.md). Script comments and cdb commands stay English.

## Tool preference

| Tool | Use |
|------|-----|
| `cdb.exe` | Preferred: non-interactive, scriptable (`-c`, `-logo`, `-z`, `-p`) |
| `windbg.exe` | GUI only if cdb missing and user insists |

Search order used by `scripts/find_cdb.ps1`:

1. `C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe`
2. `C:\Program Files\Windows Kits\10\Debuggers\x64\cdb.exe`
3. Newest `Windows Kits\10\Debuggers\x64\cdb.exe` under Program Files*
4. `cdb.exe` on `PATH`

Install: Visual Studio Installer / Windows SDK → **Debugging Tools for Windows**.

## Defaults (this repo)

| Param | Default | Notes |
|-------|---------|--------|
| `PdbDir` | `out` | GN/ninja PDBs next to PEs |
| `OutDir` | `out/crash` | dumps + `*-analyze.log` / `*-hang.log` |
| Public symbols | `SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols` | OS / CRT / VC runtime |

## Hard-coded crash analyze string

`analyze_dump.ps1 -Mode Crash` (default) always runs (do not rewrite ad hoc):

```
.sympath+ <abs-PdbDir>
.sympath+ SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols
.reload
!analyze -v
.ecxr
kn
kv
lm vm
q
```

Log: `out/crash/<yyyyMMdd-HHmmss>-analyze.log` via cdb `-logo`.

## Hard-coded hang analyze string

`analyze_dump.ps1 -Mode Hang` and hang path after `dump_hang.ps1`:

```
.sympath+ <abs-PdbDir>
.sympath+ SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols
.reload
!analyze -v -hang
~*kv
!runaway
!locks
lm vm
q
```

`!locks` may print errors without the right extension — still keep it; stacks from `~*kv` are enough to proceed.

## run_and_catch outline (crash)

1. `cdb -g -G -o <exe> <args>` (go on start; go on exit)
2. On second-chance exception: `.dump /ma <OutDir>/<stamp>.dmp`
3. Quit debugger, then invoke crash analyze path

## dump_hang / run_and_hang outline

1. **dump_hang:** `cdb -p <pid>` → `.dump /ma` → `q` → offline hang analyze
2. **run_and_hang:** start PE **without** debugger → wait `TimeoutSec` → if still alive, `dump_hang` on that PID; if exited early with crash-like code, fall back to existing dump / `run_and_catch` guidance

Kill residual `cdb` / target PE with a non-sandbox terminal (`Stop-Process` / `taskkill` without `/F` first).

## Common exception codes

| Code | Meaning |
|------|---------|
| `0xC0000005` | Access violation |
| `0xC00000FD` | Stack overflow |
| `0xC0000094` | Integer divide by zero |
| `0xC0000409` | Stack buffer overrun / fast fail |
| `0xE06D7363` | C++ exception (`msc`) |
| `0x80000003` | Breakpoint (often intentional) |

## Reading crash logs

1. `FAULTING_IP` / `ExceptionCode` / `ExceptionAddress`
2. `STACK_TEXT` / `STACK_COMMAND` — prefer frames with `foo.cc @ line`
3. `MODULE_NAME` / `IMAGE_NAME` — faulting binary
4. `ANALYSIS_VERSION` / `BUCKET_ID` — triage hints only; still form a source hypothesis

## Reading hang logs

1. `!analyze -v -hang` summary / hang bucket if present
2. `~*kv` — find UI/main thread and waiters (`WaitForSingleObject`, `MsgWaitForMultipleObjects`, condition variables, GPU sync)
3. `!runaway` — CPU-burning livelock vs idle deadlock
4. `!locks` — critical-section owners when available
5. Prefer product frames with source line over ntdll wait stubs when forming the hypothesis

## Symbol troubleshooting

- Wrong bitness: use **x64** cdb for x64 PEs under `out/`.
- Stale PDB: rebuild with `.\build.bat` then re-analyze.
- Private frames show as `module+0xoffset`: confirm matching PDB timestamp next to the PE.
- Hang attach fails: process may have exited, or needs a non-sandbox shell / matching integrity level.

## Related skills

- `auto-bug-fix` — harness / te green after crash/hang is fixed
- `auto-build-fix` — compile-only when rebuild is needed before symbols exist
- `codebase-memory` — graph-first lookup (project `smartgis`)
