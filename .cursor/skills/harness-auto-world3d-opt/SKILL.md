---
name: harness-auto-world3d-opt
description: >-
  Runs the world3d equal-profile harness: same True-Earth Scene3D rendering
  materials and effects across parallel strategies x image-driven backends
  (GL, D3D leftover, FlyCube/DX12, Null, GDI), then emits screenshots and an
  execution performance comparison table. Use when the user invokes
  /harness-auto-world3d-opt, or says world3d 矩阵, 同等渲染物料, 并行策略×图像驱动,
  GL/D3D/FlyCube 对比, world3d equal-profile, or asks for world3d screenshots +
  performance comparison.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Harness auto world3d opt（同等物料 × 并行×图像驱动）

**同等渲染物料及效果，并行策略×图像驱动（GL D3D FlyCube等），并给出截图及执行性能对比表**

Closed loop: build → run matrix → inspect PNGs → performance table → (optional) optimize against equal-profile budgets → re-run.

Sibling of `harness-auto-map2d-opt` (2D china). This skill owns **plugin.world3d / Scene3D** (+ leftover stereo GL/D3D legs).

## Authorization

When this skill is invoked, attached (`@harness-auto-world3d-opt` / `/harness-auto-world3d-opt`), or followed, the agent **MUST** run the matrix (and rebuild if binaries are missing) and deliver **screenshots + comparison table** — do not defer the harness to the user.

## Hard rules

1. **Equal profile (locked) on FlyCube rows:** `--plugin-showcase=world3d` seed. Do **not** strip atmosphere / DEM / cloud to fake ms wins.
2. **Image-driven gate:** non-trivial BMP required (except documented `null`). Prefer `*.inspect.png` for `Read`.
3. **Axes:** FlyCube/DX12 · Null · GDI · **Stereo/GL** · **Leftover/D3D11**; FlyCube prep parallel on/off.
4. Prefer `build.bat debug src/app/views:views` **and** `build.bat debug SmartGis`. Compile lock **OFF**. Stay on **`master`**.
5. CBM first (`smartgis`). Product default remains **FlyCube/DX12**.
6. Fairness: leftover china stereo ≠ world3d pointcloud overlay — compare wall/phase columns, not identical materials across engines.

## Entry commands

```bat
.\build.bat debug src/app/views:views
.\build.bat debug SmartGis
py -3 testing/tools/harness/plugin/run_world3d_backend_matrix.py
```

Artifacts: `out/Debug/captures/analysis/world3d_opt/matrix/`

## Env (benchmark wiring)

| Env | Values | Effect |
| --- | --- | --- |
| `SMT_SCENE3D_ENGINE` | `flycube` / `stereo_gl` / `stereo_d3d` / `gdi` | `apply_scene3d_engine_from_env` — wins over showcase GDI default |
| `SMT_STEREO_API` | `OpenGL` / `Direct3D` | Leftover `smt_stereo_hwnd_create` (also forced by `stereo_gl` / `stereo_d3d`) |
| `SMT_SCENE3D_SHOWCASE_D3D` | `0` / `1` | Leftover D3D alias |
| `SMT_PLUGIN_WORLD3D_GPU` | `0` = Null-ish | Views world3d GPU gate |
| `SMT_GPUSCENE_PREP_PARALLEL` | `0` / `1` | FlyCube prep parallel |
| `SMT_PREFER_GDI_DEVICE` | `1` | Skip FlyCube preference |

## Workflow

```
World3d opt progress:
- [ ] 1. Build SmartGisViews + SmartGis
- [ ] 2. Run run_world3d_backend_matrix.py
- [ ] 3. Confirm *.inspect.png (incl. gl / d3d_leftover)
- [ ] 4. Print performance comparison table
- [ ] 5. Read flycube + gl + d3d_leftover inspect PNGs
- [ ] 6. Optimize mode (if asked): fix → rebuild → re-run
```

Runnable rows: `flycube`, `prep_par_off`, `prep_par_on`, `null`, `gdi`, **`gl`**, **`d3d_leftover`**. Do **not** mark GL/D3D as N/A when PE is merely unbuilt.

### Performance table

| Row | Backend | Parallel | wall_ms | pass | inspect |
| --- | --- | --- | ---: | --- | --- |
| flycube | FlyCube/DX12 | prep_default | | | |
| prep_par_off / prep_par_on | FlyCube/DX12 | prep_0 / on | | | |
| null / gdi | Null / GDI | … | | | |
| gl | Stereo/GL | leftover_serial | | | |
| d3d_leftover | Leftover/D3D11 | leftover_serial | | | |

## Related

- Per-frame profile + opt loop: `.cursor/skills/harness-auto-scene3d-frame-opt/SKILL.md`
- Runner: `testing/tools/harness/plugin/run_world3d_backend_matrix.py`
- Policy: `src/content/browser/present/scene3d/policy/scene3d_rhi_session.*`
- Leftover suites: `legacy.scene3d.china` · `legacy.scene3d.china.d3d`
- Detail: [reference.md](reference.md)
