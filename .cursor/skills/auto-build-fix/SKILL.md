---
name: auto-build-fix
description: >-
  Use when the user asks to build, compile, verify compile, fix build errors
  or warnings, "make it compile", clean `-Werror`/warnings, or ship with a
  green build; also when they say 构建, 编译, 修编译错误, 修警告, 直到成功,
  or 自动构建并修复直至成功为止. For this repo: Windows GN via build.bat
  (mogu-aligned). Not MSBuild / not SmartGIS.sln. Agent runs the build and
  artifact self-check (check_out_artifacts L0/L1/L2) until products are
  consistent; see .cursor/rules/build/agent-may-build.mdc.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto build + fix until green

**自动构建并修复直至成功为止**

Drive the loop through **`build.bat`** at the repo root. Canonical transcripts: **`out/Debug/build.log`** and **`out/Release/build.log`**. Prefer **`build.bat debug …`** for a faster fix loop.

This skill’s deliverable is **correct, link-consistent products** under `out/<config>/` — not merely a green compiler. Half-links (`LNK1168`: new DLLs + old `SmartGIS.exe`) count as **failure**.

## Authorization

When this skill is invoked, attached (`@auto-build-fix`), or followed, the agent **MUST** run builds **and** artifact self-checks, and iterate until the done bar — do not defer compile or closure checks to the user.

Normative allow rule: **`.cursor/rules/build/agent-may-build.mdc`**.

## Hard rules — how to build

1. **Entry:** from the **smartgis** repo root, run `.\build.bat` (or `cmd /c build.bat`). Default builds **both** `out/Debug` and `out/Release`; use `build.bat debug` / `build.bat release` for one config.
2. **Engineering management is GN.** `build.bat` → `gn gen out/Debug|Release` + `ninja -C out/Debug|Release`. Aliases: `m` / `te` / `a` / `b`.
3. **Do not** invent bare `gn gen` / `ninja` as the primary loop unless `build.bat` is missing/broken and you are fixing the entry itself.
4. **Do not** use `SmartGIS.sln` / MSBuild / `build.bat sln` as the fix loop. That track is rejected. `vs2008/` is leftover only.
5. Product sources are **`src/`**.
6. **Tools:** `build\bin\gn.exe` + ninja; `build.bat` writes `out/environment.x64.x64` (copied into each config dir) via PowerShell.
7. Gen roots are **`out/Debug`** and **`out/Release`**. Do not use bare `out/` or `out/Default` as a gen root. Third-party CMake install prefix is **`out/third_party`**. See `.cursor/rules/build/build-output.mdc`.
8. **Build lock:** compile lock is **off by default**; agents must **not** enable `out/.build.lock.on` / `SMARTGIS_BUILD_LOCK=1`. Prefer `build.bat debug <single_target>`. Exit **3** = busy — do non-compile work or wait; do **not** bare ninja. See `.cursor/rules/build/build-lock.mdc`.
9. **Artifact gate (mandatory):** `build.bat` already runs `build/tools/check_out_artifacts.py` (**L0 writable** before ninja, **L1+L2 product** after ninja). After every successful `build.bat` in this skill, the agent **MUST** also re-run the self-check explicitly and treat a non-zero exit as **not green**.

```bat
.\build.bat
.\build.bat debug
.\build.bat debug te
```

Mapping: `docs/superpowers/mogu-mapping.md`.

## Artifact self-check (correct products)

Tool: **`build/tools/check_out_artifacts.py`**.

| Layer | Mode | When | Catches |
| --- | --- | --- | --- |
| **L0** | `--mode writable` | Before ninja (also via `build.bat`) | Locked `SmartGIS.exe` / import DLLs → `LNK1168` half-link |
| **L1** | `--mode product` | After ninja | Stale ninja closure (`ninja -n` pending on exe + import DLLs) |
| **L2** | (inside `product`) | After L1 clean | PE import missing beside exe; **DLL newer than exe** skew |

