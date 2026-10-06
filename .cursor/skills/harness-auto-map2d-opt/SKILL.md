---
name: harness-auto-map2d-opt
description: >-
  Runs the map2d equal-profile harness: same china 1280x720 rendering materials
  and effects across parallel strategies x image-driven backends (Scenic rhi2d
  GDI / GDI+ / Skia, plus Vista), then emits screenshots and an
  execution performance comparison table. Use when the user invokes
  /harness-auto-map2d-opt, or says map2d 矩阵, 同等渲染物料, 并行策略×图像驱动,
  GDI/GDI+/Skia/Vista 对比, scenic 同等对比, map2d equal-profile, or asks for
  map2d screenshots + performance comparison.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto map2d opt（同等物料 × 并行×图像驱动）

**同等渲染物料及效果，并行策略×图像驱动（Scenic 引擎 GDI / GDI+ / Skia，再加 Vista），并给出截图及执行性能对比表**

GDI / GDI+ / Skia **must** run on the **Scenic** rhi2d engine (`scenic_gdi_map_paint_test` + `scenic_rhi2d_{gdi,gdiplus,skia}`). Product matrix only — `src/legacy/` is frozen and is not an axis.

Closed loop: build → run matrix → inspect PNGs → performance table → (optional) optimize against equal-profile budgets → re-run.

## Optimize order (P0 → P3)

When closing equal-profile budgets, follow plan phases — do **not** treat Scenic IR as Vista paint:

| Phase | Focus | Surfaces |
| --- | --- | --- |
| **P0** MUST | Cold upload merge | `vista/component/map` `upload_draws` / `Pass::record` |
| **P1** | Layout / incremental | `Map2dFrameCache` / vista Layout |
| **P2** | Software GDI batch | Scenic `scenic_rhi2d_gdi` + `map2d/software` paint |
| **P3** | Parallel + **false-gap** labels | `VISTA_LAYOUT_PARALLEL` + this harness CSV/`note` |

**FALSE-GAP:** Scenic rhi2d `execute_ms` = IR replay only — **not** comparable to Vista `paint_ms` / `export_ms` / `present_gpu_*`. Matrix stdout + CSV `note` + `parallel_port_matrix_NOTE.txt` state this explicitly. Never claim “Vista is N× slower” from `execute_ms` vs `export_ms`.

## Authorization

When this skill is invoked, attached (`@harness-auto-map2d-opt` / `/harness-auto-map2d-opt`), or followed, the agent **MUST** run the matrix (and rebuild if binaries are missing) and deliver **screenshots + comparison table** — do not defer the harness to the user.

## Hard rules

1. **Equal profile (locked):** China mainland `[80,16]–[128,52]`, viewport **1280×720**, same sample / style richness (hillshade when DEM present). Do **not** strip carto to fake IR ms.
2. **Fair compare / FALSE-GAP:** Scenic rhi2d `execute_ms` = **IR replay only**. Matrix equal-latitude sets `MAP2D_NO_HILLSHADE=1` so Vista skips DEM shade (same axis as Scenic IR). Still compare **phase columns** (`paint_ms` / `present_gpu_*`); never claim `execute_ms ≡ export_ms`.
3. **Axes:**
   - **Scenic rhi2d (GDI / GDI+ / Skia):** `RHI2D_PARALLEL` ∈ `{serial, tile, layer}` × `RHI2D_PORT` ∈ `{gdi, gdiplus, skia}` via `scenic_gdi_map_paint_test` LoadLibrary `scenic_rhi2d_*`
   - **Vista:** Views software export + Vista `present_gpu` (`port=views+flycube`); harness sets `VISTA_LAYOUT_PARALLEL=1` (opt-out `=0`)
   - **Scenic Map2dEngine (not a port peer):** one extra cell `MAP2D_ENGINE=scenic` — content-hosted `scenic::Engine` GDI of the same china MapScene
4. Prefer **`build.bat debug <single_target>`**. Compile lock stays **OFF**. Stay on **`master`**. Do **not** build or edit `src/legacy/` (`src/legacy` freeze).
5. Prefer **`*.inspect.png`** for `Read` (BMP often fails vision).
6. CBM first for code lookup (`user-codebase-memory-mcp`, project `smartgis`).

