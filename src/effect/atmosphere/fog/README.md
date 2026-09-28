<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# fog/

Implementation of `FogPass` (public header: `../fog_pass.h`).

Height / distance exponential fog for GIS visibility. Records a load-only
alpha-blended fullscreen NDC pass (`DepthMode::kDisabled`). Wired
through `AtmosphereFrame::record_post_opaque` after clouds.

Depth-sampled volumetric fog / leftover GL `SetFog` are out of scope — see
`docs/superpowers/plans/2026-09-27-sky-fog-terrain-lod.md`.
