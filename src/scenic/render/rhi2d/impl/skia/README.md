<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SkiaRenderDevice

Paint lane: `SkiaBackend` via `create_backend.cc` (GDI+ bootstrap without Skia
pin). Loaded at runtime by
`Renderer2d::CreateDevice("SkiaRenderDevice")` → `scenic_rhi2d_skia.dll`.