## Entry commands

From repo root:

```bat
.\build.bat debug src/scenic/render/rhi2d/impl/common:scenic_gdi_map_paint_test
.\build.bat debug src/scenic:scenic_rhi2d_gdi
.\build.bat debug src/scenic:scenic_rhi2d_gdiplus
.\build.bat debug src/scenic:scenic_rhi2d_skia
.\build.bat debug src/app/views:views
py -3 testing/tools/harness/browser/run_parallel_port_matrix.py
```
Do **not** prefix GN targets with `//` for `build.bat` / ninja on this repo.

Artifacts root: `out/Debug/captures/browser/matrix/`

| Artifact | Role |
| --- | --- |
| `scenic-{parallel}_{port}.bmp` | Scenic rhi2d capture (9 cells) |
| `vista-china.bmp` | Views + Vista equal-profile capture |
| `scenic-china.bmp` | Map2dEngine (`MAP2D_ENGINE=scenic`) capture |
| `parallel_port_matrix_with_vista.csv` / `.json` | Timing rows + `note` / `matrix_note` |
| `parallel_port_matrix_NOTE.txt` | FALSE-GAP + equal-latitude one-liner |
| `scenic_*.log` / `vista_china.log` / `scenic_china.log` | Raw stdout for phase parse |
| `*.inspect.png` | Agent-readable screenshots (create if missing) |

## Workflow

Copy and track:

```
Map2d opt progress:
- [ ] 1. Build scenic_gdi_map_paint_test + scenic_rhi2d ports + SmartGisViews
- [ ] 2. Run run_parallel_port_matrix.py
- [ ] 3. Emit *.inspect.png for every matrix BMP
- [ ] 4. Read CSV/JSON; print performance comparison table
- [ ] 5. Read key inspect PNGs (at least vista + one Scenic GDI cell)
- [ ] 6. If optimize mode: fix hot phase → rebuild → re-run matrix → update table
```

### Step 1 — Build

```bat
.\build.bat debug src/scenic/render/rhi2d/impl/common:scenic_gdi_map_paint_test
.\build.bat debug src/scenic:scenic_rhi2d_gdi
.\build.bat debug src/scenic:scenic_rhi2d_gdiplus
.\build.bat debug src/scenic:scenic_rhi2d_skia
.\build.bat debug src/app/views:views
```

Missing `out\Debug\scenic_gdi_map_paint_test.exe` or `SmartGIS.exe` → build again; do not skip Scenic ports or vista silently. After host changes under `src/scenic/render/rhi2d/impl/common/host/`, rebuild **all three** Scenic port DLLs (they each compile `device_interact.cc`).

### Step 2 — Matrix

```bat
py -3 testing/tools/harness/browser/run_parallel_port_matrix.py
```

Expect 9 Scenic rhi2d cells + 1 vista row + 1 Map2dEngine row. Exit non-zero if any `pass=false` — diagnose that cell (log + BMP) before claiming green.

### Step 3 — Screenshots (inspect PNG)

For each `out/Debug/captures/browser/matrix/*.bmp` that lacks a sibling `*.inspect.png`:

```bat
py -3 -c "from pathlib import Path; from testing.tools.loop.review.inspect_png import bmp_to_inspect_png; root=Path('out/Debug/captures/browser/matrix');
[print(bmp_to_inspect_png(p)) for p in root.glob('*.bmp')]"
```

(Run from repo root so `testing.tools…` imports resolve, or insert `sys.path` to repo root.)

### Step 4 — Performance comparison table

Read `parallel_port_matrix_with_vista.csv` (or `.json`). Runner already prints tables A+B+C + FALSE-GAP banner — copy them into the reply (or rebuild from CSV).

**A) Scenic rhi2d — 并行策略 × 图像驱动** (`execute_ms_max` = **IR only**；三端口同一 Scenic 引擎)

| parallel \ port | gdi | gdiplus | skia |
| --- | ---: | ---: | ---: |
| serial | `execute_ms_max` / wall | … | … |
| tile | … | … | … |
| layer | … | … | … |

Include `pass`, `bmp` path (or inspect PNG path) per cell when space allows.

