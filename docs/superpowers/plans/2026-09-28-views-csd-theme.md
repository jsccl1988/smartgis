<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views CSD title bar + ThemeService — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Self-drawn window frame (CSD) for main shell + dialogs, and extensible Light/Dark theme packs with View menu + Preferences.

**Architecture:** `ThemeService` owns packs and observers; `Theme::current()` reads the active snapshot. `FrameView` draws caption/buttons; `Widget` `frame_kind=custom` collapses NC via `WM_NCCALCSIZE` and routes drag/resize via `WM_NCHITTEST`.

**Tech Stack:** C++23, Views + Skia/GDI canvas, Win32 HWND, GN `out/Debug|Release`.

**Spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Custom frame + ThemeService.

## Global Constraints

- Stay on `master`; no feature branch.
- No Qt / WinUI frame path.
- New-tree `snake_case`; namespaces ≤ two public layers (`ui::views`).
- Copyright year **2026**; English comments.
- `build.bat debug` / ninja under `out/Debug`.

## File map

| Path | Role |
| --- | --- |
| `src/ui/views/kernel/shell/theme.{h,cc}` | Snapshot + `Theme::current()` → service |
| `src/ui/views/kernel/shell/theme_service.{h,cc}` | Packs, set_theme, observers, persist |
| `src/ui/views/kernel/frame/frame_view.{h,cc}` | Caption + client slot |
| `src/ui/views/kernel/frame/caption_button.{h,cc}` | Min / max / close |
| `src/ui/views/kernel/widget/widget.{h,cc}` | `frame_kind`, NC messages |
| `src/ui/views/dialogs/dialog.cc` | Custom frame for modals |
| `src/app/views/ui/browser_view.*` | Root FrameView; theme commands |
| `src/app/views/browser/commands/view_commands.*` | View → Theme / Preferences rows |
| `src/ui/views/BUILD.gn` | New sources |
| `src/ui/views/testing/unit/views_unittests.cc` | ThemeService + FrameView smoke |

---

### Task 1: ThemeService + Light/Dark packs

- [x] Add `theme_service.h/.cc`; register `dark` / `light`; persist id.
- [x] Point `Theme::current()` at the active pack.
- [x] Unit test: set_theme switches accent/bg; unknown id fails.

### Task 2: FrameView + CaptionButton

- [x] Caption strip (title + buttons); client flex child.
- [x] `caption_height_px()`, `point_in_caption_controls()`, maximize toggle API.
- [x] Paint from `Theme::current()`.

### Task 3: Widget custom frame

- [x] `InitParams::frame_kind`; custom styles; skip AdjustWindowRect expansion.
- [x] `WM_NCCALCSIZE` / `WM_NCHITTEST` / double-click caption maximize.
- [x] Theme observer → `schedule_paint`.

### Task 4: Wire shell + dialogs + menus

- [x] `BrowserView` root `FrameView`; `frame_kind=custom`.
- [x] `Dialog::run_modal` custom frame (no maximize).
- [x] View menu Theme Dark/Light + Preferences (`SelectOneDialog`).
- [x] `build.bat debug` views / app targets green.
