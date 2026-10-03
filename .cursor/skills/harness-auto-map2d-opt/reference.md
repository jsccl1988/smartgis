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
        ├─ leftover rhi2d (gdi_map_paint_test)
        │     parallel ∈ {serial, tile, layer}
        │           × port ∈ {gdi, gdiplus, skia}   ← 图像驱动
        │     metric: execute_ms (IR replay)
        │
        └─ src_render (SmartGisViews --map2d-showcase=china)
              software export_bmp + FlyCube present_gpu
              metrics: export_ms, paint_ms, present_gpu_{cold,warm}_ms, phase_*
```

## Fairness (normative)

| Claim | OK? |
| --- | --- |
| Compare leftover cells across parallel × port on `execute_ms_max` | Yes |
| Compare src_render phases across runs (before/after opt) | Yes |
| Treat leftover `execute_ms` as equal work to `export_ms` | **No** |
| Drop hillshade to hit leftover IR budget | **No** |

Leftover path paints from an IR command buffer (no DEM hillshade / MapFrame layout cost in `execute_ms`). src_render pays layout + hillshade + software paint + optional GPU upload/present.

## Phase columns (src_render)

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
  leftover-serial_gdi.bmp (+ .inspect.png)
  leftover-serial_gdiplus.bmp
  leftover-serial_skia.bmp
  leftover-tile_gdi.bmp
  …
  leftover-layer_skia.bmp
  leftover_{parallel}_{port}.log
  src_render-china.bmp (+ .inspect.png)
  src_render_china.log
  parallel_port_matrix_with_src_render.csv
  parallel_port_matrix_with_src_render.json
  leftover_parallel_port_matrix.csv
  leftover_parallel_port_matrix.json
```

Showcase may also write `out/Debug/captures/map2d/map2d-showcase-china.bmp`; the runner copies a large enough candidate into `src_render-china.bmp`.

## Example reply skeleton

```markdown
## Map2d equal-profile 矩阵

配置：Debug · china · 1280×720 · 同等物料

### Leftover 并行×端口（execute_ms_max）

| parallel | gdi | gdiplus | skia |
| --- | ---: | ---: | ---: |
| serial | 50 | 52 | 48 |
| tile | … | … | … |
| layer | … | … | … |

### src_render 相位

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

- `out/Debug/captures/map2d/matrix/src_render-china.inspect.png`
- `out/Debug/captures/map2d/matrix/leftover-serial_gdi.inspect.png`

说明：leftover execute_ms 为 IR-only，不可与 export_ms 直接等同。
```

## Diagnose red cell

1. Open matching `leftover_*.log` or `src_render_china.log`.
2. Confirm BMP exists and `bmp_bytes` > ~10KB.
3. Leftover: check `SMT_RHI2D_PORT` DLL under `out/Debug` (LoadLibrary).
4. src_render: check GPU adapter / `SMT_MAP2D_SHOWCASE_GPU`; cold crash → plan Task 2 (device reuse, no timed invalidate).
5. Rebuild only the failing PE; re-run full matrix for a consistent table.

## Optimize hotspots (plan map)

| Hot metric | Likely code | Plan task |
| --- | --- | --- |
| `paint_ms` / `export_ms` | map2d export / present-cache reuse | Task 3 |
| `present_gpu_cold_ms` / `gpu_upload_ms` | effect/map upload, device session | Task 2 / 5 |
| `layout_ms` | Map2dFrameCache rebuild | Task 4 |
| `hillshade_ms` | DEM shade cache / overview | Task 3 |

## Related suites (not the matrix)

- `testing/tools/harness/map2d/map2d.china/` — loop_runner showcase + score
- `testing/tools/harness/map2d/map2d.orthogrid/` — orthogrid variant

Matrix is the A/B surface for **并行策略×图像驱动**; suite china is the product score / visual-review path.
