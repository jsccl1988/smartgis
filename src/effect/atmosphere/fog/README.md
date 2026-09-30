<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# fog/

Implementation of `FogPass` (public header: `fog_pass.h`).

Height / distance exponential fog for GIS visibility. Records a load-only
alpha-blended fullscreen NDC pass (`DepthMode::kDisabled`). Pixel shader uses
sky-style `CameraCB` view-ray unproject; optional scene-depth SRV reconstructs
aerial distance (cleared/sky depth ~1.0 uses the far-ray). Haze tint comes from
`FogDrawParams` color (host usually mirrors analytical sky).

**Device depth SRV wiring is parent-owned** (`FogPass::record(..., depth)` —
typically `Device::shared_depth_texture()`). This module only binds the texture
when the caller passes a non-null pointer; it does not create or resize the
shared depth buffer.

Wired through `AtmosphereFrame::record_post_opaque` after clouds.
