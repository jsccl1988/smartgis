<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-world3d-opt — reference

## Matrix model

```
同等渲染物料  (all rows: SmartGIS.exe --plugin-showcase=world3d)
        │
        ├─ FlyCube/DX12
        │     PLUGIN_WORLD3D_PERF_BARE=1
        │     parallel ∈ {prep_default, prep_0, prep_on}
        │     role = perf
        │
        ├─ Null          (GPU-off; **full** product materials)
        │     role = smoke  ← not a performance peer
        │
        ├─ Scenic/GL     (row gl_scenic)
        │     SCENE3D_ENGINE=stereo_gl  → scenic_impl + scenic_render_gl
        │     role = perf
        │
        ├─ Scenic/D3D11  (row d3d_scenic)
        │     SCENE3D_ENGINE=stereo_d3d → scenic_impl + scenic_render_d3d
        │     role = perf
        │
        └─ Scenic/GDI    (row scenic; SCENE3D_ENGINE=scenic)
              scenic.dll software Engine
              role = scenic  ← not a GPU peer

Leftover/legacy hosts are frozen. Do not launch SmartGIS-Legacy.exe.
```

Product app: **View → Engine** wires the same stack (`view.engine.scenic_gl` /
`scenic_d3d` / `scenic`). `Scene3dStereoSession` LoadLibrary `scenic_impl`, never
`leftover_render`.

## Perf bare (`PLUGIN_WORLD3D_PERF_BARE=1`)

| Off | Still on |
| --- | --- |
| sky / ocean / cloud / fog | China DEM + orbit |
| pointcloud overlay (load skipped) | present / BMP |
| warmup `pump_messages(50)` | phase JSON + `ms_per_present` |

## Fairness

| Claim | OK? |
| --- | --- |
| Compare FlyCube prep on vs off on **ms_per_present** / phases | Yes |
| Rank Scenic GL/D3D vs FlyCube by process `wall_ms` | **No** — use `ms_per_present` |
| Require BMP for gl_scenic / d3d_scenic (not N/A) | Yes |
| Put `null` / `gdi` / `scenic` (software) in the GPU performance table | **No** |
| Launch leftover SmartGis / leftover_render for GL/D3D | **No** — frozen |
| Fabricate BMP when SmartGIS.exe missing | **No** — build then re-run |

## Perf JSON

| Row family | Leaf | Fields |
| --- | --- | --- |
| All Views world3d | `captures/plugin/plugin-showcase-world3d-perf.json` | Primary: warm `ms_per_present`. FlyCube adds `cold_phase`. |

## SCENE3D_ENGINE

| Value | Engine | Also sets |
| --- | --- | --- |
| `flycube` / `dx12` | `kFlyCube` | — |
| `stereo_gl` / `scenic_gl` / `gl` | `kStereoGl` | `STEREO_API=OpenGL` |
| `stereo_d3d` / `scenic_d3d` / `d3d11` | `kStereoGl` | `STEREO_API=Direct3D` |
| `gdi` | `kGdi` | not a GPU peer |
| `scenic` | `kScenic` | scenic.dll software (not a GPU peer) |

`--plugin-showcase=world3d` honors env (does not re-pin FlyCube).

## Artifact layout

```
out/Debug/captures/analysis/world3d_opt/matrix/
  flycube/ … gl_scenic/ … d3d_scenic/ … null/ … scenic/
  world3d_backend_matrix.csv
  world3d_backend_matrix.json
  MATRIX.md
  full_materials/
```
