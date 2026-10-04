<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `legacy/ui` capability layout (Approach C′) — Implementation Plan

**Status:** landed (archived 2026-10-03 — checkboxes complete)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** Capability dirs + top-level `res/<cap>/`; split `ambox`/`dock`/`grid`; snake_case file stems; freeze `dll_stem=ui_legacy`.

**Done when:** layout matches README; `build.bat debug ui_legacy` green.

## File map

```
shell/ viewport/ panels/ ambox/ catalog/ dialogs/ dock/ grid/ widgets/ chart/
res/{shell,catalog,dialogs,ambox,widgets,chart}/
```

## Tasks

- [x] Promote `*/res/` → `res/<cap>/`; fix `.rc` paths (forward slashes)
- [x] Split `ambox/` · `dock/` · `grid/` from `panels/` / `widgets/`
- [x] Rename stems (`shell`, `ambox_*`, `catalog_*`, `dialogs_api`, `chart_view_dlg`, `widgets_core`, …)
- [x] GN `*_sources` + root `ui_legacy` deps; historical group aliases
- [x] Include sweep; README + src-layout + umbrella §
- [x] `build.bat debug ui_legacy` green
