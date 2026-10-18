<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# smartgis.map2d

China carto / align / orthogrid document seed and map print preview.
Harness order lives in `testing/tools/harness/_shared/steps/*.py`. Chrome
(`app/startup`) is the thin dispatcher: CLI, HWND pump, mark/BMP sidecars.
Sink ops use shared `plugin::contribute_op`. Former builtin id
`smartgis.print` is withdrawn; leftover `SmtAMMapPrint` maps here.

## Showcase: suite script only

Harness suites `browser.map2d.{china,align,orthogrid}` are the showcase
body (`map2d.seed` + `export_bmp`). Startup runs that script and does not
fall through to a C++ runner. `export_bmp` in glang warms the frame cache,
checks a visible signal, copies `*.keep.bmp`, and runs cold/warm
`present_gpu` on a dedicated HWND. `fps_bench` samples the HUD when
`map2d-fps-bench-ms` is set. Command ids `map2d.scenario.china|align|orthogrid`
remain registered for the catalog; executing them reports that the suite
script owns the run. `map2d.scenario.print` and the GIS payloads
(`edit_m0`, `layers_m1`, `navigate`, `present`, `milestones`) are Python
steps under `testing/tools/harness/_shared/steps/`, called from their suite scripts.

| Id | Role |
| --- | --- |
| `map2d.seed` `{mode}` | `china` / `align` / `orthogrid` document seed |
| `map2d.open_map` / `frame_to` / `frame_fly` / `apply_look` | `Map2dSink` 2D analogue of world3d earth/look (chrome bridges) |
| `map2d.load_hillshade` / `present_gpu` / `export_bmp` | DEM hillshade + GPU present + BMP |
| `map2d.attach_dataset` / `add_standin_layer` | Vector/raster attach + stand-in polygon |
| `print.preview` | Print preview dialog + `PrintComposer` (`print.save`) |
| `testing/tools/harness/_shared/steps/edit_m0.py` … `milestones.py` | GIS steps of `--harness` (alias `--self-test`) |
| `browser.map2d.{china,align,orthogrid}` | Showcase body (`map2d.seed` + `export_bmp`) |
| `testing/tools/harness/_shared/steps/print_page.py` | `--plugin-showcase=print` layout BMP |

Export frames: `china_product` / `map2d_china` (mainland framing),
`map2d_orthogrid` (unit square). Size env: `MAP2D_SHOWCASE_W` / `H`.

GN: `map2d_views` (seed, sink ops, print preview) links the native DLL. GIS step scripts live under `testing/tools/harness/_shared/steps/` and are not a GN target. Showcase BMP / GPU / FPS are `glang.runtime` backend units. Product page chrome for the dialog stays in `print/composer`.
