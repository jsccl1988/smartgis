<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# ABI / include rename map (cutover)

Spec (include/ABI cutover): [`specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md)  
Spec (DLL reorg): [`specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md`](specs/2026-09-19-legacy-deep-abstraction-umbrella-design.md)  
Spec (foundation Hybrid；真源仅 `src/base/`，无仓库根物理 `base/`): [`specs/2026-09-14-base-root-hybrid-design.md`](specs/2026-09-14-base-root-hybrid-design.md)  
Plan: [`plans/2026-09-14-dll-reorganization.md`](archive/plans/2026-09-14-dll-reorganization.md)

Status: **in progress** (include/snake_case cutover still open；foundation Hybrid Phases 0–6 已收口，真源在 `src/base`)。**DLL reorg as-built 2026-09-28：** 产品 import-link stems — `base` · `gis` · `render` · `net`（从 base 抽出）· `content` · `ui_views` · `tool` · `plugin_host`；+ app-gated `ui_legacy` / optional `legacy_*`。Phase 2 域插件 stem 保持；`*.am` 仍 LoadLibrary。

## Include root

- Foundation + product: `//src` → `#include "base/..."` and `"layer/module/file.h"`.
- Repo root `//` remains for build helpers; `BUILDCONFIG` lists **`//src` before `//`**.
- No per-module `include_dirs` in `//build:legacy` beyond that.

## Debug / Release 文件名（`_d`）

`product_shared_library`（`build/smartgis.gni`）：

| 配置 | 磁盘文件 |
| --- | --- |
| Release | `{dll_stem}.dll` / `{dll_stem}.lib` |
| Debug | `{dll_stem}_d.dll` / `{dll_stem}_d.lib` |

例：`base_d.dll`；`ui_legacy_d.dll`。**不是**尾缀大写 `D`（旧形 `xxxD.dll` 已退役）。头文件 `#pragma comment(lib, …)` 与 `GetModuleHandle` 字符串跟同一规则。

## DLL reorg 终态（Phase 1 / 2）+ 平台 stem 重命名

一层一平台 DLL + 产品 import-link DLL；optional leftover 独立；域插件 / `*.am` 仍一 stem。细 `source_set` / 旧 GN 标签经 `group` 转发。核对自 2026-09-28 `BUILD.gn` `dll_stem`。

| 终态 `dll_stem` | 吸收的 cutover 短名 / 树 | 门控 / 备注 | 状态 |
| --- | --- | --- | --- |
| **`base`** | leftovers + `sys`（`legacy/xml` TinyXML 已删 → pugixml）。`archive`/`ipc` / `:foundation` **不是**本 stem；carto POD → `legacy/gis/present/carto`（`gis`）；`gis::Envelope` 在 `gis` | 默认 `src_all` | **完成** |
| **`net`** | `src/net`（原 Phase 1 曾并入 base；已抽出） | 默认 `src_all`；import-link | **完成** |
| `gis` | 原 `sdb` + 原 `algorithm`（**不含** vista） | 默认 `src_all`；已切断 → `legacy_render` | **完成** |
| `vista` | `src/vista/**` + leftover `legacy/gis/vista` | 默认 `src_all`；`VISTA_EXPORTS` / `VISTA_EXPORT` | **完成** |
| `scenic` | `src/scenic` public `Engine` | **不在** `src_all`（exploratory）；`SCENIC_EXPORTS` / `SCENIC_EXPORT`；copy internals 不进 `src_all` | **探索** |
| `render` | endgame `src/render/{rhi,scene,skia,…}` | 默认 `src_all`；**不含** leftover | **完成** |
| `content` | `src/content/**` | 默认 `src_all`；import-link | **完成** |
| `tool` | `src/tool/**`（`TOOL_EXPORT` / `TOOL_EXPORTS`；非 leftover `legacy_tool`） | 默认 `src_all`；`:dispatch`→`:tool` | **完成** |
| `ui_views` | `ui/views` + `ui/gfx` + `ui/gis`（同 PE；`UI_EXPORT`） | SmartGisViews / plugin_host；`:gfx_headers` 给 gpu | **完成** |
| `plugin_host` | `plugin/runtime/host` + widgets（`PLUGIN_HOST_*`） | Views 宿主；非 legacy `PLUGIN_EXPORT` | **完成** |
| `ui_legacy` | `gui` … `stat_chart`（+ `tool_group_sources`） | `build_app` | **完成** |
| `legacy_render` | leftover host (GDI / scene3d / bridge / `Smt3DRenderer` loader) | optional | **完成** |
| `legacy_render_gl` | leftover OpenGL `SmtGLRenderDevice` | optional; LoadLibrary from 3D factory | **完成** |
| `legacy_render_d3d` | leftover D3D11 `SmtD3DRenderDevice` | optional; LoadLibrary from 3D factory | **完成** |
| `legacy_tool` | leftover `legacy/tool/**` | optional | **完成** |
| `plugin_dem` … / `plugin` | 域插件 / AuxModule | Phase 2；`*.am` LoadLibrary | **保持** |
| `app_core` | （不变） | app-gated | **保持** |

仍为 **source_set**（有意）：产品 `*_views` / `processing_views`、`gpu`、`base/math`、legacy adapters。已移除 stem：`leftover_attr`、`sde_mem` / `sde_smf` / `sde_ws`、独立 `sdb`/`algorithm`。原 `effect/*` 已并入 **`vista.dll`**。

### Cutover 短名 → reorg 终态

| Cutover `dll_stem` | 终态 `dll_stem` | Phase |
| --- | --- | --- |
| `core` / `style` / `sys` | **`base`** | 1 + Hybrid |
| `net` | **`net`**（独立；非 base） | 1′ extract |
| `gis` / `sde_mgr` / `sde_gdal` / 原 `sdb` | **`gis`** | 1 |
| `geo` / `proj` / `tin` / `stat` / 原 `algorithm` | **`gis`** | 1 |
| endgame render SS | `render` | 1 |
| — | `content` / `tool` / `ui_views` / `plugin_host` | product import-link |
| `gui` … `stat_chart` | `ui_legacy` | 1 |
| leftover engines | `legacy_render` | 1 optional |
| leftover `tool` | `legacy_tool` | 1 optional |
| leftover `tool_group` 源 | `ui_legacy` | 1 例外 |
| `plugin_dem` … / `plugin` | *(unchanged)* | 2 |
| `app_core` | `app_core` | — |

### Export 宏（终态 DLL）

| DLL | Build define | Header macro | 迁移期旧宏 |
| --- | --- | --- | --- |
| `base` | `BASE_EXPORTS` | `BASE_EXPORT` | `CORE_*` / `STYLE_*` / `SYS_*` |
| `net` | `NET_EXPORTS` | `NET_EXPORT` | — |
| `gis` | `GIS_EXPORTS`（及子模块） | `GIS_EXPORT` / … | 原 `SDB_*` / `GEO_*` / … |
| `vista` | `VISTA_EXPORTS` | `VISTA_EXPORT` | 原编进 `gis.dll` 的 CPU vista + leftover adapters；原 `effect` source_set |
| `scenic` | `SCENIC_EXPORTS` | `SCENIC_EXPORT` | 上一代引擎产品门面；非 leftover `Smt_*` |
| `render` | `RENDER_EXPORTS` | `RENDER_EXPORT` | — |
| `content` | `CONTENT_EXPORTS` | `CONTENT_EXPORT` | — |
| `tool` | `TOOL_EXPORTS` | `TOOL_EXPORT` | 终局 `tool.dll`；勿与 leftover 混 |
| `ui_views` | `UI_EXPORTS` | `UI_EXPORT` | gfx + `ui/gis` 同 PE；头 `ui/ui_export.h` |
| `plugin_host` | `PLUGIN_HOST_EXPORTS` | `PLUGIN_HOST_EXPORT` | 勿用 legacy `PLUGIN_EXPORT` |
| `ui_legacy` | `UI_LEGACY_EXPORTS` | `UI_LEGACY_EXPORT` | `GUI_*` / … |
| `legacy_render` | `LEGACY_RENDER_EXPORTS` | `LEGACY_RENDER_EXPORT` | 各 leftover `*_EXPORT` |
| `legacy_render_gl` | `LEGACY_RENDER_GL_EXPORTS` | `LEGACY_RENDER_GL_EXPORT` | OpenGL device + `Create3D*` |
| `legacy_render_d3d` | `LEGACY_RENDER_D3D_EXPORTS` | `LEGACY_RENDER_D3D_EXPORT` | D3D11 device + `CreateD3D*` |
| `legacy_tool` | `LEGACY_TOOL_EXPORTS` | `LEGACY_TOOL_EXPORT` | 曾用 `TOOL_*`；已让给终局 |

## 历史：2010 `Smt*` → cutover 短名

下表保留 cutover 大爆炸对照（`Smt*` → 短 stem）。**磁盘上的最终文件名以「DLL reorg 终态」为准**（短名多数已并入层 stem）。

| Old dll_stem | Cutover dll_stem | Old define | Cutover define / export macro family |
| --- | --- | --- | --- |
| SmtCore | core | CORE_EXPORT | CORE_EXPORT (define `Export_core` → prefer `CORE_EXPORT` in headers) |
| SmtBaseLib | style | STYLE_EXPORT | STYLE_EXPORT |
| SmtSysCore | sys | SYS_EXPORT | SYS_EXPORT |
| SmtGeoCore | geo | GEO_EXPORT | GEO_EXPORT |
| (GEO_EXPORT) | *(absorbed into geo)* | GEO_EXPORT | GEO_EXPORT |
| SmtGisCore | gis | GIS_EXPORT | GIS_EXPORT |
| SmtGisPrj | proj | PROJ_EXPORT | PROJ_EXPORT |
| SmtTinMesh | tin | TIN_EXPORT | TIN_EXPORT |
| SmtStaCore | stat | STAT_EXPORT | STAT_EXPORT |
| SmtNetCore | net | NET_EXPORT | NET_EXPORT |
| SmtRender | render | RENDER_EXPORT | RENDER_EXPORT |
| Smt3DRenderer | render3d | RENDER3D_EXPORT | RENDER3D_EXPORT |
| SmtGdiRenderDevice | render_gdi | RENDER_GDI_EXPORT | RENDER_GDI_EXPORT |
| SmtGdiSimpleRenderDevice *(retired)* | *(was `render_gdi_simple`)* | — | Alias → `CreateRenderDevice` / `SmtGdiRenderDevice` |
| SmtGLRenderDevice | render_gl | RENDER_GL_EXPORT / RENDER3D_EXPORT (gl) | RENDER_GL_EXPORT |
| Smt3DBaseLib | scene3d | SCENE3D_EXPORT | SCENE3D_EXPORT |
| Smt3DMdLib | model3d | MODEL3D_EXPORT | MODEL3D_EXPORT |
| PointCloud3d | pointcloud | POINTCLOUD_EXPORT | POINTCLOUD_EXPORT |
| Smt3DTerrain | terrain | TERRAIN_EXPORT | TERRAIN_EXPORT |
| SmtSDEDeviceMgr | sde_mgr | SDE_MGR_EXPORT | SDE_MGR_EXPORT |
| GdalDevice | sde_gdal | SDE_GDAL_EXPORT | SDE_GDAL_EXPORT |
| SmtSDEMemDevice | sde_mem | — (removed) | — |
| SmtSDESmfDevice | sde_smf | — (removed) | — |
| SmtSDEWSDevice | sde_ws | — (removed) | — |
| SmtToolCore | tool | TOOL_EXPORT | TOOL_EXPORT |
| SmtGroupToolCore | tool_group | TOOL_GROUP_EXPORT | TOOL_GROUP_EXPORT |
| SmtGuiCore | gui | GUI_EXPORT | GUI_EXPORT |
| SmtMFCExCore | mfc_ex | MFC_EX_EXPORT | MFC_EX_EXPORT |
| SmtXViewCore | xview | XVIEW_EXPORT | XVIEW_EXPORT |
| SmtXCatalogCore | xcatalog | XCATALOG_EXPORT | XCATALOG_EXPORT |
| SmtXAMBoxCore | xambox | XAMBOX_EXPORT | XAMBOX_EXPORT |
| SmtStaDiagram | stat_chart | STAT_CHART_EXPORT | STAT_CHART_EXPORT |
| SmtAuxModule | plugin | PLUGIN_EXPORT | PLUGIN_EXPORT |
| SmtAMDemCreater | plugin_dem | — | — |
| SmtAMOrthogrid | plugin_orthogrid | — | — |
| SmtAMBAOGridCreater | plugin_orthogrid | — | — |
| SmtAM3DModelCreater | plugin_model3d | — | — |
| SmtAMMapPrint | plugin_print | — | — |
| SmtAMMapProject | plugin_proj | — | — |
| SmtAppCore | app_core | APP_CORE_EXPORT | APP_CORE_EXPORT |

GN `defines` for export: use the **export macro name** as the define that means “building this DLL”. Headers:

```cpp
#if defined(CORE_EXPORTS)
#define CORE_EXPORT __declspec(dllexport)
#else
#define CORE_EXPORT __declspec(dllimport)
#endif
```

合并后的层 DLL 在 GN 上同时定义新 `FOO_EXPORTS` 与旧子模块 `*_EXPORTS`，直到头文件统一到层宏。

## Basename collisions (must disambiguate)

| Basename | Paths | Rule |
| --- | --- | --- |
| `command.h` | `legacy/core/command/command.h`, `tool/command.h` | Prefer path sharing longest dir prefix with includer; else `tool/command.h` for `tool/**`, `legacy/core/command/command.h` for others |
| `gdi_aux_api.h` / `gdi_renderbuf.h` | `legacy/render/rhi2d/impl/gdi/…` | Prefer `rhi2d/impl/gdi/` (`gdi_simple/` removed; former top `gdi/` collapsed; `gdi_bufpool` removed → `base::tls_allocate`) |
| `scene.h` | `render/scene/scene.h`, `sdb/scene/scene.h` | Prefer same layer as includer (`render/` vs `sdb/`) |
| `resource.h` / `stdafx.h` / `targetver.h` | many modules | Prefer header under the same module directory as the includer |

## Namespace targets (public)

| Old | New |
| --- | --- |
| `SmtCore` and ad-hoc globals in core | `base` (+ `base::detail`) |
| Style / envelope (`SmtBaseLib`) | `base` |
| GIS map/feature/layer | `sdb` |
| Geo algorithms | `geo` |
| Render | `render` |
| Net | `net` |
| Tool | `tool` |
| Plugin host | `plugin` |
| Content | `content` |
| App | `app` |

## Plugin stem strings

| Old file stem | New |
| --- | --- |
| SmtAMDemCreater | plugin_dem |
| SmtAMMapProject | plugin_proj |
| SmtAMMapPrint | plugin_print |
| SmtAM3DModelCreater | plugin_model3d |
| SmtAMOrthogrid / SmtAMBAOGridCreater | plugin_orthogrid |

Stable plugin ids (`smartgis.dem`, …) stay. Phase 2：**不**把域插件并入平台 DLL。

## Tools

- `.tmp/cutover/scan_abi_residuals.py` (local, not versioned) — fail if flat includes / `Export_Smt` / `dll_stem = "Smt` remain
- `.tmp/cutover/rewrite_includes.py` — rewrite flat `#include "x.h"` using unique map + collision rules
- `.tmp/cutover/rewrite_exports.py` — rename Export_Smt* tokens and BUILD dll_stem

---

**最后更新：** 2026-09-15
