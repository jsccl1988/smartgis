---
name: auto-cbm-gen
description: >-
  Use when the user asks to refresh the codebase-memory index, invoke
  /auto-cbm-gen, or says CBM index, 更新索引, or codebase memory index update.
  For this repo: MCP user-codebase-memory-mcp, project smartgis,
  root C:/Dev/src/gis/smartgis.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto CBM index update

**更新本仓库 codebase-memory 索引**

Namespace: `user-codebase-memory-mcp`.

| | |
|---|---|
| Project | `smartgis` |
| Root | `C:/Dev/src/gis/smartgis` |

## Steps (normative)

1. Call `list_projects` — confirm project `smartgis` with `root_path` `C:/Dev/src/gis/smartgis` (normalize path separators).
2. Call `index_repository` with:
   - `repo_path=C:/Dev/src/gis/smartgis`
   - `name=smartgis`
   - `mode=full` (prefer; use `moderate` only if full is too heavy)
   - `persistence=false`
3. Call `index_status` and briefly report the result in **简体中文**.

## Constraints

- Do **not** invent a second project name or WSL store; Windows Cursor project is `smartgis`.
- Do **not** auto-reindex outside this skill / explicit user ask.
- Progress in **简体中文**; identifiers in **English**.
