<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/base` Hybrid 全量替换（原「仓库根 `base/`」）

**Date:** 2026-09-14  
**Status:** accepted（Phases 0–6 consolidator 已收口；**2026-09-15 amendment：** foundation 真源从仓库根迁入 `src/base/`）  
**Goal:** 将遗留 `src/base/core`（`Smt*`）对照 mogu **全部**替换到 foundation 树；制图 style / `sys` / `net` 留在产品层；分期 strangler，阶段末不留旧名转发壳。  
**Related:** [`../../build/src-layout.md`](../../build/src-layout.md)、[`../../build/mogu-mapping.md`](../../build/mogu-mapping.md)、[`../../build/abi-rename-map.md`](../../build/abi-rename-map.md)、[`2026-09-13-code-style-include-abi-cutover-design.md`](2026-09-13-code-style-include-abi-cutover-design.md)、[`2026-09-13-base-archive-design.md`](2026-09-13-base-archive-design.md)、[`2026-09-13-base-ipc-mojom-design.md`](2026-09-13-base-ipc-mojom-design.md)、[`2026-09-14-dll-reorganization-design.md`](2026-09-14-dll-reorganization-design.md)  
**Plan (Cursor):** `base_root_hybrid_fd0c40fd.plan.md`（会话外；本仓以本 spec + `docs/build` 为准）

## Amendment (2026-09-15)

| 项 | 原 Phase 0–6 约定 | 现行 |
| --- | --- | --- |
| 真源落点 | 仓库根 **`base/`** | **`src/base/`**（与产品平台 DLL 同树；foundation 与 leftovers 并存） |
| 首选 GN | `//base:base` | **`//src/base:foundation`** |
| 仓库根 `base/` | 真源树 | **已删除**；兼容别名仅在根 `BUILD.gn`（`//:base`）与 `core/BUILD.gn`（`//core:core`） |
| Include | `BUILDCONFIG` 的 `//` 命中根 `base/` | **`//src` 优先** → `#include "base/..."` 仅命中 `src/base/` |

历史 Phase 叙述仍保留「根 `base/`」措辞，表示当时交付；以本节与 Locked decisions 现行表为准。

## Motivation

- 产品树已分层，但基础面曾是 2010 `Smt*`（`SmtLogManager` / `SmtCSLock` / `SmtDynLib` …），与 mogu foundation 不对齐。
- Hybrid 分期已把 mogu 式头面落到 foundation；产品约定是 **产品代码在 `src/`**，故 foundation 真源亦迁入 `src/base/`，仓库根不再放源码树。
- 「一层一 DLL」的产品 stem 与 foundation GN 标签曾易混；**磁盘 stem = `platform`**，foundation = `:foundation`（无 DLL）。

## Locked decisions（现行）

| Topic | Choice |
| --- | --- |
| 范围 | 整棵 `src/base/core` 对照 mogu 完成；不是单模块试点 |
| 手法 | **Hybrid**：薄面 port mogu header；厚/平台面按 mogu API 形状本仓 rewrite（Windows：`LoadLibrary`；无裸拷 `unistd`/`dlfcn`） |
| 落点 | 终局 foundation = **`src/base/`**；仓库根**无**物理 `base/`；兼容别名 `//:base` / `//core:core` |
| 边界 A — 进 foundation | `core`（headers）、`threading`、`files`、`memory`、`util`、`archive`、`ipc`；按需 `string` / `time` / `traits` / `container` 子集 |
| 边界 A — 留产品层 | 制图 pen/brush/Envelope → **`src/sdb/carto`**；`sys`、`net` 不动 |
| 硬排除 | 不搬 mogu `base::mutex`（新树 `std::mutex`）；不整棵搬 archive Json/Text/Yaml sink；不 vendor Chromium；不引入 Qt |
| ABI | 破 `Smt*`；**阶段末不留**旧名转发壳；日常改动在 `master` |
| 推进 | **分期 strangler**（每期绿再进下一期） |
| 链接 | `//src/base:foundation` = `source_set` 聚合（多为 header-only） |
| 产品 DLL stem | **`dll_stem=platform`** |

## DLL / 标签命名

| 名称 | 含义 | 磁盘 |
| --- | --- | --- |
| **`//src/base:foundation`** | mogu 式 foundation | **无**产品 DLL；静态链入消费方 |
| **`//:base`** / **`//core:core`** | 根 / `core` BUILD.gn 转发 → `:foundation` | 无物理 `base/` 目录 |
| **`//src/base:base`**（+ alias `:platform`） | 产品平台层 shared_library | **`dll_stem=platform`** → `platform.dll` / `platform_d.dll` |
| **产品平台 DLL 内容** | 剩余 core leftovers + carto + xml + `sys` + `net` | carto 仍链入本 DLL；archive/ipc 经 foundation / public_deps |

