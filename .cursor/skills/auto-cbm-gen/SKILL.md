---
name: auto-cbm-gen
description: >-
  Refresh the codebase-memory (CBM) index for this repo. Use when the user
  invokes /auto-cbm-gen, or says CBM index, 更新索引, codebase memory index
  update, or when auto-idle-pipeline reaches the CBM stage. MCP
  user-codebase-memory-mcp; project smartgis; root C:/Dev/src/gis/smartgis.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Auto CBM index update

**更新本仓库 codebase-memory 索引**

Independently callable (`/auto-cbm-gen`) and also stage 3 of `/auto-idle-pipeline`.

## Target

| | |
|---|---|
| MCP namespace | `user-codebase-memory-mcp` |
| Project | `smartgis` |
| Root | `C:/Dev/src/gis/smartgis` |

Windows Cursor store only. Do **not** use the WSL CBM store or invent a second project name.

## Authorization

When this skill is invoked, attached (`@auto-cbm-gen`), or followed by the idle pipeline, the agent **MUST** run the index steps below — do not defer reindex to the user.

## Steps (normative)

1. Discover tools if needed (`GetDynamicTools` on `user-codebase-memory-mcp`), then call via `CallDynamicTool`.
2. `list_projects` — confirm project `smartgis` with `root_path` `C:/Dev/src/gis/smartgis` (normalize path separators). If missing, still proceed with `index_repository` using the table above.
3. `index_repository` with:
   - `repo_path` = `C:/Dev/src/gis/smartgis`
   - `name` = `smartgis`
   - `mode` = `full` (prefer; use `moderate` only if full is too heavy / times out)
   - `persistence` = `false`
4. `index_status` (project `smartgis`) and briefly report result in **简体中文**.

Optional after index: `check_index_coverage` only if the user asked for coverage proof or a prior claim needs verification.

## Done bar

- `index_repository` completed without hard error.
- `index_status` reported (status / freshness summary to the user in 简体中文).

## Hard stops

- MCP namespace missing, unreachable, or stuck in `needsAuth` / `error` after one auth attempt (`mcp_auth` via `CallDynamicTool` with empty args) — report and **stop**.
- `index_repository` fails or times out repeatedly — report; do not invent a local fake index.
- Wrong project / root detected (e.g. WSL vs08 `smartgis`) — **stop**; do not index the wrong tree.

## Constraints

- Do **not** auto-reindex outside this skill, `/auto-idle-pipeline` stage 3, or an explicit user ask.
- Do **not** change product C++ / build just to “refresh” the graph.
- Progress in **简体中文**; identifiers in **English**.
