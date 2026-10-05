<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# testing/tools — harness suite loops

Unified harness under **`harness/`**: **one suite → one directory**
(`suite.json` + optional `*.il`). Cross-suite tools live in
**`harness/_shared/`**. Entry (`loop_runner.py`) and shared library (`loop/`)
stay at `testing/tools/`.

## Layout

```
testing/tools/
  loop_runner.py
  loop/
    suite.py / runner.py / build.py / process.py / kill.py / compat.py
    score/          # bmp.py, marks.py, bmp_io.py
    interact/       # dsl.py, os_inject.py
    record/         # hwnd.py
  harness/
    _shared/
      scripts/{grammar,gen,vscode,interact}/   # Interact.g4, ANTLR, vscode, orphan .il
      case/{engine,align,util}/                # engine matrix, maplibre align, kill shim
    <family>/<suite_id>/
      suite.json                               # contract (id matches dir name)
      *.il                                     # suite-owned Interact script (optional)
  README.md
```

| Path | Role |
| --- | --- |
| `loop_runner.py` | Preferred entry |
| `loop/` | Shared kill / build / score / suite load |
| `loop/score/` | BMP + marks gates (`bmp.py`, `marks.py`, `bmp_io.py`) |
| `loop/interact/` | Interact DSL + OS inject (`dsl.py`, `os_inject.py`) |
| `loop/record/` | HWND capture (`hwnd.py`) + **IL recorder** (`il_recorder.py`, `os_hook.py`, `il_compact.py`) |
| `harness/<family>/<suite_id>/suite.json` | Suite contract (`ui/` `map2d/` `plugin/` `atmosphere/` `legacy/` `shell/`) |
| `harness/_shared/scripts/grammar/Interact.g4` | Interact DSL grammar (ANTLR 4.13.2) |
| `harness/_shared/scripts/gen/` | ANTLR regen (`gen_interact.bat`, `gen_interact_gn.py`) |
| `harness/_shared/scripts/vscode/` | VS Code/Cursor language extension + install bat |
| `harness/_shared/scripts/interact/` | Shared / orphan `.il` (e.g. `map2d.align`, atmosphere coast/land/ocean) |
| `harness/_shared/case/engine/` | Engine matrix / BMP relabel |
| `harness/_shared/case/align/` | MapLibre dual-still align |
| `harness/_shared/case/util/` | Small shims (`kill_showcase.py`) |

## Discovery / script paths

- **Discovery:** recursive `harness/**/suite.json` (skip `_shared/`). Optional
  `{id}.json` when parent dir name equals the stem. Suite `id` in JSON must
  match the directory name.
- **`script` field:** relative to the suite directory first
  (`"browse.il"`), then relative to `harness/` (e.g.
  `"_shared/scripts/interact/map2d.align.il"` or
  `"../ui.interact/ui.interact.il"` for a sibling reuse).

## Naming

| Kind | Format | Example |
| --- | --- | --- |
| Suite dir | `harness/<family>/<id>/` | `harness/ui/ui.shell/` |
| Contract | `suite.json` | `harness/shell/browse/suite.json` |
| Report | `out/<config>/captures/<family>/{id with .→_}_loop_report.json` | `captures/ui/ui_shell_loop_report.json` |
| Record | `out/<config>/captures/record/{id}_{stamp}.mp4` or `*_frames/` | `captures/record/browse_…_frames/` |
| Analysis playback | `out/<config>/captures/analysis/<topic>/` | `captures/analysis/flood/` |
| Showcase / marks | `out/<config>/captures/<family>/` | `captures/map2d/map2d-showcase-china.bmp` |
| Interact script | `{id}.il` colocated (or shared path) | `harness/shell/browse/browse.il` |

## Preferred entry

```bat
py -3 testing\tools\loop_runner.py --list
py -3 testing\tools\loop_runner.py --suite browse --no-build
py -3 testing\tools\loop_runner.py --suite ui.shell
py -3 testing\tools\loop_runner.py --suite ui.interact --no-build
py -3 testing\tools\loop_runner.py --suite plugin.orthogrid --no-build
py -3 testing\tools\loop_runner.py --suite legacy.scene3d.china.d3d --no-build

REM Record interactions → .il (hybrid OS + optional DebugAgent semantics)
py -3 testing\tools\loop_runner.py --record-il
py -3 testing\tools\loop_runner.py --record-il --attach --title "SmartGIS Views"
py -3 testing\tools\loop\record\il_recorder.py --attach --title "SmartGIS Views"
```

Stop recording with **Ctrl+Shift+F9** or console **Enter** / **Ctrl+C**. Output under
`out/<config>/captures/record/il_<stamp>/` (`events.jsonl` + `recorded.il`).
Launch sets `SG_DEBUG=1` so DebugAgent can emit `select_map_tab` etc. into the IL.
Suite dirs contain **`suite.json` + optional `*.il` only** (no colocated `*_loop.py` / `*.args.json`).
Inline processing args: `run_processing(id="…", args="{\"k\":\"$var\"}")`.

UI chrome visual suites (BMP + `ui_shell_dark` gates; force `UI_THEME=dark`):

| Suite | `--ui-showcase` | BMP |
| --- | --- | --- |
| `ui.shell` | `shell` | `ui-showcase-shell.bmp` |
| `ui.data` | `data` | `ui-showcase-data.bmp` |
| `ui.scene` | `scene` | `ui-showcase-scene.bmp` |
| `ui.catalog` | `catalog` | `ui-showcase-catalog.bmp` |
| `ui.interact` | `interact` (DSL inproc) | `ui-showcase-interact.bmp` |
| `ui.interact.os` | `interact` (DSL OS inject; script → `../ui.interact/ui.interact.il`) | `ui-showcase-interact.bmp` |
| `ui.interact.smoke` | `interact` (minimal DSL, marks) | marks |
| `ui.interact.combo` | `interact` (combo DSL demo, marks) | marks |

Suite-owned Interact scripts live next to `suite.json`. Grammar:
`harness/_shared/scripts/grammar/Interact.g4` (ANTLR gen under `out/*/gen`).

| Script | Suite | Role |
| --- | --- | --- |
| `ui.interact.il` | `ui.interact` / `ui.interact.os` | Full chrome + map combo |
| `ui.interact.smoke.il` | `ui.interact.smoke` | Minimal parse/exec smoke |
| `ui.interact.combo.il` | `ui.interact.combo` | Mid-weight `path`/`chord`/`pan_burst`/`wheel_burst` demo |

Combo ops: `seq` / `repeat` / `chord` / `path` / `pan_burst` / `wheel_burst`.

## Editor (VS Code / Cursor)

| File | Support |
| --- | --- |
| `Interact.g4` | Recommend **ANTLR4 grammar syntax support** (`mike-lischke.vscode-antlr4`) — highlight, Go to Definition / References on rules. See `.vscode/README.md`. |
| `*.il` | `testing\tools\harness\_shared\scripts\vscode\install_vscode_interact.bat` — TextMate highlight + F12 to `Interact.g4` / `interact_dsl.cc`. |

```bat
py -3 testing\tools\loop_runner.py --suite browse --no-build
```

## Other shared case tools

| Script | Role |
| --- | --- |
| `harness/_shared/case/align/maplibre_align.py` | Dual product BMP + MapLibre Native stills |
| `harness/_shared/case/engine/run_engine_shots.py` / `_safe` / `resume_engine_shots.py` | Multi-engine matrix |
| `harness/_shared/case/engine/relabel_engine_bmps.py` | Title-strip annotator |
| `harness/_shared/case/util/kill_showcase.py` | Shim → `loop.kill` |

**最后更新:** 2026-09-30
