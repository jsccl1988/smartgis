<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP5 — Shell 编译闸门

**Status:** active  
**Date:** 2026-09-19  
**Goal:** 默认产品壳（Views / `build.bat app`）与 `//src:src_all` **不**被迫依赖 leftover 聚合目标（`legacy_*_all` / MFC exe / `ui_legacy` DLL）；leftover 仍可通过显式 opt-in 命令编译；文档与 GN 注释/`assert_no_deps` 把闸门钉死。  
**Related:** [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md)（SP0）、[`../../build/src-layout.md`](../../build/src-layout.md)、[`2026-09-14-app-legacy-split-design.md`](2026-09-14-app-legacy-split-design.md)、[`2026-09-13-ui-views-mfc-migration-design.md`](2026-09-13-ui-views-mfc-migration-design.md)  
**Plan:** [`../plans/2026-09-19-shell-compile-gate.md`](../plans/2026-09-19-shell-compile-gate.md)

## Non-goals

- **不删除** `src/legacy/**` 源码；不借机停编全部 leftover TU。
- **不破** leftover `dll_stem` / `Smt_*` 导出（本子规格无 leftover-only break）。
- 禁止 Qt；不把 WinUI / CEF / MFC 升为默认壳。
- **不**大改 `scene3d` 业务（属 SP4）；本轮只文档化 Views 对 `dem_height_field_static` 的窄依赖例外。
- **不**一次拆爆 `test_all` / 整仓 GN；增量分离 product vs leftover 测试组即可。

## Locked decisions

| Topic | Choice |
| --- | --- |
| 默认壳 | `build.bat app` / `views` → `//:views` → `SmartGisViews.exe` |
| 默认库图 | `ninja -C out all` / `//:all` → `//src:src_all`（终局 DLL + `content` / `dispatch` / AuxModule `plugin`） |
| 禁止默认拉入 | `legacy_render_all` / `legacy_tool_all` / `legacy_app_all` / `legacy_ui_all` / `ui_legacy` / MFC `SmartGis.exe` |
| Opt-in leftover | `build.bat legacy_app` / `ui_legacy` / `ninja -C out legacy_render` / `legacy_tool` / `legacy_all` |
| GN 闸门手法 | `assert_no_deps` + 根/`src` group 注释；`//:legacy_all` 显式 opt-in 聚合 |
| Views 例外（已收口） | SP4 第三刀后 Views **不**链 `dem_height_field_static`；走 `gis::DemRaster` |
| `test_all` | 可继续含 leftover paint 单测；另提供 `test_shell`（仅终局/壳相关）供闸门验证 |

## Path ownership

| May edit | Must not edit (siblings) |
| --- | --- |
| 根 `BUILD.gn`、`src/BUILD.gn`、`src/app/views/BUILD.gn`（仅闸门注释/`assert_no_deps`/group） | `src/legacy/**` 业务逻辑 |
| `docs/build/*`、根 `README.md`、`docs/README.md`（索引行） | SP4 `scene3d` 大规模迁 World/GpuScene |
| 本 spec + plan；伞状 Child 表 SP5 行 | 借机改 SP1/SP2/SP3 热点实现 |

## Dependency

```
build.bat app | views
        → //:views → SmartGisViews
        → 终局 content / render / tool::dispatch / ui::views
        ✗ 不得 deps → legacy_*_all / ui_legacy / SmartGis.exe

//:all → //src:src_all
        ✗ 不得 deps → legacy_render|tool|app|ui 聚合 / MFC UI DLL

//:legacy_all（opt-in）
        → legacy_render_all + legacy_tool_all + legacy_app_all + ui_legacy …
```

终局 **不得** `#include "legacy/…"`。Views DEM 经 `gis::DemRaster`（SP4 第三刀已切）。

## ABI

- leftover 导出与 `dll_stem`：**不变**。
- 本轮仅改 GN group / 文档；无产品 DLL stem 变更。

## Success criteria

1. [x] Living design 含 Goal / Non-goals / Locked / Path / Dependency / ABI / Success / Out of scope。  
2. [x] `//src:src_all` 带 `assert_no_deps`，禁止 leftover DLL / `legacy_*_all` / MFC app。  
3. [x] `//src/app/views:views`（及 `SmartGisViews`）`assert_no_deps` 禁止 `legacy_render` DLL / tool / app / ui_legacy / `dem_height_field_static`。  
4. [x] 根 `//:legacy_all` opt-in 聚合存在；`docs/build/src-layout.md` 有 Shell 闸门专节与 opt-in 命令表。  
5. [x] `//:test_shell`（或等价）不含 leftover paint / `dem_stereo` / MFC app 测试。  
6. [x] `build.bat app`（或 `views`）与 `ninja -C out src_all`（经 `all`）编通；相关壳单测可跑。  
7. [x] 伞状 Child 表 SP5 行指向本文；根 README「最后更新」在触及构建叙述时刷新。

## Out of scope for later

- SP4：Views 已去掉 `dem_height_field_static`（`gis::DemRaster`）；leftover `SmtScene` octree 仍可继续变薄。  
- 从 `test_all` **删除** leftover 测试（可保留为 opt-in 质量网）。  
- 删除 leftover 源码或默认停编全部 legacy TU。

## Done when（本增量）

- Spec + plan 落地；GN 闸门 + 文档 + 构建验证。  
- 不 commit / 不新开分支 / 不强制 reindex。
