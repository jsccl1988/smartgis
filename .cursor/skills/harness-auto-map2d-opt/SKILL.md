---
name: harness-auto-map2d-opt
description: >-
  Runs the map2d equal-profile harness: same china 1280x720 rendering materials
  and effects across parallel strategies x image-driven backends (GDI, GDI+,
  Skia, FlyCube), then emits screenshots and an execution performance comparison
  table. Use when the user invokes /harness-auto-map2d-opt, or says map2d 矩阵,
  同等渲染物料, 并行策略×图像驱动, GDI/GDI+/Skia/FlyCube 对比, map2d equal-profile,
  or asks for map2d screenshots + performance comparison.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto map2d opt（同等物料 × 并行×图像驱动）

**同等渲染物料及效果，并行策略×图像驱动（GDI GDI+ Skia FlyCube等），并给出截图及执行性能对比表**

Closed loop: build → run matrix → inspect PNGs → performance table → (optional) optimize against equal-profile budgets → re-run.

## Optimize order (P0 → P3)

When closing equal-profile budgets, follow plan phases — do **not** chase leftover IR:

| Phase | Focus | Surfaces |
| --- | --- | --- |
| **P0** MUST | Cold upload merge | `vista/map` `upload_draws` / `Pass::record` |
| **P1** | Layout / incremental | `Map2dFrameCache` / vista Layout |
| **P2** | Software GDI batch | `map2d/software` paint / frame GDI |
| **P3** | Parallel + **false-gap** labels | `SMT_VISTA_LAYOUT_PARALLEL` + this harness CSV/`note` |

**FALSE-GAP:** leftover `execute_ms` = IR replay only — **not** comparable to Vista `paint_ms` / `export_ms` / `present_gpu_*`. Matrix stdout + CSV `note` + `parallel_port_matrix_NOTE.txt` state this explicitly. Never claim “Vista is N× slower” from `execute_ms` vs `export_ms`.

## Authorization

When this skill is invoked, attached (`@harness-auto-map2d-opt` / `/harness-auto-map2d-opt`), or followed, the agent **MUST** run the matrix (and rebuild if binaries are missing) and deliver **screenshots + comparison table** — do not defer the harness to the user.

## Hard rules

1. **Equal profile (locked):** China mainland `[80,16]–[128,52]`, viewport **1280×720**, same sample / style richness (hillshade when DEM present). Do **not** strip carto to fake leftover IR ms.
2. **Fair compare / FALSE-GAP:** leftover `execute_ms` = **IR replay only**. Matrix equal-latitude sets `SMT_MAP2D_NO_HILLSHADE=1` so Vista skips DEM shade (same axis as leftover). Still compare **phase columns** (`paint_ms` / `present_gpu_*`); never claim `execute_ms ≡ export_ms`.
3. **Axes:**
   - Leftover: `SMT_RHI2D_PARALLEL` ∈ `{serial, tile, layer}` × `SMT_RHI2D_PORT` ∈ `{gdi, gdiplus, skia}`
   - Vista: Views software export + FlyCube `present_gpu` (`port=views+flycube`); harness sets `SMT_VISTA_LAYOUT_PARALLEL=1` (opt-out `=0`)
4. Prefer **`build.bat debug <single_target>`**. Compile lock stays **OFF**. Stay on **`master`**.
5. Prefer **`*.inspect.png`** for `Read` (BMP often fails vision).
6. CBM first for code lookup (`user-codebase-memory-mcp`, project `smartgis`).

## Entry commands

From repo root:

```bat
.\build.bat debug src/legacy/render/rhi2d/impl/common:gdi_map_paint_test
.\build.bat debug src/legacy/render:legacy_rhi2d_gdi
.\build.bat debug src/legacy/render:legacy_rhi2d_gdiplus
.\build.bat debug src/legacy/render:legacy_rhi2d_skia
.\build.bat debug src/app/views:views
py -3 testing/tools/harness/map2d/run_parallel_port_matrix.py
```
Do **not** prefix GN targets with `//` for `build.bat` / ninja on this repo.

Artifacts root: `out/Debug/captures/map2d/matrix/`

| Artifact | Role |
| --- | --- |
| `leftover-{parallel}_{port}.bmp` | Leftover capture (9 cells) |
| `vista-china.bmp` | Views + FlyCube equal-profile capture |
| `parallel_port_matrix_with_vista.csv` / `.json` | Timing rows + `note` / `matrix_note` |
| `parallel_port_matrix_NOTE.txt` | FALSE-GAP + equal-latitude one-liner |
| `leftover_*.log` / `vista_china.log` | Raw stdout for phase parse |
| `*.inspect.png` | Agent-readable screenshots (create if missing) |

## Workflow

Copy and track:

```
Map2d opt progress:
- [ ] 1. Build gdi_map_paint_test + SmartGisViews
- [ ] 2. Run run_parallel_port_matrix.py
- [ ] 3. Emit *.inspect.png for every matrix BMP
- [ ] 4. Read CSV/JSON; print performance comparison table
- [ ] 5. Read key inspect PNGs (at least vista + one leftover cell)
- [ ] 6. If optimize mode: fix hot phase → rebuild → re-run matrix → update table
```

### Step 1 — Build

```bat
.\build.bat debug src/legacy/render/rhi2d/impl/common:gdi_map_paint_test
.\build.bat debug src/legacy/render:legacy_rhi2d_gdi
.\build.bat debug src/legacy/render:legacy_rhi2d_gdiplus
.\build.bat debug src/legacy/render:legacy_rhi2d_skia
.\build.bat debug src/app/views:views
```

