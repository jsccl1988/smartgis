<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# sky/

Implementation of `SkyPass` (public header: `../sky_pass.h`).

Analytical far-sky / horizon tint from sun direction (POD). Records a
fullscreen NDC triangle with a dedicated sky HLSL pipeline (CameraCB + SkyCB
view-ray). Wired through `AtmosphereFrame::record_pre_opaque` (color-only
clear + sky, then depth clear for ocean/terrain).

## View ray

RH unproject matches CameraCB (`look_at` column-major world-to-view):

1. `view_dir = (ndc.x / proj[0][0], ndc.y / proj[1][1], -1)`
2. World direction from **view rows** as camera axes
   (`right * x + up * y + (-forward) * z`) — same as industry fullscreen sky.
   Avoids `transpose(view 3x3)` seams when the horizon/zenith gradient runs
   across the screen.

`SkyPass::sample_sky_rgb` takes a world unit direction (no unproject); its
tint / sun / Bruneton-lite terms mirror `kPsSky`.

## Bruneton-lite (not full Hillaire LUT)

CPU and GPU use the same analytical approximation — **not** Bruneton/Hillaire
transmittance / multi-scatter LUT tables:

- Rayleigh-ish: deepen zenith toward blue (scale R↓ B↑ with view elevation)
- Mie-ish: warmer horizon tint near the sun azimuth
- Ozone-ish: slight purple at twilight (low `|sun_y|`)
- Sun: compact disk (`pow` high exponent) + softer corona; both use `sun_glow`

Full aerial-perspective LUT remains deferred — see
`docs/superpowers/plans/2026-09-27-sky-fog-terrain-lod.md` and
`docs/superpowers/specs/2026-09-13-render-rhi-scene-design.md` §Atmosphere look pack.
