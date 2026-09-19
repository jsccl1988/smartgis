<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# WinDbg / cdb reference (smartgis)

Companion to [SKILL.md](SKILL.md). Script comments and cdb commands stay English.

## Tool preference

| Tool | Use |
|------|-----|
| `cdb.exe` | Preferred: non-interactive, scriptable (`-c`, `-logo`, `-z`) |
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
| `OutDir` | `out/crash` | dumps + `*-analyze.log` |
| Public symbols | `SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols` | OS / CRT / VC runtime |

## Hard-coded analyze `-c` string

`analyze_dump.ps1` always runs (do not rewrite ad hoc):

```
.sympath+ <abs-PdbDir>; .sympath+ SRV*C:\Symbols*https://msdl.microsoft.com/download/symbols; .reload; !analyze -v; .ecxr; kn; kv; lm vm; q
```

Log: `out/crash/<yyyyMMdd-HHmmss>-analyze.log` via cdb `-logo`.

## run_and_catch outline

1. `cdb -g -G -o <exe> <args>` (go on start; go on exit)
2. On second-chance exception: `.dump /ma <OutDir>/<stamp>.dmp`
3. Quit debugger, then invoke the same analyze path as `analyze_dump.ps1`

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

## Reading the log

1. `FAULTING_IP` / `ExceptionCode` / `ExceptionAddress`
2. `STACK_TEXT` / `STACK_COMMAND` — prefer frames with `foo.cc @ line`
3. `MODULE_NAME` / `IMAGE_NAME` — faulting binary
4. `ANALYSIS_VERSION` / `BUCKET_ID` — triage hints only; still form a source hypothesis

## Symbol troubleshooting

- Wrong bitness: use **x64** cdb for x64 PEs under `out/`.
- Stale PDB: rebuild with `.\build.bat` then re-analyze.
- Private frames show as `module+0xoffset`: confirm matching PDB timestamp next to the PE.

## Related skills

- `auto-bug-fix` — e2e / te green after crash is fixed
- `auto-build-fix` — compile-only when rebuild is needed before symbols exist
- `codebase-memory` — graph-first lookup (project `smartgis`)
