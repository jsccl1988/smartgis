---
name: auto-build-fix
description: >-
  Use when the user asks to build, compile, verify compile, fix build errors
  or warnings, "make it compile", clean `-Werror`/warnings, or ship with a
  green build; also when they say 构建, 编译, 修编译错误, 修警告, 直到成功,
  or 自动构建并修复直至成功为止. For this repo: Windows GN via build.bat
  (mogu-aligned). Not MSBuild / not SmartGIS.sln. Agent runs the build;
  see .cursor/rules/build/agent-may-build.mdc.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto build + fix until green

**自动构建并修复直至成功为止**

Drive the loop through **`build.bat`** at the repo root. Canonical transcripts: **`out/Debug/build.log`** and **`out/Release/build.log`**. Prefer **`build.bat debug …`** for a faster fix loop.

## Authorization

When this skill is invoked, attached (`@auto-build-fix`), or followed, the agent **MUST** run builds and iterate until the done bar — do not defer compile to the user.

Normative allow rule: **`.cursor/rules/build/agent-may-build.mdc`**.

## Hard rules — how to build

1. **Entry:** from the **smartgis** repo root, run `.\build.bat` (or `cmd /c build.bat`). Default builds **both** `out/Debug` and `out/Release`; use `build.bat debug` / `build.bat release` for one config.
2. **Engineering management is GN.** `build.bat` → `gn gen out/Debug|Release` + `ninja -C out/Debug|Release`. Aliases: `m` / `te` / `a` / `b`.
3. **Do not** invent bare `gn gen` / `ninja` as the primary loop unless `build.bat` is missing/broken and you are fixing the entry itself.
4. **Do not** use `SmartGIS.sln` / MSBuild / `build.bat sln` as the fix loop. That track is rejected. `vs2008/` is leftover only.
5. Product sources are **`src/`**.
6. **Tools:** `build\bin\gn.exe` + ninja; `build.bat` writes `out/environment.x64.x64` (copied into each config dir) via PowerShell.
7. Gen roots are **`out/Debug`** and **`out/Release`**. Do not use bare `out/` or `out/Default` as a gen root. Third-party CMake install prefix is **`out/third_party`**. See `.cursor/rules/build/build-output.mdc`.
8. **Build lock:** `build.bat` locks **compile only** (`out/.build.lock`); `te`/`e2e` runners run **unlocked** after. Set `SMARTGIS_BUILD_OWNER`. Prefer `build.bat debug <single_target>`. Exit **3** = busy — do non-compile work or wait; do **not** bare ninja. See `.cursor/rules/build/build-lock.mdc`.

```bat
.\build.bat
.\build.bat debug
.\build.bat debug te
```

Mapping: `docs/superpowers/mogu-mapping.md`.

## Fix loop (log-driven)

1. Run `.\build.bat debug` from repo root (or full `.\build.bat` when verifying both configs).
2. Parse **`out/Debug/build.log`** (and **`out/Release/build.log`** if that config was built):
   - `FAILED:` / `ninja: build stopped`
   - every real **`error:`** / linker failure
   - actionable **`warning:`**
3. Fix **root cause** in source or `BUILD.gn` (not `#if 0`, not `-Wno-*` unless asked).
4. Rebuild via the same `.\build.bat` invocation.
5. Repeat until the **done bar**.

**Done bar:** exit **0**, no `FAILED:`, no remaining **`error:`** in the config log(s) that were built.

## Evidence before success

Claim green **only** after a fresh successful `.\build.bat` (or the scoped `debug`/`release` command used in the loop). Show command, exit 0, and log confirmation.

## Hard stops

- Missing VS C++ / **MFC** / Windows SDK / DirectX June 2010 (`d3dx9math.h`)
- Same error unchanged **3+** times
- Missing gn/ninja / env write failure

MFC component ID (VS 18): `Microsoft.VisualStudio.Component.VC.v145.MFC.x86.x64`.

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**
