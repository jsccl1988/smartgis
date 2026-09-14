<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# DLL reorganization (platform layer merge + plugin hot-load)

**Date:** 2026-09-14  
**Status:** accepted（Phase 1 平台 DLL + optional leftover + 文档回写已落地；归档待 Phase 2 核对后另变更集）  
**Goal:** D — 部署精简 + 架构分层对齐 + 插件热加载模型；允许分阶段。  
**Granularity:** C — 终态拓扑同「一层一 DLL」；落地只改 `shared_library` / `dll_stem` / export，GN 内保留细 `source_set`。  
**Related:** [`../../build/src-layout.md`](../../build/src-layout.md)、[`../../build/abi-rename-map.md`](../../build/abi-rename-map.md)、[`2026-09-13-code-style-include-abi-cutover-design.md`](2026-09-13-code-style-include-abi-cutover-design.md)、[`2026-09-14-plugin-subdir-layout-design.md`](2026-09-14-plugin-subdir-layout-design.md)、[`2026-09-13-plugin-host-design.md`](2026-09-13-plugin-host-design.md)  
**Plan:** [`../plans/2026-09-14-dll-reorganization.md`](../plans/2026-09-14-dll-reorganization.md)

## Motivation

目录已按 `base` / `sdb` / `algorithm` / `render` / `plugin` / `ui` / `app` 分层，但磁盘上仍约 40 个 `dll_stem`（见 abi-rename-map）。过细 DLL 导致：

- 部署与启动加载面过大；
- 链接边与导出宏爆炸，和分层文档不一致；
- 插件热加载边界被平台碎 DLL 淹没。

本 spec 只收敛 **shared_library 边界**，不重排源码树、不重开 include cutover。

## Locked decisions

| Topic | Choice |
| --- | --- |
| Goal | D（精简 + 分层 + 热加载），分 Phase 1 / 2 |
| Topology | 一层一 DLL（平台） |
| Method | C：细 `source_set` 保留；改 `smt_shared_library` / `dll_stem` / export |
| `sys` / `net` | **并入 `base` DLL**（见下方依赖说明） |
| Endgame `render` | 仅 `src/render/**` → `render` |
| Leftover engines | 独立 optional `legacy_render` DLL；默认不进 `src_all` |
| Leftover tools | 独立 optional `legacy_tool` DLL；不进平台主 DLL |
| UI MFC chrome | Phase 1 合并为 `ui_legacy`（`smt_build_app` 门控） |
| Plugins | Phase 2：每插件一 DLL 保留 |
| Plugin host | 保持 **source_set**（见 Phase 2） |
| `content` / `tool/dispatch` | 继续 source_set，不单独 DLL |

## Dependency direction (locked)

```
app.exe / app_core (app-gated)
  └─ content (source_set)
       ├─ ui_legacy          [smt_build_app]
       ├─ ui/views (source_set stub)
       ├─ render             [endgame only]
       ├─ sdb  ──► algorithm
       │    └─(tile)──► base   # former net lives in base
       ├─ plugin/host (source_set)
       └─ base               # core+style+sys+net+archive/ipc SS
plugins/*.dll ──LoadLibrary──► platform DLLs only (no reverse deps)
legacy_render.dll  [optional] ── may be linked by leftover paths; not by endgame render
legacy_tool.dll    [optional]
```

Allowed edges: `app/content` → `ui_*` / `render` / `sdb` / `algorithm` / `base`；`sdb` → `algorithm` → `base`；`render` → `base`（及必要的 `sdb` 只读类型，若已有则保持，禁止新环）；plugins → platform。  
Forbidden: platform → plugin；`render` → `legacy_render`；`base` → `sdb`/`algorithm`/`render`/`ui`.

### `sys` / `net` → `base`（无环）

CBM / BUILD 核对：`sys` 仅 deps `//src/base:core`；`net` 仅 deps `core` + `archive`。并入 `base` 不引入循环。  
若日后实现引入 `base`→`net` 反向需求，再把 `net` 拆回独立 DLL，并在本 spec 记例外——**当前无此必要**。

### Known migration snag（例外，实现时必须处理）

今日 `//src/sdb/map:gis` 仍 deps `//src/legacy/render/render3d:render3d`。Phase 1 合并 `sdb` 前须 **切断该边**（下沉适配、条件依赖、或迁出 leftover 调用），否则 `sdb.dll` 会被迫拉 optional leftover，违反「legacy_render 默认不进 src_all」。

