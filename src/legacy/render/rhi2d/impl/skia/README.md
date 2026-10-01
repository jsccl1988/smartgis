<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmtSkiaRenderDevice

Paint lane: `SkiaBackend` via `create_backend.cc` (GDI+ bootstrap without Skia
pin). Loaded at runtime by
`SmtRenderer::CreateDevice("SmtSkiaRenderDevice")` → `legacy_rhi2d_skia.dll`.
