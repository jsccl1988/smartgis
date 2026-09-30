<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/core` subdirectory layout Implementation Plan

> **For agentic workers:** Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Header-only `src/legacy/core` with STL internals; scheme C includes; `core_sources` stays a (empty) face of `base.dll`.

**Architecture:** Living §11b / §11b.1 / §11b.2.

## Done when

- [x] Tree = modules under `legacy/core` (headers only)
- [x] No `.cpp` under `legacy/core/`
- [x] Free helpers `inline` + STL; listener/command header-only (no `BASE_EXPORT`)
- [x] `build.bat debug menu_test` green
- [x] §11b.2: `Rect` owns normalize/contains/cast_to; eps/`is_equal` in macros; deleted `util/{geom,math,variant}.h`; no `GetAppPath` alias
