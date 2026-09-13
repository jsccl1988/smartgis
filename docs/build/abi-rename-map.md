<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# ABI / include rename map (cutover)

Spec (include/ABI cutover): [`../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md)  
Spec (DLL reorg): [`../superpowers/specs/2026-09-14-dll-reorganization-design.md`](../superpowers/specs/2026-09-14-dll-reorganization-design.md)  
Plan: [`../superpowers/plans/2026-09-14-dll-reorganization.md`](../superpowers/plans/2026-09-14-dll-reorganization.md)

Status: **in progress** (include/snake_case cutover still open). **DLL reorg Phase 1 已落地**（`base` / `sdb` / `algorithm` / `render` / `ui_legacy` + optional `legacy_render` / `legacy_tool`）；Phase 2 插件 stem 保持不变。

## Include root

- Product include root: `//src` only.
- Form: `#include "layer/module/file.h"`.
- No per-module `include_dirs` in `//build:legacy`.

## Debug / Release 文件名（`_d`）

`smt_shared_library`（`build/smartgis.gni`）：

| 配置 | 磁盘文件 |
| --- | --- |
| Release | `{dll_stem}.dll` / `{dll_stem}.lib` |
| Debug | `{dll_stem}_d.dll` / `{dll_stem}_d.lib` |

例：`base_d.dll`、`ui_legacy_d.dll`。**不是**尾缀大写 `D`（旧形 `xxxD.dll` 已退役）。头文件 `#pragma comment(lib, …)` 与 `GetModuleHandle` 字符串跟同一规则。

## DLL reorg 终态（Phase 1 / 2）

一层一平台 DLL；optional leftover 独立；**每插件仍一 DLL**。细 `source_set` / 旧 GN 标签经 `group` 转发到新 DLL。核对自 2026-09-14 `BUILD.gn` `dll_stem`。

| 终态 `dll_stem` | 吸收的 cutover 短名 / 树 | 门控 / 备注 | 状态 |
| --- | --- | --- | --- |
| `base` | `core`, `style`, `sys`, `net`；`ipc` / `archive` source_set 链入 | 默认 `src_all` | **完成** |
| `algorithm` | `geo`, `proj`, `tin`, `stat` | 默认 `src_all` | **完成** |
| `sdb` | `gis`, `sde_mgr`, `sde_gdal`；`tile` / `model` / `scene` / `edit` source_set 链入 | 默认 `src_all`；已切断 → `legacy_render` | **完成** |
| `render` | endgame `src/render/{rhi,scene,skia,…}` | 默认 `src_all`；**不含** `legacy_render/**` | **完成** |
| `ui_legacy` | `gui`, `mfc_ex`, `xview`, `xcatalog`, `xambox`, `stat_chart`（另含 `tool_group_sources` 以免与 `legacy_tool` 环依赖） | `smt_build_app` / `build.bat ui_legacy`；不进默认 `src_all`；产物 `ui_legacy_d.dll` | **完成** |
| `legacy_render` | leftover `render` bridge、`render3d`、`render_gdi`、`render_gdi_simple`、`render_gl`、`scene3d`、`model3d`、`pointcloud`、`terrain` | optional；不进默认 `src_all` | **完成** |
| `legacy_tool` | `tool`（`tool_group` 源链入 `ui_legacy`，见上） | optional；不进默认 `src_all` | **完成** |
| `plugin_dem` / `plugin_proj` / `plugin_print` / `plugin_model3d` / `plugin_orthogrid` | （不变） | Phase 2：每插件一 DLL | **保持** |
| `plugin` | leftover AuxModule 运行时 | Phase 2 | **保持** |
| `app_core` | （不变） | app-gated；本轮不强制并入平台 | **保持** |

仍为 **source_set**（不成产品 DLL）：`content`、`tool/dispatch`、`plugin/host`、`ui/views`。可选 leftover：`leftover_attr`（`sdb/map`）。已移除 stem：`sde_mem` / `sde_smf` / `sde_ws` 等。

### Cutover 短名 → reorg 终态

| Cutover `dll_stem` | 终态 `dll_stem` | Phase |
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
| *(endgame render / rhi / skia / scene SS)* | `render` | 1 |
| `gui` | `ui_legacy` | 1 |
| `mfc_ex` | `ui_legacy` | 1 |
| `xview` | `ui_legacy` | 1 |
| `xcatalog` | `ui_legacy` | 1 |
| `xambox` | `ui_legacy` | 1 |
| `stat_chart` | `ui_legacy` | 1 |
| `render` (bridge leftover), `render3d`, `render_gdi`, `render_gdi_simple`, `render_gl`, `scene3d`, `model3d`, `pointcloud`, `terrain` | `legacy_render` | 1 optional |
| `tool` | `legacy_tool` | 1 optional |
| `tool_group` | `ui_legacy`（源） / 标签仍可 group 转发 | 1 实现例外 |
| `plugin_dem` … `plugin_orthogrid` | *(unchanged)* | 2 |
| `plugin` | *(unchanged)* | 2 |
| `app_core` | `app_core` | — |

### Export 宏（终态 DLL）

| DLL | Build define | Header macro | 迁移期旧宏 |
| --- | --- | --- | --- |
| `base` | `BASE_EXPORTS` | `BASE_EXPORT` | `CORE_*` / `STYLE_*` / `SYS_*` / `NET_*` 可别名或双 define |
| `sdb` | `SDB_EXPORTS` | `SDB_EXPORT` | `GIS_*` / `SDE_*` |
| `algorithm` | `ALGORITHM_EXPORTS` | `ALGORITHM_EXPORT` | `GEO_*` / `PROJ_*` / `TIN_*` / `STAT_*` |
| `render` | `RENDER_EXPORTS` | `RENDER_EXPORT` | — |
| `ui_legacy` | `UI_LEGACY_EXPORTS` | `UI_LEGACY_EXPORT` | `GUI_*` / `MFC_EX_*` / `XVIEW_*` / … |
| `legacy_render` | `LEGACY_RENDER_EXPORTS` | `LEGACY_RENDER_EXPORT` | 各 leftover `*_EXPORT` |
| `legacy_tool` | `LEGACY_TOOL_EXPORTS` | `LEGACY_TOOL_EXPORT` | `TOOL_*` |

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
| SmtGdiSimpleRenderDevice | render_gdi_simple | RENDER_GDI_SIMPLE_EXPORT | RENDER_GDI_SIMPLE_EXPORT |
| SmtGLRenderDevice | render_gl | RENDER_GL_EXPORT / RENDER3D_EXPORT (gl) | RENDER_GL_EXPORT |
| Smt3DBaseLib | scene3d | SCENE3D_EXPORT | SCENE3D_EXPORT |
| Smt3DMdLib | model3d | MODEL3D_EXPORT | MODEL3D_EXPORT |
| Smt3DPointCloud | pointcloud | POINTCLOUD_EXPORT | POINTCLOUD_EXPORT |
| Smt3DTerrain | terrain | TERRAIN_EXPORT | TERRAIN_EXPORT |
| SmtSDEDeviceMgr | sde_mgr | SDE_MGR_EXPORT | SDE_MGR_EXPORT |
| SmtSDEGdalDevice | sde_gdal | SDE_GDAL_EXPORT | SDE_GDAL_EXPORT |
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
| `command.h` | `base/core/command.h`, `tool/command.h` | Prefer path sharing longest dir prefix with includer; else `tool/command.h` for `tool/**`, `base/core/command.h` for others |
| `gdi_aux_api.h` / `gdi_bufpool.h` / `gdi_renderbuf.h` | `legacy_render/gdi/…`, `legacy_render/gdi_simple/…` | Prefer same `gdi` vs `gdi_simple` as includer |
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

- `tools/cutover/scan_abi_residuals.py` — fail if flat includes / `Export_Smt` / `dll_stem = "Smt` remain
- `tools/cutover/rewrite_includes.py` — rewrite flat `#include "x.h"` using unique map + collision rules
- `tools/cutover/rewrite_exports.py` — rename Export_Smt* tokens and BUILD dll_stem

---

**最后更新：** 2026-09-14