Missing `out\Debug\gdi_map_paint_test.exe` or `SmartGisViews.exe` → build again; do not skip leftover or vista silently. After host changes under `rhi2d/impl/common/host/`, rebuild **all three** port DLLs (they each compile `device_interact.cc`).

### Step 2 — Matrix

```bat
py -3 testing/tools/harness/map2d/run_parallel_port_matrix.py
```

Expect 9 leftover cells + 1 vista row. Exit non-zero if any `pass=false` — diagnose that cell (log + BMP) before claiming green.

### Step 3 — Screenshots (inspect PNG)

For each `out/Debug/captures/map2d/matrix/*.bmp` that lacks a sibling `*.inspect.png`:

```bat
py -3 -c "from pathlib import Path; from testing.tools.loop.review.inspect_png import bmp_to_inspect_png; root=Path('out/Debug/captures/map2d/matrix');
[print(bmp_to_inspect_png(p)) for p in root.glob('*.bmp')]"
```

(Run from repo root so `testing.tools…` imports resolve, or insert `sys.path` to repo root.)

### Step 4 — Performance comparison table

Read `parallel_port_matrix_with_vista.csv` (or `.json`). Runner already prints tables A+B + FALSE-GAP banner — copy them into the reply (or rebuild from CSV).

**A) Leftover — 并行策略 × 图像驱动** (`execute_ms_max` = **IR only**)

| parallel \ port | gdi | gdiplus | skia |
| --- | ---: | ---: | ---: |
| serial | `execute_ms_max` / wall | … | … |
| tile | … | … | … |
| layer | … | … | … |

Include `pass`, `bmp` path (or inspect PNG path) per cell when space allows.

**B) Vista phases (fair surface)** — leftover IR is **not** a column here

| metric | ms | notes |
| --- | ---: | --- |
| wall_ms | | process wall |
| export_ms | | software + BMP IO |
| paint_ms | | paint only (excl IO when present) |
| present_gpu_cold_ms | | first FlyCube present |
| present_gpu_warm_ms | | StaticReuse warm |
| layout_ms / hillshade_ms / software_paint_ms / bmp_io_ms / gpu_upload_ms / gpu_present_ms | | phase clocks |
| `smt_vista_layout_parallel` | | harness env (`1` default) |

Lead with FALSE-GAP: leftover `execute_ms` ≠ Vista `export_ms` / paint / present; equal-latitude `SMT_MAP2D_NO_HILLSHADE=1`.

### Step 5 — Visual evidence

`Read` at least:

1. `vista-china.inspect.png`
2. One leftover inspect (prefer `leftover-serial_gdi.inspect.png` or best `pass` cell)

Brief visual note: hillshade / coastline / labels present or missing. Do **not** start product fixes from vision unless the user asked to optimize / fix.

### Step 6 — Optimize mode (only when asked)

Triggers: user says 优化 / opt / equal-profile / 压 warm / 修 map2d 性能, or invokes this skill **and** asks to close plan budgets.

1. Open plan budgets / **P0→P1→P2→P3**: `docs/superpowers/plans/2026-10-01-src-render-map2d-equal-profile-optimize.md` (**P0** cold upload merge first; **P3** is labeling + `SMT_VISTA_LAYOUT_PARALLEL` — do not chase leftover IR)
2. Living §: `docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` §Vista Map2d equal-profile optimize
3. Hot phase from table B → CBM → root-cause fix in owned surfaces for that phase (not leftover IR)
4. Rebuild focused target → re-run matrix → new table + inspect
5. **Done bar (Debug china 1280×720):** warm `present_gpu_ms` ≤ **80**; `paint_ms` ≤ **100**; cold first present ≤ **400** after layout warm; visual hillshade + labels still on inspect

Non-goals (do not): delete hillshade/MapFrame to match leftover IR; MapLibre Native port; make leftover the product default; treat FALSE-GAP as a product bug.

## Env knobs (matrix already sets most)

| Env | Role |
| --- | --- |
| `SMT_RHI2D_PORT` | `gdi` / `gdiplus` / `skia` (leftover LoadLibrary port) |
| `SMT_RHI2D_PARALLEL` | `serial` / `tile` / `layer` |
| `SMT_RHI2D_PARALLEL_LOG` | emit `execute_ms` |
| `SMT_RHI2D_MATRIX_BMP` | leftover BMP path |
| `SMT_MAP2D_SHOWCASE_W/H` | `1280` / `720` |
| `SMT_MAP2D_SHOWCASE_GPU` | `1` = FlyCube present |
| `SMT_MAP2D_NO_HILLSHADE` | matrix default `1` = equal-latitude (no DEM); `0` = product shade-on |
| `SMT_MAP2D_EXPORT_REUSE` | unset in matrix (full paint); `1` = bench-only blit |
| `SMT_VISTA_LAYOUT_PARALLEL` | matrix default `1` (request vista tess parallel); `=0` opt-out serial — product getenv wire is parallel-plan V1 / P3a |

## Communication

- Progress and tables in **简体中文**
- Paths / env / metrics in **English** identifiers
- Lead with the comparison table + screenshot paths; keep narrative short

## Related

- Per-frame profile + opt loop: `.cursor/skills/harness-auto-map2d-frame-opt/SKILL.md`
- Runner: `testing/tools/harness/map2d/run_parallel_port_matrix.py`
- Suites: `testing/tools/harness/map2d/map2d.china/`
- Inspect: `testing/tools/loop/review/inspect_png.py`
- Visual review (bug closed-loop): `.cursor/skills/harness-visual-review/SKILL.md`
- Plan / §: links above
- Detail: [reference.md](reference.md)