## Phase 1 — platform DLLs

只改链接/导出边界；各层内现有 `source_set` / 子 target 标签尽量保留（例如 `//src/sdb/datasource/gdal:sde_gdal` 可变为 source_set，由 `//src/sdb:sdb` shared_library 聚合）。

| New `dll_stem` | Absorbs (current stems / trees) | Notes |
| --- | --- | --- |
| `base` | `core`, `style`, `sys`, `net`；`ipc`/`archive` 仍为 source_set 链进本 DLL | 一层 foundation |
| `sdb` | `gis`, `sde_mgr`, `sde_gdal`；`tile` / `model` / `scene` / `edit` source_sets 链进本 DLL | 切断对 `legacy_render` 的硬依赖 |
| `algorithm` | `geo`, `proj`, `tin`, `stat` | chart 仍在 `ui_legacy` |
| `render` | endgame `src/render/{rhi,scene,skia,math,…}`（今日多为 source_set / 过渡 target） | **不含** `legacy/render/**` |
| `ui_legacy` | `gui`, `mfc_ex`, `xview`, `xcatalog`, `xambox`, `stat_chart` | 仅 `smt_build_app`；不进默认 `src_all` |

### Optional（不进默认 `src_all`）

| New `dll_stem` | Absorbs | Notes |
| --- | --- | --- |
| `legacy_render` | `render`（bridge leftover）、`render3d`, `render_gdi`, `render_gdi_simple`, `render_gl`, `scene3d`, `model3d`, `pointcloud`, `terrain` | 单一 optional DLL；也可分步先 group 后单 DLL，终态一 stem |
| `legacy_tool` | `tool`, `tool_group` | 对应 `src/legacy/tool/**` |

### Not Phase 1 platform merges

| Target | Treatment |
| --- | --- |
| `content` | source_set |
| `tool/dispatch` | source_set |
| `plugin/host` | source_set（见 Phase 2） |
| `app_core` | 保持 app-gated DLL（或日后链进 exe）；本轮不强制并入平台五 DLL |
| `ui/views` | source_set stub（`//:ui_views`） |

## Phase 2 — plugins

| Stem | Role |
| --- | --- |
| `plugin_dem` / `plugin_proj` / `plugin_print` / `plugin_model3d` / `plugin_orthogrid` | **每插件一 DLL**（热加载 / 独立分发） |
| `plugin` | leftover AuxModule 运行时（`plugin/legacy`）；适配器继续 `LoadLibrary` `*.am` |
| `plugin/host` | **保持 source_set**，链进 chrome / `content` 宿主；**不**并入 `base`（会拖 UI/Views），**不**并入各域插件 DLL |

Stable plugin ids（`smartgis.dem` 等）与 L1 目录布局不变。

## Target `dll_stem` map (old → new)

Debug 产出文件名在 stem 后追加 `_d`（如 `base_d.dll`），不是尾缀 `D`；Release 仍为 `xxx.dll`。见 `build/smartgis.gni` 的 `smt_shared_library`。

Cutover 后的短名 stem → Phase 1/2 终态：

| Current `dll_stem` | Final `dll_stem` | Phase |
| --- | --- | --- |
| `core` | `base` | 1 |
| `style` | `base` | 1 |
| `sys` | `base` | 1 |
| `net` | `base` | 1 |
| `gis` | `sdb` | 1 |
| `sde_mgr` | `sdb` | 1 |
| `sde_gdal` | `sdb` | 1 |
| `geo` | `algorithm` | 1 |
| `proj` | `algorithm` | 1 |
| `tin` | `algorithm` | 1 |
| `stat` | `algorithm` | 1 |
| *(endgame render / rhi / skia / math SS)* | `render` | 1 |
| `gui` | `ui_legacy` | 1 |
| `mfc_ex` | `ui_legacy` | 1 |
| `xview` | `ui_legacy` | 1 |
| `xcatalog` | `ui_legacy` | 1 |
| `xambox` | `ui_legacy` | 1 |
| `stat_chart` | `ui_legacy` | 1 |
| `render` (bridge leftover), `render3d`, `render_gdi`, `render_gdi_simple`, `render_gl`, `scene3d`, `model3d`, `pointcloud`, `terrain` | `legacy_render` | 1 optional |
| `tool`, `tool_group` | `legacy_tool` | 1 optional |
| `plugin_dem` … `plugin_orthogrid` | *(unchanged)* | 2 |
| `plugin` | *(unchanged)* | 2 |
| `app_core` | `app_core`（或并入 exe，非本 spec 强制） | — |