规则：

1. 文档与代码评论中区分 **GN 标签 `foundation` / 兼容 `//:base`** 与 **磁盘 `dll_stem=platform`**。
2. **`dll_stem=platform` 已落地**；`#pragma comment(lib, "platform…")` 已同批。
3. DLL reorg「一层一 DLL」对 **产品层** 仍有效。

## Include 根

| 根 | 用途 | 示例 |
| --- | --- | --- |
| `//src`（`BUILDCONFIG` 优先 + `//build:legacy`） | foundation + 产品树 | `#include "base/core/log.h"`；`#include "sdb/map/map.h"` |
| `//` | 仓库根辅助 | build helpers；**不再**承载 foundation 源码 |

## 目标布局

```text
src/base/                 # //src/base:foundation + //src/base:base (platform DLL)
  core/                   # foundation headers + deferred leftovers sources
  threading/ util/ files/ memory/ time/
  archive/                # //src/base/archive:archive
  ipc/                    # //src/base/ipc:ipc
# (no repo-root base/ — //:base and //core:core forward in BUILD.gn / core/BUILD.gn)
src/
  sdb/carto/  sys/  net/  plugin/
```

## 遗留 → 终局（摘要）

| 遗留 | 终局 | 手法 |
| --- | --- | --- |
| `log` / `logmanager` | `base/core/log.h` | Port + Win 适配 |
| `core_assert` / 宏面 | `base/core/{macros,debug,export,build_config}.h` | Port |
| `cslock` / `srwlock` / `trd_sync` | `std::mutex` / `shared_mutex` | Rewrite；**不** port `synchronization/mutex.h` |
| `thread` / `threadpool` / `workthread` | `base/threading` | Port 薄 API + rewrite |
| `filesys` | `base/files` + `base/util/path` | Hybrid |
| `dynlib*` | `base/util/library` | Port + `LoadLibraryW` |
| `plugin*`（core 内） | `base/util/plugin` + `src/plugin` | Hybrid |
| `mempool*` / `memshare` | `base/memory` 子集；共享内存可留 `src/sys` | Hybrid |
| `timer` | `base/time` | Port/rewrite |
| `listener` / `command` / `msg*` | 不进 foundation 聚合语义；deferred 仍在 `src/base/core` | Rewrite / 删除 |
| `xml*` | `src/legacy/xml` | 平移 |
| `winservice` | `src/sys` 或 `src/legacy` | 平移 |
| `matrix2d.h` | `algorithm` 或 `sdb` | 平移 |
| `src/base/style` | `src/sdb/carto` | 搬家 |
| archive / ipc | `src/base/archive`、`src/base/ipc` | 曾上移根 `base/`，现与 foundation 同回 `src/base` |

## Phases（历史；均已收口）

| Phase | 内容 | 绿门槛 |
| --- | --- | --- |
| **0** | 根 `base/` GN+README、include、DLL 命名文档、本 spec、`//core`→foundation | 当时 `//base:base` 可解析 |
| 1–4 | core / threading / path / memory+time | 相关单测 / `src_all` |
| 5 | archive/ipc 曾上移根 `base/` | 后经 amendment 迁入 `src/base` |
| 6 | carto、xml、`dll_stem=platform`、deferred leftovers | `build.bat` / `src_all` |
| **amendment** | 真源 `base/` → `src/base/`；`:foundation`；根物理 `base/` **删除**；`//:base` / `//core:core` 转发 | foundation + `platform_d.dll` + `ipc_test` |

## Deferred（有意未搬）

`listener` / `command` / `msg*` / `api` / `bas_struct` / `env_struct` / `core_assert` 仍在 `src/base/core`，因产品树大量 `#include "base/core/…"`。后续整批改 include 后再并入 `src/tool` / `src/plugin` / `src/sys`，或随调用方重写删除。

## 风险

- mogu `log.h` / `library.h` 含 POSIX；必须 Windows 适配。
- 勿把 `//src/base:foundation`（或兼容 `//:base`）与产品 `dll_stem=platform` 混称。

## 与既有 spec 的关系

- **修订** src-layout：foundation 真源仅在 `src/base/`；仓库根无物理 `base/`；产品平台 DLL **stem = `platform`**。
- **沿用** A1 archive、IPC named-pipe、mutex 规则、DLL「一层一 DLL」对产品层。
