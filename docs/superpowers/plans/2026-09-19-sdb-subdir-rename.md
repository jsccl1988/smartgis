<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# sdb → gis 子目录重命名 Implementation Plan

> **For agentic workers:** 按任务顺序执行；规格：[`../specs/2026-09-19-sdb-subdir-rename-design.md`](../specs/2026-09-19-sdb-subdir-rename-design.md)。**不要 commit**，除非用户明确要求。工作在 **master**。

**Goal:** 落地 options 1–5：carto→base；scene→world；model→assets；文档澄清 gis≠DB；整层 sdb→gis。

**Architecture:** 物理 `git mv` 保历史 → 批量改 include/GN/namespace → 文档与 abi 图 → 编译验证。

**Tech Stack:** GN/Ninja、`build.bat`、CBM 发现、PowerShell/Python 批量替换。

---

### Task 1: carto → `src/base/carto`

- [x] `git mv src/sdb/carto src/base/carto`
- [x] `//src/base/BUILD.gn` deps → `//src/base/carto:carto_sources`
- [x] 全仓 `"sdb/carto/` → `"base/carto/`；GN → `//src/base/carto`

### Task 2: scene→world、model→assets

- [x] `git mv src/sdb/scene` → `world`；`model` → `assets`
- [x] GN `world_sources` / `assets_sources` + 旧名 group 别名

### Task 3: 树改名 `src/sdb` → `src/gis`

- [x] 逐子项迁入 `src/gis`（整目录 rename 曾 Permission denied）
- [x] `smt_shared_library("gis")`，`dll_stem = "gis"`，group `gis_all`
- [x] include / GN 批量 `"sdb/` → `"gis/`、`//src/sdb` → `//src/gis`

### Task 4: 命名空间

- [x] `namespace sdb` → `namespace gis`
- [x] 去掉 `world/`、`assets/` 内 `scene` / `model` 嵌套（升到 `gis`）
- [x] `sdb::style::` → `gis::style::`；`#pragma comment(lib, "gis…")`

### Task 5: 文档

- [x] `docs/build/src-layout.md`、`abi-rename-map.md`
- [x] `docs/README.md` 索引；`src/README.md`；相关活跃 specs 路径

### Task 6: 验证

- [x] 产品树无 `src/sdb/`；`#include "sdb/` = 0
- [x] `gis_d.dll` 链接成功；`scene_test` / `model_test` / `tile_test` / `style_test` / `land_mask_test` 通过
- [x] `platform_d.dll` 曾 LNK1168（占用）；未强杀；后续重试成功更新
