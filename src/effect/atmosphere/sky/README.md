<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# sky/

Implementation of `SkyPass` (public header: `../sky_pass.h`).

Analytical far-sky / horizon tint from sun direction (POD). Records a Y-up
hemisphere dome with `PipelineId::kSolid`. Wired through
`AtmosphereFrame::record_pre_opaque` (clears color + depth when enabled).

Full physical scattering / `PipelineId::kSky` HLSL remains deferred — see
`docs/superpowers/plans/2026-09-27-sky-fog-terrain-lod.md`.
