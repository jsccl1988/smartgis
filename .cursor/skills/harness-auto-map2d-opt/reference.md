<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-map2d-opt — reference

Progressive disclosure for the skill. Read when parsing logs, diagnosing a red cell, or extending the matrix.

## Matrix model

```
同等渲染物料 (china 1280x720, same extent/sample/style)
        │
        ├─ Scenic rhi2d (scenic_gdi_map_paint_test)
        │     parallel ∈ {serial, tile, layer}
        │           × port ∈ {gdi, gdiplus, skia}   ← 图像驱动（同一 Scenic 引擎）
        │     DLL: scenic_rhi2d_{gdi,gdiplus,skia}[_d].dll
        │     metric: execute_ms (IR replay)
        │
        ├─ vista (SmartGIS.exe --map2d-showcase=china)
        │     software export_bmp + FlyCube present_gpu
        │     metrics: export_ms, paint_ms, present_gpu_{cold,warm}_ms, phase_*
        │
        └─ Scenic Map2dEngine (MAP2D_ENGINE=scenic)
              content-hosted scenic::Engine GDI of the same china MapScene
              not a GDI+/Skia port peer
```

`src/legacy/` is frozen. This matrix’s equal-compare axis is Scenic rhi2d + Vista only.

## Fairness (normative) — FALSE-GAP

| Claim | OK? |
| --- | --- |
| Compare Scenic rhi2d cells across parallel × port on `execute_ms_max` | Yes |
| Compare Vista phases across runs (before/after opt) | Yes |
| Equal-latitude: matrix `MAP2D_NO_HILLSHADE=1` (no DEM) vs Scenic IR | Yes |
| Treat Scenic `execute_ms` as equal work to `export_ms` / paint / present | **No** (FALSE-GAP) |
| Claim “Vista is N× slower” from IR vs export | **No** |
| Rank GDI+ / Skia against a frozen `src/legacy` port | **No** |
| Delete product hillshade permanently to match IR budgets | **No** |

Sample `matrix_note` / `parallel_port_matrix_NOTE.txt` string:

```
FALSE-GAP: scenic rhi2d execute_ms = IR replay only; NOT comparable to Vista paint_ms/export_ms/present_gpu_*; never claim execute_ms == export_ms; equal-latitude: MAP2D_NO_HILLSHADE=1 (Vista skips DEM shade; same carto axis as scenic rhi2d IR which has no hillshade)
```

Scenic rhi2d paints from an IR command buffer (no DEM hillshade / MapFrame layout in `execute_ms`). Matrix equal-latitude turns off Vista DEM shade via env; Vista still pays MapFrame layout + software paint + optional GPU upload/present. Runner prints Scenic port grid (table A) + Vista phase table (table B) + Map2dEngine (table C) on every run.

## Optimize order P0–P3

| Phase | Do first | Do not |
| --- | --- | --- |
| **P0** | Cold `upload_draws` merge | Chase Scenic IR as Vista paint |
| **P1** | Layout / frame-cache incremental | Strip MapFrame |
| **P2** | Software GDI batch (Scenic GDI + map2d software) | Permanent NO_HILLSHADE product default |
| **P3** | `VISTA_LAYOUT_PARALLEL` + false-gap labels in harness | Treat FALSE-GAP as a bug |

Harness sets `VISTA_LAYOUT_PARALLEL=1` on the vista cell (`=0` opt-out). Product emitters must `getenv` that flag (parallel plan V1 / equal-profile P3a) — until then tess still keys off job count only.

## Phase columns (Vista)

Parsed from showcase logs by `run_parallel_port_matrix.py`:

| Field | Meaning |
| --- | --- |
| `layout_ms` | MapFrame / frame-cache layout |
| `hillshade_ms` | DEM shade / underlay prep |
| `software_paint_ms` | CPU paint into bitmap |
| `paint_ms` | Prefer paint-only (may match software_paint when reuse) |
| `bmp_io_ms` | BMP write |
| `gpu_upload_ms` | Upload to FlyCube resources |
| `gpu_present_ms` | GPU present slice |
| `phase_gate_export_ok` | phase sum ≈ `export_ms` within ±15% |
| `phase_gate_cold_ok` | phase sum ≈ cold present within ±15% |

