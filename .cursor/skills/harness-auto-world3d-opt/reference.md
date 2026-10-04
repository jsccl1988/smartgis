<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-world3d-opt — reference

## Matrix model

```
同等渲染物料
        │
        ├─ FlyCube/DX12  (SmartGisViews --plugin-showcase=world3d)
        │     SMT_PLUGIN_WORLD3D_PERF_BARE=1  (DEM-only; no atmo/overlay)
        │     parallel ∈ {prep_default, prep_0, prep_on}
        │     role = perf
        │
        ├─ Null          (same Views entry; GPU-off; **full** product materials)
        │     role = smoke  ← not a performance peer
        │
        ├─ Leftover/GL   (row gl_leftover; SmartGis --scene3d-showcase china)
        │     SMT_STEREO_API=OpenGL
        │     role = perf
        │
        └─ Leftover/D3D11 (row d3d_leftover; SmartGis --scene3d-showcase china)
              SMT_STEREO_API=Direct3D
              role = perf

GDI: omitted from the 3D matrix (shell/2D device; not a GPU peer).
```

## Perf bare (`SMT_PLUGIN_WORLD3D_PERF_BARE=1`)

| Off | Still on |
| --- | --- |
| sky / ocean / cloud / fog | China DEM + orbit |
| pointcloud overlay (load skipped) | FlyCube present / BMP |
| warmup `pump_messages(50)` | phase JSON + `ms_per_present` |

## Fairness

| Claim | OK? |
| --- | --- |
| Compare FlyCube prep on vs off on **ms_per_present** / phases | Yes |
| Rank leftover vs FlyCube by process `wall_ms` | **No** — use `ms_per_present` |
| Require BMP for gl_leftover / d3d_leftover (not N/A) | Yes |
| Put `null` / `gdi` in the performance table | **No** |
| Treat leftover ms/present as identical GPU work to FlyCube | **No** (different seed) |
| Fabricate BMP when SmartGis.exe missing | **No** — build then re-run |

## Perf JSON

| Row family | Leaf | Fields |
| --- | --- | --- |
| FlyCube world3d | `captures/plugin/plugin-showcase-world3d-perf.json` | Primary: warm `ms_per_present` (`discard_cold=1`, n=5). Warm phase: `mesh_ms`…`present_swap_ms`. Cold attribution: top-level `dem_load_ms` / `tess_ms` / `hypso_ms` / `upload_ms` / `pso_ms` plus nested `cold_phase` blob (first present). |
| Leftover GL/D3D | `captures/legacy/legacy-scene3d-showcase-china-{gl,d3d}-perf.json` (+ generic `…-showcase-perf.json`) | `ms_per_present`, `present_ms`, `present_count` |

Matrix copies the fresh leaf into `matrix/<row_id>/` and prefers **`ms_per_present`** in `MATRIX.md`. Cold columns come from `cold_phase` when present.

DEM/hypso bake cache (process + `out/data/cache/dem_bake/`, override `SMT_DEM_BAKE_CACHE`) keys on path + file stamp (+ `max_edge` for hypso).

## SMT_SCENE3D_ENGINE

Implemented in `content::apply_scene3d_engine_from_env()`:

| Value | Engine | Also sets |
| --- | --- | --- |
| `flycube` / `dx12` | `kFlyCube` | — |
| `stereo_gl` / `opengl` / `gl` | `kStereoGl` | `SMT_STEREO_API=OpenGL` |
| `stereo_d3d` / `d3d` / `direct3d` / `d3d11` | `kStereoGl` | `SMT_STEREO_API=Direct3D` |
| `gdi` | `kGdi` | (API exists; **not** a matrix perf/smoke row) |

When set, `browser_main` does **not** force showcase GDI default.

Helpers: `prefer_scene3d_stereo_opengl()` · `prefer_scene3d_stereo_d3d()`.

## Leftover BMP leaves

| Backend | Preferred leaf |
| --- | --- |
| OpenGL | `legacy-scene3d-showcase-china-gl.bmp` (fallback `…-china.bmp`) |
| D3D11 | `legacy-scene3d-showcase-china-d3d.bmp` |

## Artifact layout

```
out/Debug/captures/analysis/world3d_opt/matrix/
  flycube/ … gl_leftover/ … d3d_leftover/ … null/
  world3d_backend_matrix.csv
  world3d_backend_matrix.json
  MATRIX.md   # Performance = bare perf rows only; Smoke section for null
  full_materials/   # M4 — role=full_materials (PERF_BARE unset)
    flycube/ …
    world3d_full_materials_matrix.csv
    world3d_full_materials_matrix.json
    MATRIX.md   # Full materials table; never merge into bare Performance
```

## M4 full-materials (non-bare)

```bat
py -3 testing/tools/harness/plugin/run_world3d_full_materials_matrix.py
py -3 testing/tools/harness/plugin/run_world3d_full_materials_matrix.py --row flycube
```

| | Bare matrix | Full-materials sibling |
| --- | --- | --- |
| Env | `SMT_PLUGIN_WORLD3D_PERF_BARE=1` | unset |
| role | `perf` / `smoke` | `full_materials` |
| Peer ranking | warm `ms_per_present` (FlyCube + leftover) | FlyCube full materials only |
| Phase reuse | optional | `ocean_prep_ms` / `record_ms` / `present_swap_ms` (same JSON leaves as scene3d-frame-opt) |

Do **not** put `role=full_materials` rows into the bare Performance table.