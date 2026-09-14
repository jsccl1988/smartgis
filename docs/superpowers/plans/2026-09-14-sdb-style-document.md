<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Sdb Style Document Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Land `sdb::style` — MapLibre-subset Style JSON, external SymbolLibrary, filter/zoom RuleEngine, and bridge to `base::SmtStyle`.

**Architecture:** New module `src/sdb/style/` in the `sdb` DLL. `base/style` stays POD-only. Render consumes resolved paint / legacy style only.

**Tech Stack:** C++23, hand-rolled JSON (tileset-style), GN `source_set` → `//src/sdb:sdb`, gtest via `testing/test.gni`.

## Global Constraints

- Namespace `sdb::style` (two public layers; internals in `sdb::style::detail`).
- Functions `snake_case`; types PascalCase.
- Comments in English.
- No Qt; no new JSON third_party; no `src/style/` top-level; do not move Envelope out of `base/style` in this plan.
- Work on `master` only.

## File map

| File | Role |
| --- | --- |
| `src/sdb/style/style_types.h` | LayerType, FilterNode, StyleLayer, StyleDocument, ResolvedPaint, SymbolEntry |
| `src/sdb/style/style_document.h/.cc` | parse / serialize Style JSON |
| `src/sdb/style/symbol_library.h/.cc` | manifest + root path lookup |
| `src/sdb/style/style_rules.h/.cc` | filter eval, zoom match, resolve |
| `src/sdb/style/paint_resolve.h/.cc` | color parse + `to_smt_style` |
| `src/sdb/style/json_mini.h/.cc` | detail mini parser shared by document + manifest |
| `src/sdb/style/BUILD.gn` | `style_sources` + `style_test` |
| `src/sdb/style/style_test.cc` | unit tests |
| `src/sdb/style/README.md` | module note (ZH) |
| `src/sdb/BUILD.gn` | dep on `:style_sources` |
| layout / docs index | `src-layout.md`, `src/README.md`, `base/README.md`, `docs/README.md` |

---

### Task 1: Types + mini JSON + StyleDocument

**Files:** `style_types.h`, `json_mini.*`, `style_document.*`, `BUILD.gn` scaffold

- [x] Add headers/types for document + filter AST
- [x] Mini JSON object/array/string/number/bool
- [x] `parse_style_document` / `serialize_style_document` for v1 subset
- [x] Wire `style_sources` into `//src/sdb:sdb`

### Task 2: SymbolLibrary (parallel-safe after types)

**Files:** `symbol_library.*`

- [x] `load_manifest`, `set_root`, `find`
- [x] Test via shared `style_test`

### Task 3: RuleEngine + paint bridge (parallel-safe after types)

**Files:** `style_rules.*`, `paint_resolve.*`

- [x] `eval_filter`, `layer_matches_zoom`, `select_layers`, `resolve`
- [x] `parse_color`, `to_smt_style`
- [x] Tests: filter/zoom/resolve/bridge

### Task 4: Docs + MapLayer thin hook

- [x] Update layout READMEs / docs index
- [x] Optional: `MapLayer` `style_document` shared_ptr setter/getter
- [x] `build.bat` / ninja `style_test` green

---

## Parallel note

Tasks 2 and 3 may land in parallel once Task 1 types + `json_mini` land. Task 4 last.
