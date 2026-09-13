---
name: auto-build-fix
description: >-
  Use when the user asks to build, compile, verify compile, fix build errors
  or warnings, “make it compile”, clean `-Werror`/warnings, or ship with a
  green build; also when they say 构建, 编译, 修编译错误, 修警告, 直到成功,
  or 自动构建并修复直至成功为止. For this repo: Windows GN via build.bat
  (mogu-aligned). Not MSBuild / not SmartGIS.sln.
---

# Auto build + fix until green

**自动构建并修复直至成功为止**

Drive the loop through **`build.bat`** at the repo root. Canonical transcript: **`out/build.log`**.

## Authorization

When this skill is invoked, attached (`@auto-build-fix`), or followed, the agent **MUST** run builds and iterate until the done bar — do not defer compile to the user.

## Hard rules — how to build

1. **Entry:** from the **smartgis** repo root, run `.\build.bat` (or `cmd /c build.bat`).
2. **Engineering management is GN.** `build.bat` → `gn gen out` + `ninja -C out`. Aliases: `m` / `te` / `a` / `b`.
3. **Do not** invent bare `gn gen` / `ninja -C out` as the primary loop unless `build.bat` is missing/broken and you are fixing the entry itself.
4. **Do not** use `SmartGIS.sln` / MSBuild / `build.bat sln` as the fix loop. That track is rejected. `vs2008/` is leftover only.
5. Product sources are **`src/`** (not `branches/SmartGis.1.1.vs08`).
6. **Tools:** `build\bin\gn.exe` + ninja; `build.bat` writes `out/environment.x64.x64` via PowerShell (no store Python).
7. **Bazel:** still not used. **GN does consume `third_party/.install`** (mogu/mgis). Library **sources** are `third_party/.src/` (fetch / Gitea / local junction). Do not use sln. Do not use `out/Default`.

```bat
.\build.bat
.\build.bat te
```

Mapping: `docs/build/mogu-mapping.md`.

## Fix loop (log-driven)

1. Run `.\build.bat` from repo root.
2. Parse **`out/build.log`**:
   - `FAILED:` / `ninja: build stopped`
   - every real **`error:`** / linker failure
   - actionable **`warning:`**
3. Fix **root cause** in source or `BUILD.gn` (not `#if 0`, not `-Wno-*` unless asked).
4. Rebuild via the same `.\build.bat`.
5. Repeat until the **done bar**.

**Done bar:** exit **0**, no `FAILED:`, no remaining **`error:`** in **`out/build.log`**.

## Evidence before success

Claim green **only** after a fresh successful `.\build.bat`. Show command, exit 0, and log confirmation.

## Hard stops

- Missing VS C++ / **MFC** / Windows SDK / DirectX June 2010 (`d3dx9math.h`)
- Same error unchanged **3+** times
- Missing gn/ninja / env write failure

MFC component ID (VS 18): `Microsoft.VisualStudio.Component.VC.v145.MFC.x86.x64`.

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**
