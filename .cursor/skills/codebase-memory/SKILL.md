---
name: codebase-memory
description: >-
  Graph-first code lookup for this SmartGIS repo via codebase-memory-mcp.
  Use when exploring architecture, finding functions, tracing callers,
  searching code, 查找代码, Grep/Glob, impact, or dead code. Must use CBM before
  repo-wide Grep.
---

<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SmartGIS CBM

Code lookup **must** use CBM. Namespace: `user-codebase-memory-mcp`.

| | |
|---|---|
| Project | `smartgis` |
| Root | `C:/Dev/src/gis/smartgis` |
| Ignore | `.cbmignore` (`out/`, `third_party/.src`, `windows_app_sdk`, …) |
| Windows exe | `C:/Users/LENOVO/.local/bin/codebase-memory-mcp.exe` |

WSL CBM is another store; its `smartgis` is vs08. Do not use it here.

## Workflow

1. `list_projects` — confirm `root_path`.
2. `search_graph(project="smartgis", name_pattern="...")`.
3. `get_code_snippet` / `trace_path(direction="both")`.
4. `check_index_coverage` on cited paths. `parse_partial` is common in MFC/macros — grep those ranges if the claim is about that code.

Index only when the user asks (`index_repository` `name=smartgis`, `persistence=false`). Do not auto-reindex.

Grep/Glob only if CBM is down, the user gave an exact path, or the search is already scoped (`path` required).

Vendor matrix: user skill `cbm-code-discovery` and `~/.cursor/skills/codebase-memory/SKILL.md`.
