<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/views` — Views toolkit (endgame)

Chromium-style **Views** (widget / layout / events / controls) for in-process C++ chrome. Public namespace: `ui::views`. Includes use `"ui/views/..."`.

This directory is the **public toolkit**. Product composition is **`src/app/views`** (`out/SmartGisViews.exe`, destination entry). The app hosts a `Widget` / `Splitter` and places toolkit widgets; it does not paint catalog / ambox / chart / layer panels by hand. Leftover MFC `SmartGis.exe` stays until parity. Chrome port: [`docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md).

## Public surface

| Kind | Types |
| --- | --- |
| Kernel | `View`, `Widget`, `LayoutManager` (`FillLayout` / `BoxLayout`), events, `Theme` |
| Primitives | `Label`, `Button`, `Textfield`, `Checkbox`, `RadioButton`, `Combobox`, `TabStrip`, `TableView` |
| Dialogs | `pick_open_file` / `pick_save_file`, `show_message_box` (Win32) |
| GIS | `CatalogView`, `LayerTree`, `AttributeTable`, `FeatureInfo`, `StatusBar`, `AmboxView`, `ChartView` |
| Map hang | `MapViewport` (child HWND; attach modes unchanged) |

Map pixels stay on `src/map` / `src/feature` + `src/render`. Architecture: [`docs/build/ui-views-skia.md`](../../../docs/build/ui-views-skia.md). Control split: [`docs/superpowers/specs/2026-09-13-ui-views-controls-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-controls-design.md).

GN: `//src/ui/views:views` via `//:ui_views`. Not in `src_all`. `views_unittests` is a console self-test (no MFC).

---

**最后更新：** 2026-09-13
