---
name: harness-auto-bootstrap
description: >-
  Loop-profiles SmartGisViews cold start through first map present
  (startup_profile cat=startup spans) and drives targeted optimizations until
  wall_ms is within 200ms. Use when the user invokes /harness-auto-bootstrap,
  or says 启动到首地图, 启动->首地图渲染, startup_profile, WaitFirstMapPresent,
  SMT_STARTUP_PROFILE, 冷启动 200ms, or asks to profile-then-optimize bootstrap
  until first carto present.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto bootstrap（启动 → 首地图）

**loop profile 启动->首地图渲染，并针对性的优化直至降至200ms以内**

Closed loop: build → product cold launch with startup dump → parse hottest `cat=startup` span → CBM → one-hypothesis fix → rebuild → re-bench → until **done bar** / hard stop.

Sibling of `harness-auto-map2d-frame-opt` (warm per-frame StaticReuse). This skill owns **process start → first china carto present**, not FPS / `present_gpu_warm_ms`.

## Authorization

When this skill is invoked, attached (`@harness-auto-bootstrap` / `/harness-auto-bootstrap`), or followed, the agent **MUST** run the startup bench, iterate fixes, and re-bench — do not defer the loop to the user.

## Hard rules

1. **Locked path:** product `SmartGIS.exe` with **no** `--*-showcase` / `--self-test`. First map = **China carto on Map Edit** (sync seed + sync first present).
2. **Fair wall:** metric is `[startup-profile] wall_ms` covering `wWinMain`/`BrowserMain` **including** `WaitFirstMapPresent`. Do **not** fake 200ms by omitting the wait, deferring China, skipping hillshade, forcing GDI overlay, or using `--ui-showcase=shell`.
3. Prefer **`build.bat debug src/app/views:views`**. Compile lock stays **OFF**. Stay on **`master`**.
4. CBM first (`user-codebase-memory-mcp`, project `smartgis`) before repo-wide Grep.
5. Hypothesis first — one hot startup span per iteration; no shotgun edits.
6. After dump, **stop the exe** (gentle `Stop-Process` / `taskkill` without `/F`, then `/F` only if still alive). Use a non-sandbox shell so the signal lands.

## Locked profile + budget (Debug)

| Axis | Value |
| --- | --- |
| Entry | `out\Debug\SmartGIS.exe` (plain launch) |
| Seed | `SMT_SYNC_CHINA_SEED=1` (China in init/show, not post-show timer) |
| First map | `SMT_SYNC_FIRST_MAP_PRESENT=1` (`WaitFirstMapPresent` in the dump) |
| Trace | `SMT_STARTUP_PROFILE=1` |
| Dump | `SMT_STARTUP_PROFILE_DUMP=out\Debug\log\startup_profile.txt` |
| OOP | `SMT_DISABLE_OOP_RENDER=1` (same as living §Startup profile product measure) |
| **Done** | `wall_ms` ≤ **200** |
| Visual | first present drew china carto (`WaitFirstMapPresent` returned before 15s cap) |

