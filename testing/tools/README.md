<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# testing/tools ÃÂ¢ÃÂÃÂ harness suite loops

Unified harness under **`harness/`**: **one suite ÃÂ¢ÃÂÃÂ one directory**
(`suite.json` + optional `*.py`). Cross-suite tools live in
**`harness/_shared/`**. Entry (`loop_runner.py`) and shared library (`loop/`)
stay at `testing/tools/`.

## Layout

```
testing/tools/
  loop_runner.py
  loop/
    contract/       # suite.json types + loader
    drive/          # kill / build / env / inproc / OS / plugin.json
    interact/       # os_inject.py
    record/         # hwnd capture + Python recorder
    score/          # bmp.py, marks.py, round.py
    review/         # inspect PNG + visual_review
    runner.py / cli.py / gate.py
    suite.py / kill.py / private_runtime.py  # stable import shims
  harness/
    _shared/
      scripts/vscode/                          # markup / vts preview extensions
      case/{engine,align,util}/                # engine matrix, maplibre align, kill shim
      steps/                                   # shared suite steps (discovery skips _shared/)
    <family>/<suite_id>/
      suite.json                               # contract (id matches dir name)
      *.py                                     # suite-owned Python script (optional)
  README.md
```

| Path | Role |
| --- | --- |
| `loop_runner.py` | Preferred entry |
| `loop/` | Shared library (`contract` / `drive` / `score` / ÃÂ¢ÃÂÃÂ¦) |
| `loop/score/` | BMP + marks gates (`bmp.py`, `marks.py`, `bmp_io.py`) |
| `loop/interact/` | OS inject (`os_inject.py`) |
| `loop/record/` | HWND capture (`hwnd.py`) + recorder (`py_recorder.py`, `os_hook.py`, `il_compact.py`, `py_emit.py`) |
| `harness/<family>/<suite_id>/suite.json` | Suite contract (`plugin/` `browser/` `ui/`) |
| `src/app/views/il.runtime/codegen/session/` | In-process `.py` session (`exec_python`, `script_load`) |
| `harness/_shared/scripts/vscode/` | VS Code/Cursor language extension + install bat |
| `harness/_shared/case/engine/` | Engine matrix / BMP relabel |
| `harness/_shared/case/align/` | MapLibre dual-still align |
| `harness/_shared/case/util/` | Small shims (`kill_showcase.py`) |

## Discovery / script paths

- **Discovery:** recursive `harness/**/suite.json` (skip `_shared/`). Optional
  `{id}.json` when parent dir name equals the stem. Suite `id` in JSON must
  match the directory name.
- **`script` field:** relative to the suite directory first
  (`"browse.py"`), then relative to `harness/` (e.g.
  `"../ui.interact/ui.interact.py"` for a sibling reuse).

## Naming

| Kind | Format | Example |
| --- | --- | --- |
| Suite dir | `harness/<family>/<id>/` | `harness/ui/ui.shell/` |
| Contract | `suite.json` | `harness/browser/browser.map2d.browse/suite.json` |
| Report | `out/<config>/captures/<family>/{id with .ÃÂ¢ÃÂÃÂ_}_loop_report.json` | `captures/ui/ui_shell_loop_report.json` |
| Record | `out/<config>/captures/record/{id}_{stamp}.mp4` or `*_frames/` | `captures/record/browse_ÃÂ¢ÃÂÃÂ¦_frames/` |
| Analysis playback | `out/<config>/captures/analysis/<topic>/` | `captures/analysis/flood/` |
| Showcase / marks | `out/<config>/captures/<family>/` | `captures/plugin/plugin-showcase-world3d.bmp` |
| Interact script | `{id}.py` colocated (or shared path) | `harness/browser/browser.map2d.browse/browser.map2d.browse.py` |

## Preferred entry

```bat
py -3 testing\tools\loop_runner.py --list
py -3 testing\tools\loop_runner.py --gate --no-build
py -3 testing\tools\loop_runner.py --suite browser.map2d.browse --no-build
py -3 testing\tools\loop_runner.py --suite browser.harness --no-build
py -3 testing\tools\loop_runner.py --suite browser.gpu --no-build
py -3 testing\tools\loop_runner.py --suite ui.shell
py -3 testing\tools\loop_runner.py --suite plugin.world3d --no-build
py -3 testing\tools\loop_runner.py --suite plugin.mine --no-build
py -3 testing\tools\loop_runner.py --suite plugin.orthogrid --no-build
py -3 testing\tools\loop_runner.py --record-il
py -3 testing\tools\loop_runner.py --record-il --attach --title "SmartGIS Views"
py -3 testing\tools\loop\record\py_recorder.py --attach --title "SmartGIS Views"
```

`--gate` is the product runtime bar (`browser.gpu` then `browser.harness`). `build.bat debug harness`
compiles `//:harness` then runs the same gate.

