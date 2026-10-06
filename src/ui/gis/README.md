<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/gis` — Product GIS horizon

GIS panels and product dialogs layered **above** `//src/ui/views` and **below** `//src/app/views`.

| | |
| --- | --- |
| DLL | `ui_views.dll` (same PE as views/gfx; `:gis` → `//src/ui/views:ui_views`) |
| Namespace | `ui::views` (directory move first; no mass symbol rename) |
| Includes | `"ui/gis/<area>/…"` |
| Export | `UI_EXPORT` (`ui/ui_export.h`) — sole export for all of `//src/ui` |

## Tree

```
src/ui/gis/
  BUILD.gn
  README.md
  gis.h / gis.cc          umbrella
  form_helpers.h          shared modal field validation helpers
  catalog/                CatalogView, LayerTree + Create*/AddBasemap modals
  inspect/                AttributeTable, FeatureInfo, Measure, Selection
                          + AttributeSchema modal
  shell/                  AmboxView, AtmospherePanel, ChartView, StatusBar
  style/                  Legend, Symbology, LayerProperties
  analysis/               Processing, SpatialAnalysis, GeoprocessingHistory
  debug/                  DebugConsole, DiagnosticTools, RenderTrace
                          (load_markup + src/ui/resources/<area>/*.ui.xml)
```

Toolkit-only dialogs (`Dialog`, `MessageBox`, `FilePicker`, `InputText`, `SelectOne`) stay in `src/ui/views/dialogs/` (`resources/toolkit/`). Map hang stays in `src/ui/views/map/`. Product GIS modals live with their domain area (`catalog/`, `inspect/`), not a flat `dialogs/` bucket.

Wave1–3 panels load markup from
`resources/{shell,inspect,style,analysis,catalog,debug}/`.
Debug horizon (DebugConsole / RenderTrace / DiagnosticTools shell + `tabs_host`)
and Memory tab page horizon are markup-driven; tab pages still mount C++ children
(Output/Console/Trace + Memory chart paint). Ambox scroll shell and ChartView
title horizon are markup; dynamic tool buttons and series plot stay C++.
LayerTree stays custom paint (CatalogView already hosts it).

## Layering

```
gfx → views (kernel + primitives + markup + Dialog shell) → ui/gis → app
```

GN: `:gis_sources` compiles into `:ui_views` (same `UI_EXPORTS`). Stable `:gis` / `:ui_gis` groups forward to that DLL. Views `source_set`s do **not** list GIS TUs; App / plugins that need panels or GIS dialogs depend on `:gis` (or get it via `//:ui_views` group).

Markup ControlFactory still registers GIS tags as **placeholders** inside views (`register_gis_placeholder_markup_tags`); real panel instances are C++-constructed by the app.

Living docs: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md). As-built: [`docs/superpowers/ui-views-skia.md`](../../../docs/superpowers/ui-views-skia.md).
