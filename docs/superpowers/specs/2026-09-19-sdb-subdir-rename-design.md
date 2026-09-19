<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# sdb 子目录重命名 + 层改名 `gis`（设计）

Status: active  
Date: 2026-09-19  
Plan: [`../plans/2026-09-19-sdb-subdir-rename.md`](../plans/2026-09-19-sdb-subdir-rename.md)

## 背景

`src/sdb` 名义上像 “Spatial DB”，实际是 **GIS 模型层**（feature / map / datasource / style / tile / …），不是字面数据库。`carto` 早已链入平台 DLL；`scene` / `model` 目录名与职责（World / CPU assets）也不对齐。用户选定 options **1–5** 一并落地。

## 决策（锁定）

| # | 决策 | 终态 |
| --- | --- | --- |
| 1 | `src/base/carto` → `src/base/carto` | include `"base/carto/..."`；仍链入 `//src/base:base`（`dll_stem=platform`）；命名空间保持 `base` |
| 2 | `src/gis/world` → `…/world` | 目录 `src/gis/world`；职责 = 逻辑 World |
| 3 | `src/gis/assets` → `…/assets` | 目录 `src/gis/assets`；职责 = CPU mesh / tileset assets |
| 4 | 语义澄清 | **`gis` = GIS 模型层**，不是 literal DB；文档写明 |
| 5 | 整层 `sdb` → `gis` | 目录 `src/gis/`；include `"gis/..."`；命名空间 `gis::`；GN `//src/gis`；`dll_stem=gis` |

保留在 `gis` 树下：`style` / `tile` / `edit` / `feature` / `map` / `datasource` / `crs` / `layer`。

## 命名空间（两层规则）

遵循 `.cursor/rules/style/namespace.mdc`：

| 原 | 新 | 说明 |
| --- | --- | --- |
| `gis::` | `gis::` | 层根 |
| `gis::*` | `gis::*` | **扁平**：不引入第三语义层 `gis::world::*`；目录仍可叫 `world/` |
| `gis::*` | `gis::*` | **扁平**：目录 `assets/` |
| `gis::style::*` | `gis::style::*` | 两层模块身份，保留 |
| `base::`（carto） | `base::` | 不变 |

内部实现可放 `gis::detail`；本轮不借机大拆类型。

## Include / GN / DLL

- Include：`"gis/..."` → `"gis/..."`；carto → `"base/carto/..."`。
- GN：`//src/gis` → `//src/gis`；`smt_shared_library("gis")` + `dll_stem = "gis"`（Debug `gis_d.dll`）。
- Export：继续 `GIS_EXPORT` / `GIS_EXPORTS`；构建时可保留 `SDB_EXPORTS` 作同义 define 过渡；`#pragma comment(lib, …)` 改为 `gis.lib` / `gis_d.lib`。
- **干净打断**：树内不留 `src/gis/` 转发树；插件/文档按 abi 图更新。无树外 re-export shim（除非后续发现外部插件硬依赖 `gis.dll`——届时再记 abi 图）。

## 有意保留的 `sdb` 字符串

- GDAL 驱动名 / 类型前缀 **`SDBD`** / `sdbd_*` 文件名（OGR 连接串 `SDBD:…`）——产品 I/O ABI，**不**随层改名。
- 历史 commit / archive 文档中的旧路径可保留；**活跃** `docs/build/` 与未归档 superpowers 就地改写。

## 非目标

- 不改 `src/render/scene`（GPU scene）。
- 不改 `src/ui/views/gis` 面板目录名（与层 DLL 无关）。
- 不借机重写 feature/map ABI、不引入 Qt、不自动重建 CBM 索引。
- 不强制把 `Smt*` 虚表 ABI 改成 snake_case（既有 cutover 另案）。

## 验收

- 无产品 `src/gis/` 树。
- `carto` 在 `base`；`world` / `assets` 目录就位；层名 `gis`。
- `docs/build/src-layout.md`、`abi-rename-map.md`、`docs/README.md` 索引行已更新。
- 关键目标：`//src/gis:gis` 及原 sdb 单测在环境允许下可编过。