C++ `ScenarioRegistry` ids (must match `harness/<family>/<id>/`):

| Family | Suite ids |
| --- | --- |
| `browser/` | `browser.map2d.{browse,china,align,orthogrid}` `browser.world3d.{browse,full,legacy,globe,china_stack}` `browser.harness` `browser.gpu` `browser.console` `browser.input` `browser.campaign` — one IR shape (`horizon`→`view`→`plugin`); `*.browse` = stress; `world3d.full` also applies land/ocean/coast looks; siblings = carto/atmosphere payloads; all `kind=integration`. `browser.gpu` is `SmartGisRender.exe` only. |
| `ui/` | `ui.shell` `ui.data` `ui.scene` `ui.catalog` `ui.plugins` `ui.interact` `ui.interact.os` `ui.interact.combo` |
| `plugin/` | `plugin.world3d` `plugin.world3d.preview` `plugin.print` `plugin.orthogrid` `plugin.orthogrid3d` `plugin.traffic` `plugin.flood` `plugin.stormsurge` `plugin.mine` `plugin.geochem` `plugin.report` |

Stop recording with **Ctrl+Shift+F9** or console **Enter** / **Ctrl+C**. Output under
`out/<config>/captures/record/il_<stamp>/` (`events.jsonl` + `recorded.py`).
Launch sets `SG_DEBUG=1` so DebugAgent can emit `select_map_tab` etc. into the script.
Suite dirs contain **`suite.json` + optional `*.py` only** (no colocated `*_loop.py` / `*.args.json`).
Inline processing args: `run_processing(id="ÃÂ¢ÃÂÃÂ¦", args="{\"k\":\"$var\"}")`.

UI chrome visual suites (BMP + `ui_shell_dark` gates; force `UI_THEME=dark`):

| Suite | `--ui-showcase` | BMP |
| --- | --- | --- |
| `ui.shell` | `shell` | `ui-showcase-shell.bmp` |
| `ui.data` | `data` | `ui-showcase-data.bmp` |
| `ui.scene` | `scene` | `ui-showcase-scene.bmp` |
| `ui.catalog` | `catalog` | `ui-showcase-catalog.bmp` |
| `ui.interact` | `interact` (host + gesture inproc) | `ui-showcase-interact.bmp` |
| `ui.interact.os` | `interact` (same host; OS injects `gesture_script`) | `ui-showcase-interact.bmp` |
| `ui.interact.combo` | `interact` (same host; smoke Escape + combo gesture, marks) | marks |

These share `ui.interact.host.py` as PE entry (`script`) and select a
gesture body via `gesture_script` → `UI_INTERACT_GESTURE_SCRIPT`. The body
runs in-process through `smartgis.script`.

| Script | Suite role |
| --- | --- |
| `ui.interact.host.py` | Shared chrome (`script`); `apply_scenario_panels("interact")` |
| `ui.interact.py` | Gesture body (2D/3D pan·wheel·resize) |
| `ui.interact.combo.py` | Smoke Escape plus `path`/`chord`/`pan_burst`/`wheel_burst` |
| `browser.harness.py` | Integration: HWND tree / orbit / layout then `map2d.scenario.*` |
| `browser.console.py` | Integration: DebugAgent `:help`/`:layers`/`:extent`/`:refresh` + pan bench |

Combo ops: `seq` / `repeat` / `chord` / `path` / `pan_burst` / `wheel_burst`.

## Editor (VS Code / Cursor)

| File | Support |
| --- | --- |
| `*.py` | Harness scripts. ANTLR highlight remains for `src/gis/stat/StatExpr.g4` only. |

```bat
py -3 testing\tools\loop_runner.py --suite browser.map2d.browse --no-build
```

## Other shared case tools

| Script | Role |
| --- | --- |
| `loop/plain_browse_capture.py` | argv=[] china shell + scene3d tab (bare launch / hollow pixels) |
| `harness/_shared/case/align/maplibre_align.py` | Dual product BMP + MapLibre Native stills |
| `harness/_shared/case/align/china_estuary.py` | Yangtze / Yellow mouth: land east edge vs river tip |
| `harness/_shared/case/scene3d/orbit_pair.py` | Far/near `--scene3d-orbit-*` pair, `plugin_scene3d` score |
| `harness/_shared/case/engine/run_engine_shots.py` / `_safe` / `resume_engine_shots.py` | Multi-engine matrix |
| `harness/_shared/case/engine/relabel_engine_bmps.py` | Title-strip annotator |
| `harness/_shared/case/util/kill_showcase.py` | Shim ÃÂ¢ÃÂÃÂ `loop.kill` |

**ÃÂ¦ÃÂÃÂÃÂ¥ÃÂÃÂÃÂ¦ÃÂÃÂ´ÃÂ¦ÃÂÃÂ°:** 2026-10-19
