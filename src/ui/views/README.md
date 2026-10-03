<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/ui/views` — Views toolkit (endgame)

**Diagram:** [`docs/superpowers/diagrams/ui-views-shell-architecture.html`](../../../docs/superpowers/diagrams/ui-views-shell-architecture.html)（toolkit / gfx / compositor 角色）

**Views** (widget / layout / events / controls) for in-process C++ shell. Public namespace: `ui::views`. Includes use `"ui/views/<area>/<group>/..."` under the responsibility groups below. `map/` nests `viewport|input|chrome|device`.

This directory is the **public toolkit**. Product composition is **`src/app/views`** (`out/SmartGisViews.exe`, destination entry). The app hosts a `Widget` / `Splitter` and places toolkit widgets; it does not paint catalog / ambox / chart / layer panels by hand. Leftover MFC `SmartGis.exe` stays until parity. MFC migration: [`docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-mfc-migration-design.md).

## Physical layout (responsibility partitions)

Headers and sources live together under responsibility groups. Callers include the grouped path (or the umbrella `views.h`). Namespace stays `ui::views`. Exception: `map/map_viewport.h` and `map/touch_multitouch.h` remain as **thin public forwards** to `map/viewport/` and `map/input/` so existing `#include "ui/views/map/…"` paths keep working.

### GN layer boundaries (first cut, 2026-09-28)

Still **one** product DLL (`ui_views.dll` via `:ui_views` / stable `:views`). Internally split into layered `source_set`s so deps match the directory tree:

```
views_kernel  →  views_control_factory  (ControlFactory + MarkupAttrs only)
                    →  views_markup       (XML/CSS/Yoga + layout/GIS tag regs)
                    →  views_primitives   (no Yoga / MarkupDocument edge)
                           ↓
                    views_dialogs (toolkit shell only)
views_kernel  →  views_map
views_sources (umbrella + ControlFactory::make_default) → ui_views → :views
GIS panels + product dialogs → //src/ui/gis (same ui_views.dll / UI_EXPORT)
```

| Target | May depend on | Must not depend on |
| --- | --- | --- |
| `views_kernel` | `base`, `gfx_sources` | primitives, dialogs, markup, map, control_factory |
| `views_control_factory` | kernel | yoga, pugixml, MarkupDocument, primitives |
| `views_markup` | kernel, control_factory, yoga, pugixml | primitives, dialogs |
| `views_primitives` | kernel, control_factory | views_markup, dialogs, map |
| `views_dialogs` | primitives | views_markup |
| `views_map` | kernel, content/view_host/rhi/tool | — |

`ControlFactory` registry header stays under `markup/factory/` (`control_factory.h`) but is a **separate** GN target (`:views_control_factory`) so `views_primitives` can register tags without depending on MarkupDocument/Yoga. **Concrete** tag creators register from `primitives/register_markup_controls.*` and `markup/factory/register_markup_tags.*`; `make_default()` is an aggregation TU (`markup/factory/control_factory_default.cc`) linked via `:views_sources`.

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
  primitives/register_markup_controls.*  markup tag creators for primitives
  dialogs/                    Dialog, MessageBox, FilePicker, InputText, SelectOne (flat)
  markup/style/               CssParser, FlexStyle / StyleSheet
  markup/document/            MarkupDocument, NamedViewMap
  markup/layout/              YogaLayoutManager
  markup/factory/             ControlFactory, PlaceholderView, register_markup_tags.*,
                              control_factory_default (make_default aggregation TU)
  markup/loader/              load_markup / MarkupRoot
  markup/testdata/            preview samples (test-only)
  map/map_viewport.h          public forward → viewport/map_viewport.h
  map/touch_multitouch.h      public forward → input/touch_multitouch.h
  map/viewport/               MapViewport + display/paint/shell/flycube + features
  map/input/                  viewport_input, TouchMultitouch
  map/frame/                 identity HUD, embed opaque fill
  map/device/                 legacy CreateRenderDevice load helpers
  testing/unit/               views_unittests, markup_unittests
  testing/harness/            EventGenerator, ViewsTestBase, OverlayScene
  testing/interactive/        views_interactive_tests (L1)
  testing/bench/              views_bench (L1b perf)
  testing/pixel/              pixel harness, views_pixel_tests
  testing/testdata/           PNG goldens

