<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# harness-auto-scene3d-frame-opt — reference

Progressive disclosure. Read when parsing perf JSON, diagnosing remesh churn, or comparing to leftover.

## Frame model

```
SmartGisViews --atmosphere-showcase=legacy
        │
        ├─ warm-up present (outside timed window)
        │
        ├─ timed presents × PRESENT_COUNT (pump_ms=0 when count>3)
        │     mesh → sync → rebuild → ocean_prep → record → present_swap
        │
        └─ atmosphere-showcase-perf.json
              ms_per_present = present_ms / present_count
              last-frame Scene3dPhaseSample
```

## Phase clocks (normative)

Owned by `content::Scene3dPhaseSample` (`scene3d_phase_profile.*`):

| Field | Meaning |
| --- | --- |
| `mesh_ms` | Mesh / terrain prep slice |
| `sync_ms` | World / DEM / overlay sync |
| `rebuild_ms` | `rebuild_meshes` cost |
| `rebuild_count` | Remesh invocations on last sample |
| `ocean_prep_ms` | Ocean GPU prepare |
| `record_ms` | Pass / graph record |
| `present_swap_ms` (`present_ms` in JSON) | Present / swap |

JSON keys match the showcase writer in `present_run.cc`.

## Pump trap (normative)

When `PRESENT_COUNT>3`, showcase forces `present_pump_ms=0` and **skips** `PeekMessage` / `pump_messages`. Draining the queue can dispatch main Browser map2d GDI paint (~100–200 ms) and wreck equal-profile `ms_per_present` even when Scene3d phases are single-digit.

Do **not** “helpfully” add sleeps or pumps into the timed loop.

## Env knobs

| Env | Role |
| --- | --- |
| `SMT_ATMOSPHERE_SHOWCASE_GPU` | `1` = FlyCube path |
| `SMT_ATMOSPHERE_SHOWCASE_PRESENT_COUNT` | Timed frames (1–600); >3 → pump 0 |
| `SMT_ATMOSPHERE_SHOWCASE_LINGER_MS` | `0` for benches |
| `SMT_SCENE3D_ENGINE` | Override engine (`flycube` / …) when needed |
| `SMT_GPUSCENE_PREP_PARALLEL` | Prep parallel on/off |
| `SMT_TRACE` | Chrome-trace / RenderTrace spans |

## Fix heuristics

| Symptom | Likely root |
| --- | --- |
| `rebuild_count` every frame | `mark_meshes_dirty` on ocean/sky; clear xyz before DEM cache; overlay reattach churn |
| High `ocean_prep_ms` warm | `prepare_gpu` not gated after `dem_gpu_synced_after_*` |
| High `record_ms` warm | Double Gerstner / record after prepare; animated ocean step cost |
| Wall >> phase sum | Message pump / map2d paint / linger sleep in timed window |
| Null RHI “fast” but GPU BMP fail | Do not claim green — visual gate required |

## vs matrix skill

| Concern | This skill | `harness-auto-world3d-opt` |
| --- | --- | --- |
| GL / D3D leftover rows | Optional A/B only | Primary |
| Every-frame phases | Primary | Secondary |
| Locked legacy profile | Required | Not required |
| Done bar | `ms_per_present` + `rebuild_count=0` | Screenshots + comparison table |

## Example reply skeleton

```markdown
## Scene3d 逐帧 profile

配置：Debug · atmosphere=legacy · 640×480 · GPU=1 · PRESENT_COUNT=30

### Before → After
| metric | before | after |
| --- | ---: | ---: |
| ms_per_present | | |
| rebuild_count | | |
| ocean_prep_ms | | |
| record_ms | | |
| present_swap_ms | | |

### Hot phase
`rebuild_count` — hypothesis: …

### Next
rebuild → re-bench → …
```
