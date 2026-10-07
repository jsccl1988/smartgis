<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-bootstrap — reference

Progressive disclosure. Read when parsing dumps, env, or hang vs slow.

## Pipeline

```
wWinMain
  ParseLaunchOptions
  ContentMain / BrowserMain
    Browser.ctor → Browser.init (InitShell, SeedDocument, AttachViewports, Vista)
    [partial dump: post-init]
    Browser.show → ShowShell → WaitFirstMapPresent   ← 首地图渲染
  maybe_dump_startup_profile  (once; second call in wWinMain is no-op)
  run_loop                    ← stop the process after the dump
```

`wall_ms` prefers outer `wWinMain` / `BrowserMain` duration; else first→last startup-span coverage. File dump matches stderr.

## Env (normative for this skill)

| Env | Role |
| --- | --- |
| `STARTUP_PROFILE=1` | Enable tracing + dump (Release too) |
| `STARTUP_PROFILE_DUMP=<path>` | Text table + sibling chrome JSON |
| `SYNC_CHINA_SEED=1` | China OGR in init/show (not 1ms timer) |
| `SYNC_FIRST_MAP_PRESENT=1` | Block show until first carto present |
| `DISABLE_OOP_RENDER=1` | Locked product measure |
| `DEFER_CHINA_SEED=1` | **Forbidden** for this bench (hides first map) |
| `SKIP_AMBOX_CATALOG` | **Forbidden** (skips WaitFirstMapPresent) |
| `MAP2D_NO_HILLSHADE=1` | **Forbidden** as a 200ms cheat |
| `SYNC_FLYCUBE_INIT=1` | Optional attribution; **not** required (async Init still counted inside the wait pump) |

Debug builds dump even without `STARTUP_PROFILE=1`; still set it so Release/agent runs match.

## Hang vs slow

| Symptom | Meaning |
| --- | --- |
| No `startup_profile.txt`, `partial-post-init` exists | Hung in `Browser.show` / wait pump |
| `WaitFirstMapPresent` ≈ 15000 | Cap hit; first carto never drew |
| `wall_ms` small, no `WaitFirstMapPresent` | Wait skipped — invalid bench |
| `HillshadeBake` inside wait | Real first-map work; optimize shade/cache, do not delete |

Partial dumps do **not** claim the once-slot and do **not** overwrite the final txt.

## Spans (non-exhaustive)

From living §Startup profile: `wWinMain`, `ParseLaunchOptions`, `ContentMain` / `BrowserMain`, `Browser.ctor` / `init` / `show`, `Session.init_tool_sessions`, `PluginShell.*`, `InitShell` (`Widget.init`, `BuildContents`, `SeedDocument` / `try_open_china` / `SeedDocument.ChinaBootstrap`, `BindPresenters`, `AttachViewports`, `MapEdit.FlyCubeAttach` / `FlyCube.Init`, `WireShell`), `ShowShell` / `WaitFirstMapPresent`, `HillshadeBake`, `LoadMarkup`.

## Not this skill

| Want | Use |
| --- | --- |
| Warm map2d frame / FPS | `harness-auto-map2d-frame-opt` |
| Backend×parallel matrix | `harness-auto-map2d-opt` |
| Scene3d timed presents | `harness-auto-scene3d-frame-opt` |