src/ui/resources/             product .ui.xml / .ui.css by area (GN → shared out/ui/<area>/)
src/ui/gis/                   GIS panels + product dialogs (same ui_views.dll / UI_EXPORT)
```

Module nest remains `src/ui/views` (one layer under `ui/`). The groups are directory partitions, not a third semantic namespace. Living umbrella: [`docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`](../../../docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md) (§Declarative markup / §ui/views subdirectory). Visual editor: `out/Debug/UiDesigner.exe` (`//src/app/ui_designer:ui_designer`).

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
| Markup style | `CssParser`, `StyleSheet` / FlexStyle | `ui/views/markup/style/` |
| Markup document | `MarkupDocument`, `NamedViewMap` | `ui/views/markup/document/` |
| Markup layout | `YogaLayoutManager` | `ui/views/markup/layout/` |
| Markup factory | `ControlFactory`, `PlaceholderView`, `register_markup_tags` | `ui/views/markup/factory/` |
| Markup loader | `load_markup`, `MarkupRoot` | `ui/views/markup/loader/` |
| Dialogs | `Dialog`, `pick_open_file` / `pick_save_file`, `show_message_box`, InputText, SelectOne | `ui/views/dialogs/` |
| GIS panels + product dialogs | Catalog (+ Create*/AddBasemap), Inspect (+ AttributeSchema), shell/style/analysis/debug | `ui/gis/{…}/` |
| Map hang | `MapViewport`, `TouchMultitouch` | `ui/views/map/` |

### Markup notes

- Resources: product `.ui.xml` + `.ui.css` under `src/ui/resources/<area>/` (toolkit + GIS areas); test samples under `markup/testdata/`. GN `:markup_resources` copies areas to shared `out/ui/<area>/` (`$root_out_dir/../ui`, Debug+Release); panels/modals load via `load_markup("area/name.ui.xml")`.
- Layout: declarative hosts use **Yoga** (`YogaLayoutManager`). Imperative `BoxLayout` / `FillLayout` remain for unmigrated toolkit dialogs — do not dual-drive one host.
- GIS complex panel tags instantiate **placeholder** Views (id/size); business data stays C++-bound.
- `contextmenu` is a stub View (Win32 popup is a free function, not a View).
- Preview / editor: `build.bat UiDesigner` → `out/Debug/UiDesigner.exe` (default opens `shell/main_app.ui.xml` SmartGisViews chrome template; open/save/hot-reload, palette, properties, tree, insert/reorder, **Text2UI** Generate… / Ctrl+Shift+G — template by default, `@llm` → Cursor Agent; bottom **Console+Trace** DiagnosticToolsPanel for UI paint profile — View → Toggle Console+Trace; CSD FrameView, Dark/Light theme).
- Text2UI API: `ui/views/text2ui/` (`generate_text2ui`, template matchers, validate). Host injects `LlmBackend` (UiDesigner: Cursor Agent CLI + `CURSOR_API_KEY`).
- Main app chrome: `src/ui/resources/shell/main_app.ui.xml` (+ `.ui.css`) — product `ShellLayoutComposer` loads it and mounts Catalog / Map / Ambox / inspector / Diagnostic / Status into `*_host` panels; UiDesigner opens the same file as the default canvas.
- UI render profile: `BASE_TRACE_EVENT(..., "ui.views")` on Widget paint/commit/present + ShellCompositor raster; RenderTrace **UI** filter; Diagnostic Tools tab **Trace**.

Map pixels stay on `src/map` / `src/feature` + `src/render`. Architecture: [`docs/build/ui-views-skia.md`](../../../docs/build/ui-views-skia.md). Control split: [`docs/superpowers/specs/2026-09-13-ui-views-controls-design.md`](../../../docs/superpowers/specs/2026-09-13-ui-views-controls-design.md) (nesting superseded by the 2026-09-19 design).

GN: `//src/ui/views:views` via `//:ui_views` (layered `views_kernel` / `views_control_factory` / `views_primitives` / `views_markup` / … under one DLL). Not in `src_all`. `views_unittests` / `markup_unittests` under `testing/unit/`. L1 `views_interactive_tests` + harness under `testing/harness/` / `testing/interactive/`. L1b `views_bench` under `testing/bench/`. `views_pixel_tests` under `testing/pixel/`. PNG goldens stay in `testing/testdata/`. GUI 分层与门禁：[`docs/build/ui-testing.md`](../../../docs/build/ui-testing.md)。

---

**最后更新：** 2026-09-30
