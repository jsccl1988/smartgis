<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/render/skia` — Skia backend stub

Skia is the **canvas / paint** backend for Views chrome. Public namespace: `render::skia`. Includes use `"render/skia/..."`.

**No Skia tree is vendored.** v1 `Canvas` fills rects and draws text via GDI. Do not download Skia or copy Chromium’s `third_party/skia` in a drive-by edit. Kept here (not `src/ui/gfx`) so paint stays a render backend.

Architecture: [`docs/build/ui-views-skia.md`](../../../docs/build/ui-views-skia.md).

GN: `//src/render/skia:skia` via `//:ui_views`. Not in `render_all` / `src_all`.

---

**最后更新：** 2026-09-13
