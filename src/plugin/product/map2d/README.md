<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.map2d

China carto / align / orthogrid document seed, map print preview, and
**product-owned showcase/harness payloads** (`map2d.scenario.*`). Chrome
(`app/startup` LaunchPolicy + `il.runtime/capability`) is the thin
dispatcher: CLI, HWND pump, mark/BMP sidecars via `plugin::HarnessShell`.
IL sink ops use shared `plugin::contribute_op` /
`plugin::parse_args_json`; Interact exposes one scenario op
`map2d_run(mode=china|align|orthogrid|print)`. Former builtin id
`smartgis.print` is withdrawn; leftover `SmtAMMapPrint` maps here.

## Showcase: IL-first + C++ fallback

Harness suites `browser.map2d.{china,align,orthogrid}` ship atomic IL
scripts (`map2d.seed` + `export_bmp` frames) and pass
`--map2d-showcase-il=1`. Startup (`app/startup/scenario.cc`) runs the
suite script first when that switch is set; otherwise it falls through to
`map2d.scenario.*` (`run_map2d_showcase` in `scenario/run.cc`) — also the
path for Interact `map2d_run` and non-IL argv.

| Id | Role |
| --- | --- |
| `map2d.seed` `{mode}` | `china` / `align` / `orthogrid` document seed |
| `map2d.open_map` / `frame_to` / `frame_fly` / `apply_look` | `Map2dSink` 2D analogue of world3d earth/look (chrome bridges) |
| `map2d.load_hillshade` / `present_gpu` / `export_bmp` | DEM hillshade + GPU present + BMP |
| `map2d.attach_dataset` / `add_standin_layer` | Vector/raster attach + stand-in polygon |
| `print.preview` | Print preview dialog + `PrintComposer` (`print.save`) |
| `map2d.scenario.edit_m0` … `milestones` | GIS steps of `--harness` (alias `--self-test`) |
| `map2d.scenario.china` / `align` / `orthogrid` | C++ showcase body (seed + frame + BMP); IL suites prefer `.il` |
| `map2d.scenario.print` | `--plugin-showcase=print` layout/compose BMP |

Export frames: `china_product` / `map2d_china` (mainland framing),
`map2d_orthogrid` (unit square). Size env: `MAP2D_SHOWCASE_W` / `H`.

GN: `map2d_views` (seed + GIS `scenario/register` + print) is linked by the native DLL. `map2d_harness` (HWND present / FPS / BMP + `hwnd_register.cc` / `register_map2d_showcase`) is linked by `SmartGIS.exe` only. HWND TUs live next to GIS under `scenario/` (not a nested `showcase/` dir).
