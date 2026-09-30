<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# testing/tools — harness suite loops

## Layout

| Path | Role |
| --- | --- |
| `loop_runner.py` | Preferred entry |
| `loop/` | Shared kill / build / score |
| `suites/*.json` | Suite contracts |
| `case/` | Case scripts (aliases + engine matrix + align) |

## Preferred entry

```bat
py -3 testing\tools\loop_runner.py --list
py -3 testing\tools\loop_runner.py --suite browse --no-build
py -3 testing\tools\loop_runner.py --suite ui.shell
py -3 testing\tools\loop_runner.py --suite ui.interact --no-build
```

## Case aliases (`case/`)

Old script names under `case/` map via `loop.compat.SCRIPT_ALIASES`:

| Script | Suite |
| --- | --- |
| `case/browse_loop.py` | `browse` |
| `case/input_loop.py` | `input` |
| `case/ui_shot_loop.py` | `ui.shell` |
| `case/map2d_shot_loop.py` | `map2d.china` |
| `case/orthogrid_shot_loop.py` | `map2d.orthogrid` |
| `case/scene3d_shot_loop.py` | `atmosphere.full` |
| `case/legacy_map2d_shot_loop.py` | `legacy.map2d.china` |
| `case/legacy_scene3d_shot_loop.py` | `legacy.scene3d.china` (+ `--d3d` → `.d3d`) |
| `case/pointcloud_load_loop.py` | `pointcloud.load` |

UI chrome visual suites (BMP + `ui_shell_dark` gates; force `SMT_UI_THEME=dark`):

| Suite | `--ui-showcase` | BMP |
| --- | --- | --- |
| `ui.shell` | `shell` | `ui-showcase-shell.bmp` |
| `ui.data` | `data` | `ui-showcase-data.bmp` |
| `ui.scene` | `scene` | `ui-showcase-scene.bmp` |
| `ui.catalog` | `catalog` | `ui-showcase-catalog.bmp` |
| `ui.interact` | `interact` | `ui-showcase-interact.bmp` |

```bat
py -3 testing\tools\case\browse_loop.py --no-build
```

## Other case tools

| Script | Role |
| --- | --- |
| `case/maplibre_align.py` | Dual product BMP + MapLibre Native stills |
| `case/run_engine_shots.py` / `_safe` / `resume_engine_shots.py` | Multi-engine matrix |
| `case/relabel_engine_bmps.py` | Title-strip annotator |
| `case/kill_showcase.py` | Shim → `loop.kill` |

**最后更新:** 2026-09-30