Living §: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` §Startup profile.

`WaitFirstMapPresent` success = GPU token + `last_gpu_present_drew` + `layout_build_count>0` (or content SoT blit). Timeout (~15s) = bench **FAIL**, not a 200ms win.

## Entry commands

From repo root:

```bat
.\build.bat debug src/app/views:views
set SMT_DISABLE_OOP_RENDER=1
set SMT_SYNC_CHINA_SEED=1
set SMT_SYNC_FIRST_MAP_PRESENT=1
set SMT_STARTUP_PROFILE=1
set SMT_STARTUP_PROFILE_DUMP=out\Debug\log\startup_profile.txt
if exist out\Debug\log\startup_profile.txt del out\Debug\log\startup_profile.txt
start /wait /b out\Debug\SmartGIS.exe
```

If `start /wait` hangs in the message loop: poll `out\Debug\log\startup_profile.txt` until the `[startup-profile] wall_ms` line exists (dump runs at end of `Browser.show`), then stop `SmartGIS.exe`. Cap wait ~20s (present wait is 15s).

Read:

```bat
type out\Debug\log\startup_profile.txt
type out\Debug\log\startup_profile.partial-post-init.txt
```

Optional chrome JSON sibling: `startup_profile.json` (or `.chrome.json`). Deep spans: `SMT_TRACE=1`.

Artifacts:

| Artifact | Role |
| --- | --- |
| `out/Debug/log/startup_profile.txt` | Final phase table + `wall_ms` |
| `out/Debug/log/startup_profile.json` | Full process_trace chrome buffer |
| `startup_profile.partial-post-init.txt` | Pre-show snapshot (hang diagnosis) |
| stderr `[startup-profile]` | Same table if file missing |

## Workflow

Copy and track:

```
Bootstrap opt progress:
- [ ] 1. Build SmartGIS
- [ ] 2. Baseline: plain launch + sync China + WaitFirstMapPresent
- [ ] 3. Parse startup_profile.txt → pick hottest span on 启动→首地图
- [ ] 4. CBM → root-cause in shell / content bootstrap / map2d first present
- [ ] 5. Fix (one hypothesis) → rebuild → re-bench
- [ ] 6. Repeat until wall_ms≤200 or hard stop
- [ ] 7. Emit before/after phase table
```

### Step 1 — Build

```bat
.\build.bat debug src/app/views:views
```

Missing `out\Debug\SmartGIS.exe` → rebuild; do not skip.

### Step 2 — Baseline bench

Run the entry commands. Confirm the dump contains **`WaitFirstMapPresent`**. If that row is absent, the bench did not measure 首地图渲染 — fix env and re-run before optimizing.

### Step 3 — Parse hot phase

Fill from the table (`offset_ms` / `dur_ms`; `wall_ms` is the gate):

| Span | Baseline | After |
| --- | ---: | ---: |
| `wall_ms` | | |
| `wWinMain` / `BrowserMain` | | |
| `ParseLaunchOptions` | | |
| `ContentMain` | | |
| `Browser.ctor` / `Browser.init` | | |
| `Session.init_hosts` | | |
| `PluginShell.*` | | |
| `InitShell` / `Widget.init` / `BuildContents` | | |
| `SeedDocument` / `try_open_china` / `ChinaBootstrap` | | |
| `BindPresenters` / `AttachViewports` | | |
| `MapEdit.FlyCubeAttach` / `FlyCube.Init` | | |
| `LoadMarkup` | | |
| `ShowShell` | | |
| `WaitFirstMapPresent` | | |
| `HillshadeBake` | | |

**Hot span pick:** largest `dur_ms` that still sits on process-start → first present (ignore nested children of an already-chosen parent unless the parent is only a wrapper). If `WaitFirstMapPresent` ≈ 15s → first present never completed; debug hang, do not “optimize” other spans.

### Step 4 — Fix targets (by span)

| Hot span | Prefer code under |
| --- | --- |
| `SeedDocument` / `ChinaBootstrap` / `try_open_china` | `content/browser/bootstrap/**`, `content/browser/document/**` |
| `HillshadeBake` | map2d DEM shade; cache / defer until extent (do not delete shade) |
| `FlyCube.Init` / attach | `ui/views/map/viewport/flycube*`, MapEdit attach |
| `AttachViewports` / `BindPresenters` | `app/views/shell/ui/browser_view.*`, presenters |
| `LoadMarkup` | markup loader cache; lazy Diagnostic Tools tabs |
| `Session.init_hosts` / OOP | `content/browser/session/**` — keep OOP off for this bench |
| `WaitFirstMapPresent` (real work, not timeout) | first china layout + GPU present: `content/.../map2d/**`, `Map2dFrameCache` |
| `PluginShell.*` | defer LoadLibrary; do not scan plugins “for the profile” |
| `Browser.ctor` / `init` / `Widget.init` | shell assembly; lazy inspectors |

Non-goals: strip China/hillshade; leftover MFC `SmartGIS-Legacy.exe` as the product bar; map2d warm FPS (use `harness-auto-map2d-frame-opt`); scene3d first present (out of scope).

### Step 5 — Loop

1. State hypothesis (one sentence) tied to the hot span.
2. Patch root cause.
3. Rebuild Views.
4. Re-run the same locked-profile bench (delete old `startup_profile.txt` first).
5. Update the before/after table.
6. Stop on **done bar** or **hard stop**.

### Done bar

Debug product cold start, sync China, sync first present:

- `wall_ms` ≤ **200**
- dump includes `WaitFirstMapPresent` that finished **before** the 15s cap
- China carto still the first map (no demo-only seed)

### Hard stops

- Same hot span unchanged **3+** iterations
- First present timeout / blank map after a “perf” change
- Crash / AV → `windbg-crash-diagnose`, then return here

## Communication

- Progress and tables in **简体中文**
- Paths / env / span names in **English** identifiers
- Lead with before/after `wall_ms` + hottest span + next hypothesis

## Related

- Living §: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md` §Startup profile
- Dump: `src/base/trace/diag/startup_profile.*`
- Wait: `BrowserView::show_shell` (`SMT_SYNC_FIRST_MAP_PRESENT`)
- Warm-frame sibling: `.cursor/skills/harness-auto-map2d-frame-opt/SKILL.md`
- Detail: [reference.md](reference.md)