Configs built in the loop must be checked:

```bat
REM Debug loop (default)
py -3 build\tools\check_out_artifacts.py --out Debug --mode writable
py -3 build\tools\check_out_artifacts.py --out Debug --mode product

REM If Release was built too:
py -3 build\tools\check_out_artifacts.py --out Release --mode writable
py -3 build\tools\check_out_artifacts.py --out Release --mode product
```

Prefer the repo’s Python312 path when `py -3` is unavailable (same as `build.bat`).

### Pre-build (before first `build.bat`)

1. If prior runs left `SmartGIS.exe` / harness alive, **stop them** (gentle then force; non-sandbox shell per **`.cursor/rules/agent-terminal-kill.mdc`**).
2. Run **L0** for the target config(s). On failure: kill listed PIDs, re-run L0, then compile.

### Post-build (after every `build.bat` exit 0)

1. Run **L1+L2** (`--mode product`) for each config that was built.
2. If `product` fails with **pending** / **DLL newer than SmartGIS.exe**:
   - Ensure no lock holders (L0 again).
   - Rebuild: `build.bat debug` (or at least `build.bat debug SmartGIS.exe`).
   - Re-run `product` until exit 0.
3. Do **not** claim green while `product` is non-zero — even if ninja’s last log looked clean.

Partial single-target builds: `build.bat` may pass `--allow-missing-product` when `SmartGIS.exe` is absent. For this skill’s default loop (`build.bat debug` / full), **`SmartGIS.exe` must exist** and **`product` must pass** without relying on allow-missing.

## Fix loop (log-driven)

1. Pre-build: clear lock holders + **L0** (see above).
2. Run `.\build.bat debug` from repo root (or full `.\build.bat` when verifying both configs). Set `SMARTGIS_BUILD_OWNER=auto-build-fix` (or another stable label).
3. Parse **`out/Debug/build.log`** (and **`out/Release/build.log`** if that config was built):
   - `FAILED:` / `ninja: build stopped`
   - every real **`error:`** / linker failure (`LNK1168` = kill holders, rebuild)
   - actionable **`warning:`**
   - `check_out_artifacts` / `out artifact gate failed` lines from `build.bat` stdout
4. Fix **root cause** in source or `BUILD.gn` (not `#if 0`, not `-Wno-*` unless asked). For gate-only failures: unlock PE → rebuild closure, do not paper over with skips.
5. Rebuild via the same `.\build.bat` invocation.
6. Post-build: explicit **`product`** self-check (and `writable` if the failure was lock-related).
7. Repeat until the **done bar**.

**Done bar (all required):**

1. Latest `build.bat` (scoped config) **exit 0**.
2. Built config log(s): no `FAILED:`, no remaining **`error:`** / fatal link errors.
3. Explicit self-check exit **0** for each built config:
   - `check_out_artifacts.py --out <Config> --mode product`
4. Prefer also confirming L0: `--mode writable` exit 0 (no lock holders left that would break the next link).

## Evidence before success

Claim green **only** after:

1. A fresh successful `.\build.bat` (or the scoped `debug`/`release` command used in the loop), **and**
2. Fresh **`check_out_artifacts … --mode product`** exit 0 for every config built.

Show: build command + exit 0, log confirmation (no `FAILED:`), and the `check_out_artifacts: OK … mode=product …` line(s).

## Hard stops

- Missing VS C++ / **MFC** / Windows SDK / DirectX June 2010 (`d3dx9math.h`)
- Same error unchanged **3+** times
- Missing gn/ninja / env write failure
- Artifact gate cannot pass after **3** unlock+rebuild attempts (report lock holders / pending list; stop)

MFC component ID (VS 18): `Microsoft.VisualStudio.Component.VC.v145.MFC.x86.x64`.

## Communication

- Progress in **简体中文**
- Code / identifiers in **English**