**B) Vista phases (fair surface)** — Scenic IR is **not** a column here

| metric | ms | notes |
| --- | ---: | --- |
| wall_ms | | process wall |
| export_ms | | software + BMP IO |
| paint_ms | | paint only (excl IO when present) |
| present_gpu_cold_ms | | first Vista present |
| present_gpu_warm_ms | | StaticReuse warm |
| layout_ms / hillshade_ms / software_paint_ms / bmp_io_ms / gpu_upload_ms / gpu_present_ms | | phase clocks |
| `smt_vista_layout_parallel` | | harness env (`1` default) |

**C) Scenic Map2dEngine** — `MAP2D_ENGINE=scenic`；content-hosted `scenic::Engine` GDI，**不是** GDI+/Skia 端口同行。

Lead with FALSE-GAP: Scenic rhi2d `execute_ms` ≠ Vista `export_ms` / paint / present; equal-latitude `MAP2D_NO_HILLSHADE=1`.

### Step 5 — Visual evidence

`Read` at least:

1. `vista-china.inspect.png`
2. One Scenic rhi2d inspect (prefer `scenic-serial_gdi.inspect.png` or best `pass` cell)

Brief visual note: hillshade / coastline / labels present or missing. Do **not** start product fixes from vision unless the user asked to optimize / fix.

### Step 6 — Optimize mode (only when asked)

Triggers: user says 优化 / opt / equal-profile / 压 warm / 修 map2d 性能, or invokes this skill **and** asks to close plan budgets.

1. Open plan budgets / **P0→P1→P2→P3**: `docs/superpowers/plans/2026-10-01-src-render-map2d-equal-profile-optimize.md` (**P0** cold upload merge first; **P3** is labeling + `VISTA_LAYOUT_PARALLEL` — do not chase Scenic IR as Vista paint)
2. Living §: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` §Vista Map2d equal-profile optimize
3. Hot phase from table B → CBM → root-cause fix in owned product surfaces for that phase (Scenic port IR only for table A)
4. Rebuild focused target → re-run matrix → new table + inspect
5. **Done bar (Debug china 1280×720):** warm `present_gpu_ms` ≤ **80**; `paint_ms` ≤ **100**; cold first present ≤ **400** after layout warm; visual hillshade + labels still on inspect

Non-goals (do not): delete hillshade/MapFrame to match IR; MapLibre Native port; treat FALSE-GAP as a product bug; touch `src/legacy/` (frozen).

## Env knobs (matrix already sets most)

| Env | Role |
| --- | --- |
| `RHI2D_PORT` | `gdi` / `gdiplus` / `skia` (Scenic `scenic_rhi2d_*` LoadLibrary) |
| `RHI2D_PARALLEL` | `serial` / `tile` / `layer` |
| `RHI2D_PARALLEL_LOG` | emit `execute_ms` |
| `RHI2D_MATRIX_BMP` | Scenic rhi2d BMP path |
| `MAP2D_ENGINE` | port grid: unset; Map2dEngine cell: `scenic` |
| `MAP2D_SHOWCASE_W/H` | `1280` / `720` |
| `MAP2D_SHOWCASE_GPU` | `1` = Vista present (vista cell) |
| `MAP2D_NO_HILLSHADE` | matrix default `1` = equal-latitude (no DEM); `0` = product shade-on |
| `MAP2D_EXPORT_REUSE` | unset in matrix (full paint); `1` = bench-only blit |
| `VISTA_LAYOUT_PARALLEL` | matrix default `1` (request vista tess parallel); `=0` opt-out serial |

## Communication

- Progress and tables in **简体中文**
- Paths / env / metrics in **English** identifiers
- Lead with the comparison table + screenshot paths; keep narrative short

## Related

- Per-frame profile + opt loop: `.cursor/skills/harness-auto-map2d-frame-opt/SKILL.md`
- Runner: `testing/tools/harness/browser/run_parallel_port_matrix.py`
- Suites: `testing/tools/harness/browser/browser.map2d.china/`
- Inspect: `testing/tools/loop/review/inspect_png.py`
- Visual review (bug closed-loop): `.cursor/skills/harness-visual-review/SKILL.md`
- Plan / §: links above
- Detail: [reference.md](reference.md)
