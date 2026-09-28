<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/views` — Views toolkit (endgame)

**Views** (widget / layout / events / controls) for in-process C++ shell. Public namespace: `ui::views`. Includes use `"ui/views/<area>/<group>/..."` under the responsibility groups below. `map/` stays one level deep.

This directory is the **public toolkit**. Product composition is **`src/app/views`** (`out/SmartGisViews.exe`, destination entry). The app hosts a `Widget` / `Splitter` and places toolkit widgets; it does not paint catalog / ambox / chart / layer panels by hand. Leftover MFC `SmartGis.exe` stays until parity. MFC migration: [`docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md).

## Physical layout (responsibility partitions)

Headers and sources live together under responsibility groups. There are **no** forwarding shims at the previous paths. Callers include the grouped path (or the umbrella `views.h`). Namespace stays `ui::views`.

```
src/ui/views/
  BUILD.gn
  README.md
  views.h / views.cc          umbrella
  kernel/view/                View hierarchy
  kernel/widget/              Widget (native HWND)
  kernel/layout/              Layout, LayoutCheck, Splitter, view traits
  kernel/shell/               Theme, Event, Dpi, DialogHost
  kernel/paint/               PaintCommit / DisplayList commit
  kernel/compositor/          ShellCompositor (pending/active + raster worker)
  primitives/button/          Button, Checkbox, RadioButton
  primitives/text/            Label, Textfield
  primitives/input/           Combobox, Slider
  primitives/collection/      TabStrip, TableView, TreeView, ScrollView
  primitives/menu/            MenuBar, ContextMenu
  dialogs/shell/              Dialog, MessageBox, FilePicker, InputText, SelectOne
  dialogs/gis/                CreateDatasource/Layer/Map, AttStruct, AddBasemap
  gis/catalog/                CatalogView, LayerTree
  gis/inspect/                FeatureInfo, AttributeTable
  gis/shell/                 StatusBar, AmboxView
  gis/panel/                  ChartView, AtmospherePanel, ProcessingPanel
  map/                        MapViewport, TouchMultitouch (flat)
  testing/unit/               views_unittests
  testing/pixel/              pixel harness, views_pixel_tests
  testing/testdata/           PNG goldens
```

Module nest remains `src/ui/views` (one layer under `ui/`). The groups are directory partitions, not a third semantic namespace. Design: [`docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md`](../../../docs/superpowers/specs/2026-09-19-ui-views-subdir-responsibility-design.md).

## Public surface

| Kind | Types | Include prefix |
| --- | --- | --- |
| Kernel view | `View` | `ui/views/kernel/view/` |
| Kernel widget | `Widget` | `ui/views/kernel/widget/` |
| Kernel layout | `LayoutManager` (`FillLayout` / `BoxLayout`), `Splitter`, axis traits | `ui/views/kernel/layout/` |
| Kernel shell | events, `Theme`, `Dpi`, `DialogHost` | `ui/views/kernel/shell/` |
| Kernel paint | `PaintCommit`, `commit_view_tree` | `ui/views/kernel/paint/` |
| Kernel compositor | `ShellCompositor` | `ui/views/kernel/compositor/` |
| Primitives | `Button`, `Checkbox`, `RadioButton`, `Label`, `Textfield`, `Combobox`, `Slider`, `TabStrip`, `TableView`, `TreeView`, `ScrollView`, `MenuBar`, `ContextMenu` | `ui/views/primitives/{button,text,input,collection,menu}/` |
| Dialogs | `Dialog`, `pick_open_file` / `pick_save_file`, `show_message_box`, GIS create/att/basemap dialogs | `ui/views/dialogs/{shell,gis}/` |
| GIS panels | `CatalogView`, `LayerTree`, `AttributeTable`, `FeatureInfo`, `StatusBar`, `AmboxView`, `ChartView`, `AtmospherePanel`, `ProcessingPanel` | `ui/views/gis/{catalog,inspect,shell,panel}/` |
| Map hang | `MapViewport`, `TouchMultitouch` | `ui/views/map/` |

Map pixels stay on `src/map` / `src/feature` + `src/render`. Architecture: [`docs/build/ui-views-skia.md`](../../../docs/build/ui-views-skia.md). Control split: [`docs/superpowers/specs/2026-09-13-ui-views-controls-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-controls-design.md) (nesting superseded by the 2026-09-19 design).

GN: `//src/ui/views:views` via `//:ui_views`. Not in `src_all`. `views_unittests` sources live under `testing/unit/`. `views_pixel_tests` sources live under `testing/pixel/`. PNG goldens stay in `testing/testdata/`. GUI 分层与门禁：[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md)。

---

**最后更新：** 2026-09-28