## Artifact layout

```
out/Debug/captures/map2d/matrix/
  scenic-serial_gdi.bmp (+ .inspect.png)
  scenic-serial_gdiplus.bmp
  scenic-serial_skia.bmp
  scenic-tile_gdi.bmp
  …
  scenic-layer_skia.bmp
  scenic_{parallel}_{port}.log
  vista-china.bmp (+ .inspect.png)
  vista_china.log
  scenic-china.bmp (+ .inspect.png)   # Map2dEngine cell
  scenic_china.log
  parallel_port_matrix_with_vista.csv
  parallel_port_matrix_with_vista.json   # {matrix_note, false_gap_note, equal_latitude_note, rows:[]}
  parallel_port_matrix_NOTE.txt          # FALSE-GAP + equal-latitude one-liner
  scenic_parallel_port_matrix.csv
  scenic_parallel_port_matrix.json
```

Showcase may also write `out/Debug/captures/map2d/map2d-showcase-china.bmp`; the runner copies a large enough candidate into `vista-china.bmp` / `scenic-china.bmp`.

## Example reply skeleton

```markdown
## Map2d equal-profile 矩阵

配置：Debug · china · 1280×720 · 同等物料 · Scenic 引擎 GDI/GDI+/Skia

### Scenic rhi2d 并行×端口（execute_ms_max）

| parallel | gdi | gdiplus | skia |
| --- | ---: | ---: | ---: |
| serial | 50 | 52 | 48 |
| tile | … | … | … |
| layer | … | … | … |

### Vista 相位

| metric | ms |
| --- | ---: |
| export_ms | … |
| paint_ms | … |
| present_gpu_cold_ms | … |
| present_gpu_warm_ms | … |
| layout_ms | … |
| hillshade_ms | … |
| gpu_upload_ms | … |

### 截图

- `out/Debug/captures/map2d/matrix/vista-china.inspect.png`
- `out/Debug/captures/map2d/matrix/scenic-serial_gdi.inspect.png`

说明（FALSE-GAP）：Scenic rhi2d execute_ms 为 IR-only，不可与 Vista paint/export/present 直接等同；equal-latitude `MAP2D_NO_HILLSHADE=1`。
```

## Diagnose red cell

1. Open matching `scenic_*.log` or `vista_china.log` / `scenic_china.log`.
2. Confirm BMP exists and `bmp_bytes` > ~10KB.
3. Scenic ports: check `RHI2D_PORT` DLL under `out/Debug` (`scenic_rhi2d_gdi_d.dll` / `_gdiplus_d` / `_skia_d` in Debug).
4. vista: check GPU adapter / `MAP2D_SHOWCASE_GPU`; cold crash → plan Task 2 (device reuse, no timed invalidate).
5. Rebuild only the failing PE; re-run full matrix for a consistent table.

## Optimize hotspots (plan map)

| Hot metric | Likely code | Plan task |
| --- | --- | --- |
| `paint_ms` / `export_ms` | map2d export / present-cache reuse | Task 3 |
| `present_gpu_cold_ms` / `gpu_upload_ms` | effect/map upload, device session | Task 2 / 5 |
| `layout_ms` | Map2dFrameCache rebuild | Task 4 |
| `hillshade_ms` | DEM shade cache / overview | Task 3 |
| Scenic `execute_ms` (table A) | `src/scenic/render/rhi2d` port backend / parallel execute | compare ports only |

## Related suites (not the matrix)

- `testing/tools/harness/map2d/map2d.china/` — loop_runner showcase + score
- `testing/tools/harness/map2d/map2d.orthogrid/` — orthogrid variant

Matrix is the A/B surface for **Scenic 并行策略×图像驱动**; suite china is the product score / visual-review path.
