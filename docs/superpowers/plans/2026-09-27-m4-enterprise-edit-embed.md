<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# M4 — PostGIS/GPKG 冲突下限 + content:: 嵌入样例 Implementation Plan

> **For agentic workers:** Stay on **master**. Do **not** `git commit`. Do **not** create branches.

**Goal:** 两客户端先后编辑同一层有冲突提示或检出；`content::` 样例嵌入方能开图（最小 host 样例或测试）。

**Architecture:** 在 `gis::edit` 增加乐观版本令牌（feature 或 layer 级 `version` / `etag` 字段）；模拟双会话 `MemoryEditSession`：后写检测 stale → `ConflictError`。嵌入：`content/` 下最小示例程序或 `content_*_test` 演示 MapContents/ViewHost 开图。PostGIS 真连可选；默认用内存双会话模拟验收口令。

**Tech Stack:** C++23、`gis/model/edit`、`content`、GN。

## Path ownership（并行硬约束）

**MAY edit:**
- `src/gis/model/edit/**`
- `src/content/**`（样例 / 测试 / 公共小 API；勿改 PluginHost Processing 语义）
- `src/gis/datasource/**`（仅若需 GPKG 字段 version；优先 edit 层）
- `docs/superpowers/plans/2026-09-27-m4-enterprise-edit-embed.md`
- 相关 BUILD.gn / `*_test.cc`

**MUST NOT edit:**
- `src/app/views/**`（含 main/browser_view/map_scene/scene3d）
- `src/ui/views/**`
- `src/plugin/processing/**`
- `src/gis/scene/world/**`、`src/render/**`
- M2/M3 计划文件

## Acceptance

1. 双会话：A 读 version=1 写成功 → version=2；B 仍持 version=1 写失败并带冲突信息。
2. 单元测试 `edit_conflict_test`（或扩现有 edit 测）PASS。
3. `content` 嵌入样例：可执行测试或 `examples/` 级源文件证明宿主能 `open`/绑 ViewHost（Windows console test OK）。
4. 暴露 C API 或测试入口说明，供父 agent 在 docs / 可选 main 调用；**不**改 `main.cc`。

## Tasks

- [x] Task 1: EditSession 版本令牌 + conflict 结果类型。
- [x] Task 2: 双会话测试绿。
- [x] Task 3: content 嵌入最小样例/测试。
- [x] Task 4: 文档预留 exit 100–101；不改 main.cc。

## Constraints

- 不做完整 branch versioning / Portal。
- 禁止 Qt；`snake_case`；注释英文；版权 2026。

## Landed API (2026-09-27)

### Conflict / optimistic version

- `gis/model/edit/session/edit_session.h` — `EditOp`, `CommitStatus`, `ConflictError`, `FeatureMutation`, `EditSession`
- `gis/model/edit/session/optimistic_layer_store.h` — `OptimisticLayerStore` (`seed_feature`, `feature_version`, `layer_version`, `try_commit`)
- `gis/model/edit/session/memory_edit_session.h` — `MemoryEditSession(shared_ptr<OptimisticLayerStore>)`; `commit_optimistic` / `last_conflict` / `last_status`
- `gis/model/edit/session/command_edit_session.h` — `CommandEditSession` (SmtCommandManager log)
- `gis/model/edit/map_edit/map_edit_session.h` — `MapEditSession`; OGR feature kept by `host_token`, not `void*`
- `FeatureMutation::base_version` — optimistic token on commit

### Embed sample (`content/public/embed_sample.h`)

- Entry: `content::open_map_host_path(EmbedMapHost*, const char* path)`
- Binds `ViewHost` + records path; product hosts add `MapContents::OpenView` after a live GPU pipe (sample avoids pipe send hang).
- Test: `content_embed_sample_test` (`out/content_embed_sample_test.exe`)
- Conflict test: `edit_conflict_test` (`out/edit_conflict_test.exe`)

### Reserved exit codes (do not wire in `main.cc` here)

| Code | Meaning |
| --- | --- |
| **100** | Edit conflict (`content::kExitEditConflict`) |
| **101** | Embed `open_map_host_path` failure (`content::kExitEmbedOpenFailed`) |

### Verify

```bat
ninja -C out edit_conflict_test content_embed_sample_test
out\edit_conflict_test.exe
out\content_embed_sample_test.exe
```
