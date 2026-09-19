<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/views` — Views toolkit (endgame)

Chromium-style **Views** (widget / layout / events / controls) for in-process C++ chrome. Public namespace: `ui::views`. Includes use `"ui/views/<area>/..."` under the six responsibility directories below.

This directory is the **public toolkit**. Product composition is **`src/app/views`** (`out/SmartGisViews.exe`, destination entry). The app hosts a `Widget` / `Splitter` and places toolkit widgets; it does not paint catalog / ambox / chart / layer panels by hand. Leftover MFC `SmartGis.exe` stays until parity. Chrome port: [`docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md).

## Physical layout (responsibility partitions)

Headers and sources live together under responsibility directories. There are **no** root forwarding shims. Callers include the partitioned path (or the umbrella `views.h`).

```
src/ui/views/
  BUILD.gn
  README.md
  views.h / views.cc     umbrella
  kernel/                View, Widget, Layout, Theme, Event, Dpi, Splitter, DialogHost
  primitives/            Button, Label, Textfield, Checkbox, Radio, Combobox,
                         TabStrip, TableView, TreeView, ScrollView, MenuBar, ContextMenu
  dialogs/               Dialog, FilePicker, MessageBox, InputText, SelectOne,
                         CreateDatasource/Layer/Map, AttStruct, AddBasemap
  gis/                   CatalogView, LayerTree, AttributeTable, FeatureInfo,
                         StatusBar, AmboxView, ChartView
  map/                   MapViewport, TouchMultitouch
  testing/               pixel harness, views_*tests, testdata/ goldens
```

Module nest remains `src/ui/views` (one layer under `ui/`). The six subdirs are **public implementation partitions**, not a third semantic nest like `src/ui/views/widget/`. Design: [`docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md`](../../../docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md).

## Public surface

| Kind | Types | Include prefix |
| --- | --- | --- |
| Kernel | `View`, `Widget`, `LayoutManager` (`FillLayout` / `BoxLayout`), events, `Theme`, `Splitter`, `DialogHost` | `ui/views/kernel/` |
| Primitives | `Label`, `Button`, `Textfield`, `Checkbox`, `RadioButton`, `Combobox`, `TabStrip`, `TableView`, `TreeView`, `ScrollView`, `MenuBar`, `ContextMenu` | `ui/views/primitives/` |
| Dialogs | `Dialog`, `pick_open_file` / `pick_save_file`, `show_message_box`, GIS create/att/basemap dialogs | `ui/views/dialogs/` |
| GIS panels | `CatalogView`, `LayerTree`, `AttributeTable`, `FeatureInfo`, `StatusBar`, `AmboxView`, `ChartView` | `ui/views/gis/` |
| Map hang | `MapViewport`, `TouchMultitouch` | `ui/views/map/` |

Map pixels stay on `src/map` / `src/feature` + `src/render`. Architecture: [`docs/build/ui-views-skia.md`](../../../docs/build/ui-views-skia.md). Control split: [`docs/superpowers/specs/2026-09-13-ui-views-controls-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-controls-design.md) (nesting superseded by the 2026-09-19 design).

GN: `//src/ui/views:views` via `//:ui_views`. Not in `src_all`. `views_unittests` / `views_pixel_tests` sources and goldens live under `testing/`. GUI 分层与门禁：[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md)。

---

**最后更新：** 2026-09-19
