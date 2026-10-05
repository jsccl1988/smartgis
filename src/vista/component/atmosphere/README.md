<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/vista/component/atmosphere`

CPU atmosphere IR: `Environment` holds time, `AtmosphereParams`, `FieldStore`,
and the ocean/cloud systems the host projects into GPU passes. No RHI.
Compiled into `vista.dll` (`session_sources` + `atmosphere_cpu_sources`).
`assert_no_deps` `//src/render:render`. GPU recorders live in
`vista/pass/atmosphere`.

Living lock: [`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md`](../../../../docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md)
**§Vista IR/Pass lanes**. Diagram: [`vista-subdirectory-layers.html`](../../../../docs/superpowers/diagrams/vista-subdirectory-layers.html).

## Layers

| Path | Owns |
| --- | --- |
| `environment.*` / `atmosphere_params.h` | Public session facade. Hosts include `"vista/component/atmosphere/environment.h"`. |
| `field/` | `FieldStore`, channel enum, GDAL ingest, procedural seed. |
| `ocean/` | `OceanSystem` (spectrum / tile sample) + `cpu_waves` (FFT IR GPU ocean consumes). |
| `cloud/` | `CloudSystem` (cover / advection samples for CloudPass). |
| `detail/math.h` | RHI-free helpers shared with GPU ocean/sky. |

Do not add a public third namespace (`vista::pass` or `vista::component`). Internals stay
`vista::detail`. No forwarding headers at the old `session/` paths.
GPU must not GN-dep `session_sources`.

Tests: `field_store_test`, `field_ingest_test`, `procedural_test`,
`cloud_system_test`, `ocean_system_test`, `environment_test`.

---

**最后更新：** 2026-10-05
