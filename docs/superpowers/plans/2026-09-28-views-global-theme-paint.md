<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views global theme paint — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** `PainterRegistry` + `PaintDelegate` pipeline; builtin painters for all toolkit primitives/frame; plugin register/withdraw; `UiDesigner` on CSD + ThemeService.

**Architecture:** Recording path runs before → role painter (or legacy `paint_self`) → after. Builtins registered by string role. Plugins register via `PainterRegistry` (plugin-id tagged); `PluginHost` records installer + withdraw hook without depending on views.

**Spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Global theme paint.

**Tech Stack:** C++23, Views + Skia/GDI canvas, GN `out/Debug|Release`.

## Global Constraints

- Stay on `master`; no feature branch.
- No Qt / WinUI.
- `snake_case`; namespaces ≤ two public layers (`ui::views`).
- Copyright **2026**; English comments.
- `build.bat debug` for touched targets.

## File map

| Path | Role |
| --- | --- |
| `src/ui/views/kernel/paint/painter.h` | `Painter`, `PaintDelegate` |
| `src/ui/views/kernel/paint/painter_registry.{h,cc}` | Registry + plugin withdraw |
| `src/ui/views/kernel/view/view.{h,cc}` | `paint_role`, delegate, pipeline in record |
| `src/ui/views/kernel/paint/register_default_painters.{h,cc}` | Builtin RoleForwardPainter install |
| Primitive / frame `*.cc` | `paint_role()` only (draw stays in `paint_self`) |
| `src/content/public/plugin_host.h` + browser impl | `contribute_painter` + withdraw hook |
| `src/app/ui_designer/` | FrameView + theme |
| `src/ui/views/testing/unit/views_unittests.cc` | Registry + delegate tests |

---

### Task 1: Kernel registry + View pipeline

- [x] Add `painter.h`, `painter_registry.h/.cc`; GN `views_kernel` sources.
- [x] `View`: `paint_role()`, `set_paint_delegate`, pipeline in `ensure_commands_recorded`.
- [x] Unit test: register painter for role; delegate before/after order.

### Task 2: Builtin painters (all primitives + frame)

- [x] `register_default_painters()` in `kernel/paint/`; call from `Widget::init`.
- [x] `paint_role()` on toolkit controls + frame/splitter.
- [x] Theme change still invalidates paints (existing ThemeObserver).

### Task 3: PluginHost contribute_painter

- [x] `contribute_painter(plugin_id, role, installer)` + `set_ui_withdraw_hook`.
- [x] `withdraw` invokes hook → `PainterRegistry::withdraw_plugin`.
- [x] Wire hook from `PluginShell`.

### Task 4: UiDesigner CSD + theme + rename docs

- [x] `UiDesigner` root `FrameView`, `frame_kind=custom`, ThemeService startup.
- [x] Rename polish in views README / samples as touched.

### Task 5: Verify

- [x] `PainterRegistry` + delegate unit smoke green.
- [x] `build.bat debug UiDesigner` links.
- [ ] Full `views_unittests` suite (pre-existing Widget teardown AV under agent run — investigate separately).
