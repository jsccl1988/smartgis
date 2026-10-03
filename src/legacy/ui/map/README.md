<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/legacy/ui/map` — leftover map / 3D `CView` hosts

Capability under `ui_legacy` (`dll_stem=ui_legacy`). Semantic peer of endgame `ui/views/map`; stays under `legacy/`. Scheme C; no old-path shim.

**Living lock:** `docs/superpowers/specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md` §11e.  
**Diagram:** `docs/superpowers/diagrams/legacy-ui-map-deep-layer.html`.

## Layout

```
map/
  viewport/   # Smt2DXView · Smt2DEditXView · Smt3DXView
  chrome/     # identity_hud · aux_overlay
  framing/    # oper_map_frame (2D ZoomToRect)
  menu/       # am_menu (aux-module attach)
  tools/      # bind_workspace (2D tool ↔ Workspace)
```

| Item | Value |
| --- | --- |
| GN | `//src/legacy/ui/map:map_sources` → `ui_legacy` |
| Include | `"legacy/ui/map/<role>/…"` |
| Class ABI | `Smt2DXView` / `Smt3DXView` / `Smt2DEditXView` + `XVIEW_EXPORT` |

Historical alias: `group("viewport")` → `:map`.
