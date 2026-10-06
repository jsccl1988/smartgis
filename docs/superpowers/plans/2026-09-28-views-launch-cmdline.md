<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Views launch / CLI11 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** CLI11-parse Views PE switches; slim `main.cc` via `app/{cmdline,process,host}` + `harness/{showcase,self_test}`.

**Architecture:** App owns `ViewsLaunchOptions`; content dispatches on `ContentMainParams.process_type` when set. Showcase/self-test stay feature modules under `harness/`.

**Tech Stack:** CLI11 (header-only), GN, existing `content::content_main`, `app::BrowserView`.

**Spec:** [`../specs/2026-09-27-views-desktop-shell-design.md`](../specs/2026-09-27-views-desktop-shell-design.md) §Process entry / CLI11.

## Global Constraints

- Work on `master` only; no feature branch.
- Agent does not run `build.bat` / ninja (human compiles).
- Public C++ namespace: `app` (two layers); functions `snake_case`.
- No Qt; Views + Skia shell unchanged.

## File map

| Path | Role |
| --- | --- |
| `third_party/manifest.json` + `third_party/CLI11/BUILD.gn` | Vendor CLI11 |
| `src/content/app/content_main.h` | `process_type` / `process_type_set` |
| `src/app/views/app/cmdline/*` | Parse API + unit test |
| `src/app/views/app/process/browser_main.*` | Browser process body |
| `src/app/views/app/host/content_host.h` | Host adapter |
| `src/app/views/harness/showcase/*` | Atmosphere / map2d / ui / input showcase |
| `src/app/views/app/main.cc` | Thin `wWinMain` |
| `src/app/views/BUILD.gn` | Sources + `//third_party:CLI11` |

---

### Task 1: Vendor CLI11

- [x] Add manifest package `CLI11` (tag pin, `install_skip`, GitHub fallback)
- [x] `py -3 third_party/tools/fetch.py --package CLI11`
- [x] Thin GN + `//third_party:CLI11` / `gn:CLI11`

### Task 2: content_main params

- [x] Extend `ContentMainParams`; prefer set field in `content_main`
- [x] Keep argv fallback for `content_main_test`
- [x] Assert `process_type_set` path in that test

### Task 3: cmdline module

- [x] `ViewsLaunchOptions` + `parse_views_launch_options`
- [x] `views_launch_options_test`

### Task 4: Extract browser_main + showcase

- [x] Move showcase out of `main.cc`
- [x] `run_browser_main` + `ViewsContentHost`
- [x] Slim `main.cc`; update `BUILD.gn`

### Task 5: Human verify

```bat
.\build.bat
.\build.bat views_launch_options_test
.\build.bat te
.\build.bat e2e
```