Removed stems stay removed (`sde_mem` / `sde_smf` / `sde_ws` 等)。

## Export macro strategy

手法 C：每个 **最终** shared_library 一套 `FOO_EXPORTS` / `FOO_EXPORT`：

| DLL | Build define | Header macro |
| --- | --- | --- |
| `base` | `BASE_EXPORTS` | `BASE_EXPORT` |
| `sdb` | `SDB_EXPORTS` | `SDB_EXPORT` |
| `algorithm` | `ALGORITHM_EXPORTS` | `ALGORITHM_EXPORT` |
| `render` | `RENDER_EXPORTS` | `RENDER_EXPORT` |
| `ui_legacy` | `UI_LEGACY_EXPORTS` | `UI_LEGACY_EXPORT` |
| `legacy_render` | `LEGACY_RENDER_EXPORTS` | `LEGACY_RENDER_EXPORT` |
| `legacy_tool` | `LEGACY_TOOL_EXPORTS` | `LEGACY_TOOL_EXPORT` |

迁移期允许：

1. 子模块旧宏（`CORE_EXPORT` / `GIS_EXPORT` / …）在合并后的 DLL 内 **别名到** 新宏（`#define CORE_EXPORT BASE_EXPORT`），避免一次改完所有头文件；
2. 或 GN 同时定义旧 `*_EXPORTS` 与新 `BASE_EXPORTS`，直到调用方清扫完毕。

插件域宏（`PLUGIN_*`）Phase 2 不动原则。与 [`abi-rename-map.md`](../../build/abi-rename-map.md) 的关系：该表保留 **2010→cutover 短名**，并含 **reorg 终态** 附录（已与 Phase 1 落地对齐）。
## Relation to `src-layout`

[`src-layout.md`](../../build/src-layout.md) 描述目录与 GN 标签分层。本 spec **覆盖**其中旧「core + style 两 DLL / proj·tin 各自 DLL」等 **DLL 粒度**陈述：目录布局不变，**磁盘 DLL 为一层一 DLL + optional leftover**。Phase 1 落地后 src-layout / abi-rename-map 已回写终态表。
## Non-goals

- 不改目录树 / nesting cap；不重开全仓 include 路径大爆炸（cutover 另案）。
- 不把 `legacy_render` 并进 `render`；不把插件并进平台 DLL。
- 不引入 Qt；不改 Views + Skia 终局。
- 不把 `content` / `dispatch` / `plugin/host` 做成独立产品 DLL。
- 不做实现计划正文（另开 `docs/superpowers/plans/`）；本文件只定边界。
- 不在本轮改 `BUILD.gn`（由实现 agent 执行）。

## Migration outline (not a plan)

1. **Prep:** 切断 `gis` → `render3d`；盘点每个 `smt_shared_library` 的 deps。
2. **base:** 将 `style`/`sys`/`net` 源与 deps 收入单一 `dll_stem = "base"`；`core` 标签可保留为 group→同一 DLL 或改为 source_set。
3. **algorithm → sdb → render:** 按依赖序合并；更新 export 别名。
4. **ui_legacy:** 仅在 `smt_build_app` 图合并。
5. **optional:** `legacy_render` / `legacy_tool` 各收成一 DLL + `*_all` group。
6. **Verify:** `src_all` 不链 leftover；`build.bat app` 仍可链 `ui_legacy`；插件 LoadLibrary 路径仍指向 Phase 2 stems。
7. **Docs:** 回写 abi-rename-map + src-layout DLL 段（**已完成** Task 9）；归档本 spec 当 Phase 2 核对完成后。
## Success criteria

- 默认 `src_all` 平台共享库约 **`base` + `sdb` + `algorithm` + `render`**（+ 既有 source_set）；不再为 core/style/sys/net/gis/sde_*/geo/proj/tin/stat 各产一 DLL。
- `legacy_render` / `legacy_tool` / `ui_legacy` 门控清晰。
- Phase 2 域插件仍一插件一 DLL；host 为 source_set。
- 依赖方向无平台↔插件环；无 `render`→`legacy_render`。
