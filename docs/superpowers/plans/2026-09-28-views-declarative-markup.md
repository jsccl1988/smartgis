<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views declarative markup + UiPreview — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship XML + Yoga Flex CSS declarative UI on Views+Skia, plus `UiPreview.exe` (runtime load + visual editor same milestone) — per [`../specs/2026-09-28-views-declarative-markup-design.md`](../specs/2026-09-27-views-desktop-shell-design.md).

**Architecture:** `.ui.xml` / `.ui.css` → `MarkupDocument` (pugixml) + `CssParser` → `ControlFactory` + `YogaLayoutManager` → product dialogs / `UiPreview` canvas; `NamedViewMap` for id binding.

**Tech Stack:** C++23, GN/Ninja `out/`, Yoga, pugixml, Views + Skia (no Qt, no desktop WebView this milestone).

**Spec:** `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`

## Global Constraints

- Stay on `master`; parallel agents use **non-overlapping paths**.
- No Qt. No desktop WebView2 / HTML engine this milestone. Mobile WebView = future-only.
- Yoga for declarative layout; keep `BoxLayout` / `FillLayout` for unmigrated imperative hosts (no dual-drive).
- New-tree functions `snake_case`; namespaces ≤ two public layers (`ui::views`).
- Copyright year **2026**; English comments.
- Build via `build.bat` / `ninja -C out`; output root `out/` only.
- A+C: runtime markup **and** editor must ship together for acceptance.
- Coverage F: primitives from XML; GIS complex panels = placeholder Views first.

## File map

| Path | Role |
| --- | --- |
| `third_party` Yoga pin + `//third_party:yoga` | Flexbox layout engine |
| `src/ui/views/markup/{style,document,layout,factory,loader}/` | CssParser, MarkupDocument, YogaLayoutManager, factory, load_markup; `resources/` + `testdata/` |
| `src/ui/views/BUILD.gn` | Wire `markup/` into `:views` / unit tests |
| `src/app/ui_preview/` | `UiPreview.exe` host + editor UI |
| `src/app/ui_preview/BUILD.gn` | `//src/app/ui_preview:ui_preview` |
| `src/ui/gis/dialogs/add_basemap_dialog.*` | Markup + id bind sample |
| `src/ui/gis/dialogs/att_struct_dialog.*` | Markup + id bind sample |
| `src/ui/gis/dialogs/create_{datasource,layer,map}_dialog.*` | Markup + id bind |
| `src/ui/resources/` | Product `.ui.xml` / `.ui.css` |
| `src/ui/views/README.md` | Document `markup/` + UiPreview when landed |

**Order:** Task 1 → 2 → 3 → 4 → 5 → 6 → 7 (docs-readme can finish after migrate).

---

### Task 1: Living spec + plan + Active table (`spec-docs`)

**Files:**
- Create: `docs/superpowers/specs/2026-09-27-views-desktop-shell-design.md`
- Create: `docs/superpowers/plans/2026-09-28-views-declarative-markup.md`
- Modify: `docs/superpowers/README.md`

- [x] Write living markup design (Status active; why not fold into ui-views-controls; lock A+C, F, W/Yoga+Flex CSS+XML, pugixml, no Qt, no desktop WebView, mobile WebView future-only).
- [x] Write this plan checklist matching the milestone todos.
- [x] Add Active living row in `docs/superpowers/README.md`.

### Task 2: Yoga + markup core (`yoga-markup-core`)

**Files:** `third_party` Yoga, `src/ui/views/markup/*`, views `BUILD.gn`, unit tests

- [x] Pin Yoga (manifest + GN `//third_party:yoga`).
- [x] Implement `CssParser` (Flex subset per spec).
- [x] Implement `MarkupDocument` (pugixml load of `.ui.xml` + style link).
- [x] Implement `YogaLayoutManager` → `View::set_bounds`.
- [x] Unit tests: flex row/column, gap, grow green.

### Task 3: ControlFactory (`control-factory`)

**Files:** markup factory + NamedViewMap; primitive / GIS stub registration

- [x] `ControlFactory` maps tags → Views for all README primitives.
- [x] GIS complex tags → placeholder View (id/size) or existing type; document stubs.
- [x] `NamedViewMap` / `find_view_by_id` for product bind.

### Task 4: UiPreview host (`ui-preview-host`)

**Files:** `src/app/ui_preview/` (load/save/hot-reload canvas; no drag editor yet)

- [x] `UiPreview.exe` host aligned with SmartGisViews pattern (no map).
- [x] Load markup via same pipeline as product.
- [x] Open / Save / Save As; watch xml/css hot-reload (keep selection id if present).

### Task 5: UiPreview editor (`ui-preview-editor`)

**Files:** palette, canvas select, properties, tree, drag insert/reorder

- [x] Left palette from factory registry.
- [x] Center canvas: select; drag-insert child; sibling reorder.
- [x] Right properties: text, id, class, common flex/size → DOM + immediate layout.
- [x] Node tree; editor-owned styles write `#id` rules to sibling `.ui.css`.

### Task 6: Migrate dialogs (`migrate-dialogs`)

**Files:** `add_basemap_dialog.*`, `att_struct_dialog.*`, `create_*_dialog.*`, `src/ui/resources/`

- [x] `load_markup` API wired; dialogs load resources + bind by id.
- [x] Sample `.ui.xml` / `.ui.css` checked in.
- [x] CreateDatasource / CreateLayer / CreateMap migrated to markup.
- [x] Win32 `FilePicker` / `MessageBox` remain non-markup.

### Task 7: Module README + verify (`docs-readme`)

**Files:** `src/ui/views/README.md`; build targets

- [x] Document `markup/` + `UiPreview.exe` in views README.
- [x] `build.bat` for touched targets green (views tests + ui_preview + dialogs).

---

## Acceptance (milestone)

- [x] Yoga layout unit tests green.
- [x] Factory covers README primitives (or documented stubs).
- [x] `UiPreview.exe`: open → edit → save → reopen consistent; css hot-reload.
- [x] AddBasemap + AttStruct + CreateDatasource/Layer/Map runtime via markup.
- [x] Touched `build.bat` targets green.
