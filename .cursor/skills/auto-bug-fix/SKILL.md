---
name: auto-bug-fix
description: >-
  Use when the user asks to run e2e, product smoke, harness, self-test, fix
  test failures, reproduce a runtime bug until tests pass, or invoke
  /auto-bug-fix; also when they say 端到端, 端到端跑下, 跑测试, 修测试,
  冒烟, 直到 e2e 绿, or 自动修 bug. For this repo: Windows GN via
  build.bat harness / te. Not MSBuild / not SmartGIS.sln. Not compile-only
  (that is auto-build-fix).
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto bug-fix until harness + te green

**自动跑产品 runtime 门禁 / 测试并修根因直至绿**

`auto-build-fix` = compile loop (`build.bat` until no `FAILED` / `error:`).
This skill = **run product harness / tests / smoke, reproduce failures, fix root-cause bugs, re-run until green**. It MAY compile via the same `build.bat` when tests need a build, but the done bar is **tests + harness passing**, not merely ninja exit 0.

**REQUIRED BACKGROUND:** superpowers:systematic-debugging (hypothesis first; do not shotgun).
**REQUIRED SUB-SKILL:** codebase-memory for code lookup before repo-wide Grep.

## Authorization

When this skill is invoked, attached (`@auto-bug-fix` / `/auto-bug-fix`), or followed, the agent **MUST** run harness / tests and iterate until the done bar — do not defer the loop to the user.

## Hard rules — how to run

1. **Entry:** from the **smartgis** repo root, run `.\build.bat debug harness`. Then `.\build.bat debug te` — `harness` compiles Views + GPU (`//:harness`) then runs `testing/tools/loop_runner.py --gate` (`gpu` + `SmartGIS.exe --harness`). `te` compiles `//:test_all` and runs listed `*_test.exe` only. Prefer a single config when iterating.
2. **Engineering management is GN.** Gen roots `out/Debug` + `out/Release`. Aliases: `harness` / `te` / `a`. `e2e` is a synonym of `harness` (no `testing/e2e` tree). Do not invent bare `gn gen` / `ninja` as the primary loop unless `build.bat` is missing/broken. **`out/.build.lock` covers compile only**; `te`/`harness` runners run unlocked after ninja. Exit **3** = compile busy — wait/report, do not bypass. Prefer `build.bat debug …`. See `.cursor/rules/build/build-lock.mdc`.
3. **Do not** use `SmartGIS.sln` / MSBuild / `build.bat sln`. `vs2008/` is leftover only.
4. Product sources are **`src/`**. Hosts: Views (`SmartGIS.exe`) is the destination; leftover MFC is not the default gate. Do not treat WinUI / CEF as the endgame shell.
5. **CBM first** (`user-codebase-memory-mcp`, project `smartgis`, `root_path` `C:/Dev/src/gis/smartgis`). Grep/Glob only if CBM is down, the user named an exact path, or the search is already scoped (`path` required).
6. **Stay on `master`.** No new branches unless the user explicitly asks.
7. Layers: `docs/superpowers/ui-testing.md`. Mapping: `docs/superpowers/mogu-mapping.md`.

```bat
.\build.bat debug harness
.\build.bat debug te
```

## Fix loop (log + runner)

1. Run `.\build.bat debug harness` from repo root (builds chrome / GPU, then `loop_runner --gate`).
2. Run `.\build.bat debug te` (builds `//:test_all`, then listed `*_test.exe`).
3. Parse **`out/Debug/build.log`** (and `out/Release/build.log` if built) *and* the harness / unit transcript:
   - ninja: `FAILED:` / `ninja: build stopped` / `error:` / linker failure
   - gate: `FAIL product gate` / `FAIL gpu` / `FAIL harness` / missing `SmartGisRender.exe` / missing marks (`hwnd-ok`, `pass`, …)
   - unit tests: non-zero exit, `FAILED`, assertion text, `--harness` / `--self-test` codes (`docs/superpowers/ui-testing.md`)
4. **Reproduce** the failing exe or `--harness` (alias `--self-test`) in isolation when the log is ambiguous. Extra suites: `py -3 testing\tools\loop_runner.py --suite <id> --no-build`.
5. **Hypothesis** (systematic-debugging). Fix **root cause** in source or `BUILD.gn`.
   - Not `#if 0`, not deleting / `#ifdef`-hiding tests, not `-Wno-*` unless asked.
6. Re-run the same `.\build.bat debug harness` and `.\build.bat debug te` (or the specific failing exe after a focused rebuild).
7. Repeat until the **done bar**.

**Native crash / hang / `.dmp`:** if the runner shows access violation, crash exit, WER dump, hang/timeout (`rc=124`), or a frozen PE, **follow `auto-diagnose-fix` first** (cdb → crash catch or hang dump / `!analyze` → CBM → root-cause fix), then return here and re-run harness/te until green. Do not treat a crash/hang as a vague “test failed” without a stack.

**Done bar:** `.\build.bat debug harness` exit **0** (`loop_runner --gate` PASS) **and** `.\build.bat debug te` exit **0** (unit tests). Ninja-only exit 0 is **not** enough.

## Evidence before success

Claim green **only** after a fresh successful `.\build.bat debug harness` and `.\build.bat debug te`. Show commands, exit 0, and log / runner confirmation (`PASS`, no remaining `FAIL` / `FAILED` / `error:`).

## LNK1168 (exe locked)

`LNK1168` / cannot open the PE for write: a running `SmartGis*.exe` / test / harness PE holds the file.

1. Stop the locker (Windows: `Stop-Process` / `taskkill` **without** `/F` first; wait; `/F` only if still alive). Use a non-sandbox / system terminal so the signal lands.
2. Relink via the same `.\build.bat debug harness` or `.\build.bat debug te`.
3. Tell the user to **restart** the product if they had it open.

Do not treat LNK1168 as a source bug.

## Hard stops

- Same failure unchanged **3+** times
- Missing VS C++ / **MFC** / Windows SDK / DirectX June 2010 (`d3dx9math.h`)
- Missing gn/ninja / env write failure
- Native crash/hang that cannot be caught/analyzed after following `auto-diagnose-fix` (no cdb / no dump / unreproducible)

MFC component ID (VS 18): `Microsoft.VisualStudio.Component.VC.v145.MFC.x86.x64`.

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**

## Rationalizations (do not)

| Excuse | Reality |
|--------|---------|
| "ninja already exited 0" | Done bar is harness + tests, not compile. |
| "harness built test_all, skip te" | `harness` does not run unit tests. |
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
.\build.bat debug harness
py -3 testing\tools\loop_runner.py --gate --no-build
.\out\Debug\views_unittests.exe
.\out\Debug\SmartGIS.exe --harness
.\out\Debug\SmartGisRender.exe --self-test
```
