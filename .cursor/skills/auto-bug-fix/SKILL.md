---
name: auto-bug-fix
description: >-
  Use when the user asks to run e2e, product smoke, exe_smoke, self-test, fix
  test failures, reproduce a runtime bug until tests pass, or invoke
  /auto-bug-fix; also when they say 端到端, 端到端跑下, 跑测试, 修测试,
  冒烟, 直到 e2e 绿, or 自动修 bug. For this repo: Windows GN via
  build.bat e2e / te. Not MSBuild / not SmartGIS.sln. Not compile-only
  (that is auto-build-fix).
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto bug-fix until e2e green

**自动跑端到端 / 测试并修根因直至绿**

`auto-build-fix` = compile loop (`build.bat` until no `FAILED` / `error:`).
This skill = **run product e2e / tests / smoke, reproduce failures, fix root-cause bugs, re-run until e2e green**. It MAY compile via the same `build.bat` when tests need a build, but the done bar is **tests/e2e passing**, not merely ninja exit 0.

**REQUIRED BACKGROUND:** superpowers:systematic-debugging (hypothesis first; do not shotgun).
**REQUIRED SUB-SKILL:** codebase-memory for code lookup before repo-wide Grep.

## Authorization

When this skill is invoked, attached (`@auto-bug-fix` / `/auto-bug-fix`), or followed, the agent **MUST** run e2e / tests and iterate until the done bar — do not defer the loop to the user.

## Hard rules — how to run

1. **Entry:** from the **smartgis** repo root, run `.\build.bat e2e`. Then `.\build.bat te` — `e2e` compiles `//:test_all` but **does not run** unit tests; it only runs `out\exe_smoke.exe --require-all`.
2. **Engineering management is GN.** `out/` only. Aliases: `e2e` / `te` / `a`. Do not invent bare `gn gen` / `ninja -C out` as the primary loop unless `build.bat` is missing/broken.
3. **Do not** use `SmartGIS.sln` / MSBuild / `build.bat sln`. `vs2008/` is leftover only.
4. Product sources are **`src/`**. Hosts: Views (`SmartGisViews.exe`) is the destination; leftover MFC (`SmartGis.exe`) is process smoke only. Do not treat WinUI / CEF as the endgame shell.
5. **CBM first** (`user-codebase-memory-mcp`, project `smartgis`, `root_path` `C:/Dev/src/gis/smartgis`). Grep/Glob only if CBM is down, the user named an exact path, or the search is already scoped (`path` required).
6. **Stay on `master`.** No new branches unless the user explicitly asks.
7. Layers: `docs/build/ui-testing.md`. Mapping: `docs/build/mogu-mapping.md`.

```bat
.\build.bat e2e
.\build.bat te
```

## Fix loop (log + runner)

1. Run `.\build.bat e2e` from repo root (builds chrome / GPU / `exe_smoke`, then `--require-all`).
2. Run `.\build.bat te` (builds `//:test_all`, then listed `*_test.exe` + `exe_smoke`).
3. Parse **`out/build.log`** *and* the test runner / `exe_smoke` transcript:
   - ninja: `FAILED:` / `ninja: build stopped` / `error:` / linker failure
   - `exe_smoke`: `FAIL  <exe>` (exit / timeout / CreateProcess / `--require-all` missing PE). `SKIP` is not green under `--require-all`.
   - unit tests: non-zero exit, `FAILED`, assertion text, `--self-test` codes (`docs/build/ui-testing.md`)
4. **Reproduce** the failing exe or `--self-test` in isolation when the log is ambiguous.
5. **Hypothesis** (systematic-debugging). Fix **root cause** in source or `BUILD.gn`.
   - Not `#if 0`, not deleting / `#ifdef`-hiding tests, not `-Wno-*` unless asked.
6. Re-run the same `.\build.bat e2e` and `.\build.bat te` (or the specific failing exe after a focused rebuild).
7. Repeat until the **done bar**.

**Native crash / AV / `.dmp`:** if the runner shows access violation, crash exit, WER dump, or a hung PE that dies under debugger, **follow `windbg-crash-diagnose` first** (cdb → dump/`!analyze` → CBM → root-cause fix), then return here and re-run e2e/te until green. Do not treat a crash exit as a vague “test failed” without a stack.

**Done bar:** `.\build.bat e2e` exit **0** (`exe_smoke --require-all` all `PASS`) **and** `.\build.bat te` exit **0** (unit tests + smoke). Ninja-only exit 0 is **not** enough.

## Evidence before success

Claim green **only** after a fresh successful `.\build.bat e2e` and `.\build.bat te`. Show commands, exit 0, and log / runner confirmation (`PASS`, no remaining `FAIL` / `FAILED` / `error:`).

## LNK1168 (exe locked)

`LNK1168` / cannot open the PE for write: a running `SmartGis*.exe` / test / `exe_smoke` holds the file.

1. Stop the locker (Windows: `Stop-Process` / `taskkill` **without** `/F` first; wait; `/F` only if still alive). Use a non-sandbox / system terminal so the signal lands.
2. Relink via the same `.\build.bat e2e` or `.\build.bat te`.
3. Tell the user to **restart** the product if they had it open.

Do not treat LNK1168 as a source bug.

## Hard stops

- Same failure unchanged **3+** times
- Missing VS C++ / **MFC** / Windows SDK / DirectX June 2010 (`d3dx9math.h`)
- Missing gn/ninja / env write failure
- CEF Binary Dist missing when the user explicitly required `SmartGisCef.exe` (default `e2e` leaves CEF off)
- Native crash that cannot be caught/analyzed after following `windbg-crash-diagnose` (no cdb / no dump / unreproducible)

MFC component ID (VS 18): `Microsoft.VisualStudio.Component.VC.v145.MFC.x86.x64`.

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**

## Rationalizations (do not)

| Excuse | Reality |
|--------|---------|
| "ninja already exited 0" | Done bar is e2e + tests, not compile. |
| "e2e built test_all, skip te" | `e2e` does not run unit tests. |
| "Disable / skip the failing test" | Hide tests is forbidden. Fix the product. |
| "Quick `#if 0` / stub the assert" | Symptom patch. Find root cause. |
| "New branch for the fix" | Stay on `master` unless the user asks. |
| "Grep the whole repo first" | CBM `search_graph` first. |

## Red flags — stop

- Declaring success from ninja / `out/build.log` alone
- Changing tests to match a broken product
- Shotgun edits without a stated hypothesis
- Opening a feature branch unprompted

## Example commands (isolation)

```bat
.\build.bat e2e
.\out\exe_smoke.exe --require-all
.\out\views_unittests.exe
.\out\SmartGisViews.exe --self-test
```
