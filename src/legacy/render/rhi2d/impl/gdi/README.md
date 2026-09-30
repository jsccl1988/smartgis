<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmtGdiRenderDevice

SmartGIS leftover GDI 2D map present (`SmtRenderDevice` impl).

**Layout:** `legacy/render/rhi2d/impl/gdi/`.

| Subdir | Contents |
| --- | --- |
| `host/` | Facade `render_device.*` + `image_io` + `ui_controller`. Begin/End bind encoder; End `take`+`replay`. |
| `worker/` | FrameJob shell (`render_thread`, `raster_scheduler`). |
| `paint/canvas/` | Orchestration: `paint_canvas`, `layer_painter`, `carto_frame`, context/TLS/geom. |
| `paint/encode/` | `GdiCommandEncoder` / `GdiCommandBuffer` (record only). |
| `paint/player/` | `GdiPlayer` — sole HDC play lane (immediate + replay). |
| `paint/gdiplus/` | Process token + AA `draw_string` (capability, not a second player). |
| `surface/` | `compose.*`, `GdiSurface` / pool / owned |
| `res/` | RC + icons |
| `test/` | compose / encode / map_paint / carto |

No `core/` wrapper. No `gdiaux/`.

## Hard line vs `render::rhi`

Leftover GDI HWND present adapter with internal `Gdi*` vocabulary. Never place symbols in `render::rhi`. Present stays `BitBlt` / `InvalidateRect`.

## Play lane

- **Record:** `paint/encode` → `GdiCommandEncoder`
- **Play:** `paint/player` → `detail::GdiPlayer` (roads = dual GDI pens; anno prefers GDI+ via `paint/gdiplus`)
- **Interactive (MapLibre-aligned):** `PreviewZoomScale` stretches `vir_viewport2` around the cursor; pan uses `SetCurDrawingOrg` BitBlt offset. `ReRenderMapRealTime` is cancel → urgent FrameJob → return (no UI Sleep-wait). Settle via Timer / `ScheduleDelayedRedraw`.
