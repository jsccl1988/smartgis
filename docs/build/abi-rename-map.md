<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# ABI / include rename map (cutover)

Spec: [`../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../superpowers/specs/2026-09-13-code-style-include-abi-cutover-design.md)

Status: **in progress** (big-bang on `master`). Mechanical include/dll_stem/Export/namespace cutover landed; full `src_all` green and complete snake_case call-site sync still open — see plan Task 9.

**Next DLL merge (accepted, not yet applied):** short stems below are the cutover names. Platform reorg collapses many of them into layer stems (`base` / `sdb` / `algorithm` / `render` / `ui_legacy`, plus optional `legacy_render` / `legacy_tool`). Map: [`../superpowers/specs/2026-09-14-dll-reorganization-design.md`](../superpowers/specs/2026-09-14-dll-reorganization-design.md).

## Include root

- Product include root: `//src` only.
- Form: `#include "layer/module/file.h"`.
- No per-module `include_dirs` in `//build:legacy`.

## dll_stem + export macros

Debug on-disk names append `_d` (`base_d.dll` / `base_d.lib`), not a trailing capital `D` (legacy form was `xxxD.dll`). Release stays `xxx.dll` / `xxx.lib`. Rule lives in `smt_shared_library` (`build/smartgis.gni`).

| Old dll_stem | New dll_stem | Old define | New define / export macro family |
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

GN `defines` for export: use the **new export macro name** as the define that means “building this DLL” (same pattern as old `Export_Smt*`: defined → dllexport). Headers:

```cpp
#if defined(CORE_EXPORTS)
#define CORE_EXPORT __declspec(dllexport)
#else
#define CORE_EXPORT __declspec(dllimport)
#endif
```

Cutover may keep the old `#if !defined(Export_…)` shape temporarily by renaming the token to the new export macro identifier used as both guard and define — prefer `FOO_EXPORTS` in GN + `FOO_EXPORT` in headers when touching a file.

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

Stable plugin ids (`smartgis.dem`, …) stay.

## Tools

- `tools/cutover/scan_abi_residuals.py` — fail if flat includes / `Export_Smt` / `dll_stem = "Smt` remain
- `tools/cutover/rewrite_includes.py` — rewrite flat `#include "x.h"` using unique map + collision rules
- `tools/cutover/rewrite_exports.py` — rename Export_Smt* tokens and BUILD dll_stem

---

**最后更新：** 2026-09-13
