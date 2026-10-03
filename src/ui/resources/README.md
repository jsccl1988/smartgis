<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/resources`

Product declarative UI assets (`.ui.xml` / `.ui.css`) for Views markup dialogs
and GIS panels. Layout mirrors `src/ui/gis/<area>/` (+ `toolkit/` for generic
views dialogs).

| Area | Assets |
| --- | --- |
| `toolkit/` | `input_text`, `select_one` |
| `catalog/` | CatalogView (tabs host); CreateMap/Layer/Datasource, AddBasemap modals |
| `shell/` | StatusBar, AtmospherePanel, AmboxView (scroll shell), ChartView (title), **main_app** (SmartGisViews chrome skeleton — product via `ShellLayoutComposer`) |
| `inspect/` | Measure, Selection, FeatureInfo, AttributeTable; AttributeSchema modal |
| `style/` | Legend, Symbology, LayerProperties |
| `analysis/` | SpatialAnalysis, Processing, GeoprocessingHistory, ResultPlayback |
| `debug/` | DebugConsole, RenderTrace, DiagnosticTools (chrome + tabs_host), Memory page |

GN `:markup_resources` copies each area to shared **`out/ui/<area>/`**
(sibling of Debug/Release, same pattern as `out/data/`). Runtime:

```text
load_markup("catalog/create_map.ui.xml")
load_markup("inspect/attribute_schema.ui.xml")
load_markup("inspect/measure_panel.ui.xml")
```

Resolves via `<exe>/../ui/<rel>`, `<exe>/ui/<rel>`, and
`src/ui/resources/<rel>` when running from a source tree. Test samples under
`views/markup/testdata/` stay flat in `out/ui/`.
