<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

> **Status: superseded** (2026-09-28 merge B). Merged into [`../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](../../specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md) — §DLL / package stems (folded). Do not revise here except mechanical link fixes; revise the living umbrella in place.


# DLL reorganization (platform layer merge + plugin hot-load)

**Date:** 2026-09-14  
**Updated:** 2026-09-28  
**Status:** superseded (2026-09-28 merge B)
**Goal:** D — 部署精简 + 架构分层对齐 + 插件热加载模型；允许分阶段。  
**Granularity:** C — 终态拓扑同「一层一 DLL」；落地只改 `shared_library` / `dll_stem` / export，GN 内保留细 `source_set`。  
**Related:** [`../../build/src-layout.md`](../../build/src-layout.md)、[`../../build/abi-rename-map.md`](../../build/abi-rename-map.md)、[`2026-09-13-code-style-include-abi-cutover-design.md`](2026-09-13-code-style-include-abi-cutover-design.md)、[`2026-09-14-plugin-subdir-layout-design.md`](2026-09-14-plugin-subdir-layout-design.md)、[`2026-09-13-plugin-host-design.md`](2026-09-13-plugin-host-design.md)  
**Plan:** [`../plans/2026-09-14-dll-reorganization.md`](../plans/2026-09-14-dll-reorganization.md)

## Motivation

目录已按 `base` / `gis` / `render` / `plugin` / `ui` / `app` 分层，但磁盘上曾约 40 个 `dll_stem`（见 abi-rename-map）。过细 DLL 导致：

- 部署与启动加载面过大；
- 链接边与导出宏爆炸，和分层文档不一致；
- 插件热加载边界被平台碎 DLL 淹没。

本 spec 只收敛 **shared_library 边界**，不重排源码树、不重开 include cutover。

## Locked decisions (as-built 2026-09-28)

| Topic | Choice |
| --- | --- |
| Goal | D（精简 + 分层 + 热加载），分 Phase 1 / 2 |
| Topology | 一层一 DLL（平台）+ 后续产品 DLL（下表） |
| Method | C：细 `source_set` 保留；改 `smt_shared_library` / `dll_stem` / export |
| `sys` | **并入 `base` DLL** |
| `net` | **独立 `net` DLL**（从 `base` 抽出；`//src/net:net`，import-link） |
| GIS | **`gis` DLL**（原 `sdb` + `algorithm` 内核并入；无独立 `sdb`/`algorithm` stem） |
| Endgame `render` | 仅 `src/render/**` → `render` |
| Leftover engines | 独立 optional `legacy_render` DLL；默认不进 `src_all` |
| Leftover tools | 独立 optional `legacy_tool` DLL；不进平台主 DLL |
| UI MFC chrome | `ui_legacy`（`smt_build_app` 门控） |
| Plugins | Phase 2：每插件一 DLL 保留；legacy `*.am` 仍 LoadLibrary |
| Plugin host | **`plugin_host` DLL**（`PLUGIN_HOST_*`；`//src/plugin:host`） |
| `content` | **`content` DLL**（import-link） |
| `tool` | **`tool` DLL**（`SMT_TOOL_*`；`:dispatch` 等 group 转发） |
| `ui_views` | **`ui_views` DLL**（views + gfx 同 PE；`:views` / `:gfx` 转发） |

## Product DLL set (import-link)

| `dll_stem` | GN | Notes |
| --- | --- | --- |
| `base` | `//src/base:base` | leftovers + carto + xml + sys；**不含** net |
| `gis` | `//src/gis:gis` | model + geo/proj/tin/stat；`public_deps` → `net` |
| `render` | `//src/render:render` | endgame RHI/scene/skia；`:rhi` group 转发 |
| `net` | `//src/net:net` | HTTP / RPC（从 base 抽出） |
| `content` | `//src/content:content` | embedder / MapContents；`:view_host` → 同 PE |
| `ui_views` | `//src/ui/views:ui_views` | Views + gfx；**单 PE** |
| `tool` | `//src/tool:tool` | `SMT_TOOL_*`；`:dispatch` 转发 |
| `plugin_host` | `//src/plugin:host` → `plugin_host.dll` | `PLUGIN_HOST_*`；非 legacy `PLUGIN_EXPORT` |

**SmartGisViews**（`//src/app/views:views`）import-link 上表全部 stem；**不要**再链 `*_sources` 以免把对象编进 exe。`smt_shared_library` 产物落在仓库根 **`out/`**（与 exe 同目录），无需额外 copy。

**gpu ↔ gfx：** `//src/gpu` 与 present 只 deps `//src/ui/gfx:gfx_headers`（include path）。`:gfx` 仍转发完整 `ui_views` DLL，供需要 gfx 符号的 Views 路径。勿让 gpu PE 拉 content/tool。

### Intentionally still `source_set`（不成产品 DLL）

| Area | Notes |
| --- | --- |
| `plugin/product/*_views`、`processing_views` | Phase 2 可再 stem；今日链进宿主 |
| `effect/{map,atmosphere,scene}` | pass 对象，链进调用方 |
| `gpu`（`gpu_backend` / `gpu_lib`） | 同 PE `GpuMain`；非独立 DLL |
| `base/math`、`base/archive`、`base/ipc`、`:foundation` | static / 链入消费者或 base |
| leftover `*.am` | **LoadLibrary**（`out/plugin/`） |

## Dependency direction (locked)

```
SmartGisViews.exe  (import-link)
  ├─ ui_views.dll  (gfx objects in same PE)
  ├─ content.dll ──► tool.dll
  ├─ plugin_host.dll ──► content / tool / ui_views
  ├─ tool.dll ──► gis (edit)
  ├─ render.dll
  ├─ gis.dll ──► net.dll
  ├─ net.dll
  └─ base.dll
gpu (source_set in shell PE)
  └─ gfx_headers only (no ui_views.dll)
plugins/*.am ──LoadLibrary──► platform DLLs only (no reverse deps)
legacy_render.dll  [optional]
legacy_tool.dll    [optional]
ui_legacy.dll      [smt_build_app]
```

Allowed edges: app/content → ui_* / render / gis / base / net；`gis` → `net` / `base`；`render` → `base`（及必要的 gis 只读类型，禁止新环）；plugins → platform。  
Forbidden: platform → plugin；`render` → `legacy_render`；`base` → `gis`/`render`/`ui`.

### `net` 从 `base` 抽出（已落地）

原 Phase 1 将 `net` 并入 `base`。落地后改为独立 `net.dll`（OpenSSL / httplib 边界清晰；`gis`/`gpu`/`plugin_host`/Views 直接 `//src/net:net`）。`sys` 仍在 `base`。

### Known migration snag（已处理）

`gis` → `legacy_render` 硬边已切断；`gis.dll` 不进 optional leftover。

## Phase 1 — platform DLLs（历史；已落地后演进）

只改链接/导出边界；各层内现有 `source_set` / 子 target 标签尽量保留。

| New `dll_stem` | Absorbs | Notes |
| --- | --- | --- |
| `base` | `core`, `style`/`carto`, `sys`；`ipc`/`archive` 仍为 source_set | **不含** net（见上） |
| `gis` | 原 `sdb` + `algorithm`（geo/proj/tin/stat） | 切断对 `legacy_render` 的硬依赖 |
| `render` | endgame `src/render/**` | **不含** `legacy/render/**` |
| `ui_legacy` | `gui`, `mfc_ex`, `xview`, `xcatalog`, `xambox`, `stat_chart` | 仅 `smt_build_app` |

### Optional（不进默认 `src_all`）

| New `dll_stem` | Absorbs | Notes |
| --- | --- | --- |
| `legacy_render` | leftover engines under `legacy/render/**` | optional |
| `legacy_tool` | `legacy/tool/**`（非产品 `tool.dll`） | optional |

## Phase 2 — plugins

| Stem | Role |
| --- | --- |
| `plugin_dem` / `plugin_proj` / `plugin_print` / `plugin_model3d` / `plugin_orthogrid` | **每插件一 DLL**（热加载 / 独立分发）；Views 路径今日多为 `*_views` source_set |
| `plugin` | leftover AuxModule 运行时（`legacy/plugin`）；适配器继续 `LoadLibrary` `*.am` |
| `plugin_host` | **产品 DLL**（已落地）；**不**并入 `base`，**不**并入各域插件 DLL |

Stable plugin ids（`smartgis.dem` 等）与 L1 目录布局不变。

## Target `dll_stem` map (old → as-built)

Debug 产出文件名在 stem 后追加 `_d`（如 `base_d.dll`），不是尾缀 `D`；Release 仍为 `xxx.dll`。见 `build/smartgis.gni` 的 `smt_shared_library`。

| Current / cutover `dll_stem` | Final `dll_stem` | Notes |
| --- | --- | --- |
| `core` / `style` / `sys` | `base` | |
| `net` | **`net`** | 独立 DLL（非 base） |
| `gis` / `sde_*` / 原 `sdb` | `gis` | |
| `geo` / `proj` / `tin` / `stat` / 原 `algorithm` | `gis` | |
| endgame render SS | `render` | |
| `gui` … `stat_chart` | `ui_legacy` | app-gated |
| leftover engines | `legacy_render` | optional |
| leftover `tool` / `tool_group` | `legacy_tool` / `ui_legacy` 源 | optional |
| — | `content` / `tool` / `ui_views` / `plugin_host` | 后续产品 DLL |
| `plugin_dem` … | *(unchanged)* | Phase 2 |
| `app_core` | `app_core` | app-gated |

Removed stems stay removed (`sde_mem` / `sde_smf` / `sde_ws` 等)。

## Export macro strategy

手法 C：每个 **最终** shared_library 一套 `FOO_EXPORTS` / `FOO_EXPORT`：

| DLL | Build define | Header macro |
| --- | --- | --- |
| `base` | `BASE_EXPORTS` | `BASE_EXPORT` |
| `net` | `NET_EXPORTS` | `NET_EXPORT` |
| `gis` | `GIS_EXPORTS`（及子模块宏） | `GIS_EXPORT` / … |
| `render` | `RENDER_EXPORTS` | `RENDER_EXPORT` |
| `content` | `CONTENT_EXPORTS` | `CONTENT_EXPORT` |
| `tool` | `SMT_TOOL_EXPORTS` | `SMT_TOOL_EXPORT` |
| `ui_views` | `UI_EXPORTS` | `UI_EXPORT` |
| `plugin_host` | `PLUGIN_HOST_EXPORTS` | `PLUGIN_HOST_EXPORT` |
| `ui_legacy` | `UI_LEGACY_EXPORTS` | `UI_LEGACY_EXPORT` |
| `legacy_render` | `LEGACY_RENDER_EXPORTS` | `LEGACY_RENDER_EXPORT` |
| `legacy_tool` | `LEGACY_TOOL_EXPORTS` | `LEGACY_TOOL_EXPORT` |

迁移期允许子模块旧宏别名或 GN 双 define。插件域 `PLUGIN_EXPORT`（legacy AuxModule）与 `PLUGIN_HOST_*` 分离。

## Relation to `src-layout`

[`src-layout.md`](../../build/src-layout.md) 描述目录与 GN 标签分层。本 spec **覆盖**其中旧「core + style 两 DLL / proj·tin 各自 DLL / net-in-base」等 **DLL 粒度**陈述。终态 stem 以本文件「Product DLL set」与 [`abi-rename-map.md`](../../build/abi-rename-map.md) 为准。

## Non-goals

- 不改目录树 / nesting cap；不重开全仓 include 路径大爆炸（cutover 另案）。
- 不把 `legacy_render` 并进 `render`；不把域插件并进平台 DLL。
- 不引入 Qt；不改 Views + Skia 终局。
- 不把 `ui_views` 拆成独立 `gfx.dll`（gfx 对象留在 `ui_views` PE）。
- 不做实现计划正文（见 `docs/superpowers/plans/`）。

## Success criteria

- 默认 `src_all` 拉起 **`base` + `gis` + `render` + `net` + `content` + `tool`**（+ leftover `plugin` 运行时）；不再为 core/style/sys/gis/sde_*/geo/proj/tin/stat 各产一 DLL。
- SmartGisViews import-link 完整产品 DLL 集；gpu 不经 `:gfx` 拉 `ui_views`。
- `legacy_render` / `legacy_tool` / `ui_legacy` 门控清晰。
- Phase 2 域插件仍一插件一 DLL；`plugin_host` 为产品 DLL；`*.am` 仍 LoadLibrary。
- 依赖方向无平台↔插件环；无 `render`→`legacy_render`。
