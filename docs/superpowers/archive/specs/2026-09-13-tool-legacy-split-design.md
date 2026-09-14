<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Split leftover IATool into `src/legacy/tool`

**Date:** 2026-09-13  
**Status:** landed  
**Related:** dispatch [`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md)；布局 [`../../build/src-layout.md`](../../build/src-layout.md)；对称先例 [`2026-09-13-render-legacy-split-design.md`](2026-09-13-render-legacy-split-design.md)。  
**Scope:** 物理子目录重构（方案 1）：leftover 整包平移到 `legacy/tool/` + include/GN 全改名，**不留** `src/tool/<leftover>` 转发头。

## Goal

`src/tool/` 只保留终局 session dispatch（`command` / `interaction` / `workspace` / `gestures` / `legacy_msg`）。2010 leftover `SmtIATool` / `SmtGroupTool` 整包迁到 **`src/legacy/tool/`**（含 `group/`），默认 **不进** `src_all`。调用方 include 与 GN label 同步改为 `legacy/tool/…`。

## Non-goals

- 不改 leftover `dll_stem` / `Smt_*` 导出 ABI（仅路径与 GN label）。
- 不借机重写 leftover 工具业务逻辑。
- 不按 view/select/edit 再拆 `group/` 子目录。
- 本轮 **不要求** `smt_build_app` / 依赖 leftover 的 plugin、xview 全绿（允许断；另编 `legacy_tool_all`）。
- 不加 Qt；不把 `EventBus` 放进 `tool/`。

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| 迁出范围 | `t_iatool*` / `t_iatoolmanager*` / `t_msg*` / `tool_export.h` + 现有 `tool/group/**` |
| 落点 | `src/legacy/tool/` + `src/legacy/tool/group/` |
| 终局 | `src/tool/` 扁平保留 dispatch 源（公共头仍为 `tool/command.h` 等） |
| 改名策略 | **A**：无转发头；`"tool/t_*.h"` / `"tool/group/…"` → `"legacy/tool/…"` |
| `src_all` | 只含终局 `//src/tool:dispatch`（不再含 leftover `//src/tool:tool`） |
| 可选编 | `//src/legacy/tool:legacy_tool_all`（`tool` + `group`；`group` 仍可按 `smt_build_app` 门控） |
| GN 目标名 | leftover 保持 `tool` / `tool_group` label，路径改为 `//src/legacy/tool:tool`、`//src/legacy/tool/group:tool_group` |

## Target tree

### `src/tool/`（终局）

- `command.*` / `interaction.*` / `workspace.*` / `gestures.*` / `legacy_msg.*`
- `dispatch_test.cc`、`BUILD.gn`（`source_set("dispatch")` + test）、`README.md`

### `src/legacy/tool/`（leftover）

- `t_iatool.*` / `t_iatoolmanager.*` / `t_msg.*` / `tool_export.h`
- `BUILD.gn` → `smt_shared_library("tool")`（`dll_stem` 不变）
- `group/**` → 原 `tool/group` 整包；`BUILD.gn` → `smt_shared_library("tool_group")`
- 根 `BUILD.gn` 另提供 `group("legacy_tool_all")`

## Call-site rewrite

| From | To |
| --- | --- |
| `#include "tool/t_….h"` | `#include "legacy/tool/t_….h"` |
| `#include "tool/group/…"` | `#include "legacy/tool/group/…"` |
| `//src/tool:tool` | `//src/legacy/tool:tool` |
| `//src/tool/group:tool_group` | `//src/legacy/tool/group:tool_group` |
| `//src/tool:dispatch` | **unchanged** |

## Docs to update (same change)

- `docs/build/src-layout.md`（Tool 行 + path map）
- `src/README.md`、`src/tool/README.md`
- `docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md` leftover 路径行（修订 in place）
- 根 `BUILD.gn` 的 `tool_group` 门控 deps 指向新 label

## Done when

- `src/tool/` 无 `t_*` / `group/`
- `src_all` 仅依赖 `//src/tool:dispatch`
- 仓库内无残留 `"tool/t_` / `"tool/group/` include（除历史 archive 外）
- `ninja -C out legacy_tool_all` 可加载（app 全绿非本轮硬门）
