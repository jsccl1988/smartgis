<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: accepted

# 代码规范 + mogu 式 include + 破 ABI 全仓切断

## 决策摘要（已锁定）

| 项 | 选择 |
| --- | --- |
| Include 范围 | **全仓一次切断**：扁平 `"header.h"` → `"layer/module/header.h"`；去掉按模块铺的 `include_dirs` |
| 风格深度 | **全面 mogu 化**：snake_case 函数、两层公共命名空间、`detail`、英文注释；不保留遗留 PascalCase / `Smt_*` 作为产品主 API |
| ABI | **破 ABI**：DLL stem、导出宏、`GetProcAddress` / 插件入口字符串一并改；**不留**旧名转发壳 |
| 验收 | `//src:src_all` + `build.bat app` / `views` 绿；仓内插件按新导出名可加载；相关测试绿；旧→新映射表落文档 |
| 执行 | **大爆炸**：一个逻辑变更集（可多 commit），中间不承诺可编译，直到收尾绿 |
| 分支 | 只在 **`master`** 上改 |

对照：mogu 产品头为 `#include "base/json/var.h"`（include 根 = 模块树根）。本仓产品树在 `src/`，故 include 根定为 **`//src`**，写法为 `#include "base/style/envelope.h"`（**不要**写成 `"src/base/..."`）。

## §1 Include 根

### 规则

1. 产品编译只认 **一个** include 根：`//src`（已在 `build/BUILDCONFIG.gn` 的 `include_dirs` 或等价全局 config 中保证；`config("legacy")` **不再**追加各模块目录）。
2. 所有产品 `#include "..."` 使用相对 `src/` 的完整路径。
3. 同名头（多模块 `api.h` / `core.h`）靠路径消歧；仍冲突则 **改文件名**（先例：`core_assert.h`、`net_string.h`），并记入映射表。
4. 三方头继续只经 `//third_party:<name>` 的 `public_configs`，不进入 legacy include 树。
5. **本轮不搬物理目录**（布局已是 `src/<layer>/<module>/`）；只改 include 字符串与 GN。

### GN

- 清空 `build/BUILD.gn` → `config("legacy")` 里 `$smt_src/base/core`、`$smt_src/algorithm/geo`、… 整段按模块 `include_dirs`。
- 保留与路径无关项：MBCS、`BUILD_AS_DLL`、`smt_compat.h` forced include、Eigen 等确需的例外（Eigen 仍建议走目标 `public_configs`，能挪则挪）。
- `smt_shared_library` 可继续挂 `//build:legacy`（defines / MBCS），但该 config **不再**充当 include 树。

### 示例

```cpp
// BAD
#include "envelope.h"
#include "geometry.h"
#include "map.h"

// GOOD
#include "base/style/envelope.h"
#include "algorithm/geo/geometry.h"
#include "sdb/map/map.h"
#include "content/public/view_host.h"
```

## §2 命名、命名空间、导出与 DLL

### 函数与类型（产品树）

| 类别 | 规则 |
| --- | --- |
| 函数 / 成员函数 | `snake_case`（含 `is_` / `has_` / `can_`） |
| 类型 | `PascalCase`（与 mogu / 现有新树一致） |
| 公共命名空间 | 最多两层：`base`、`geo`、`sdb`、`render`、`content`、`net`、`plugin`、`tool`、`app`、`ui::views`、`ui::gui`（legacy UI 进 `ui` 子模块名） |
| 内部 | `area::module::detail` 或匿名命名空间 |
| 禁止 | 第三语义层公共命名空间；新树再引入 `Smt_*` 命名空间 |

遗留 `Smt_*` 命名空间、`CSmt*` MFC 应用类、`Export_Smt*` 在本变更集内 **全部替换**；MFC 消息宏所需的类名可改为无 `Smt` 前缀的 `PascalCase`（如 `CDemCreaterApp`）。

### DLL stem（`output_name`）

对齐 GN 短名 / 层名，去掉 `Smt` 前缀。Debug 仍可加 `D` 后缀（现有 `smt_shared_library` 逻辑）。

| 旧 `dll_stem` | 新 `dll_stem` |
| --- | --- |
| `SmtCore` | `core` |
| `SmtBaseLib` | `style` |
| `SmtSysCore` | `sys` |
| `SmtGeoCore` | `geo` |
| `SmtGisCore` | `gis` |
| `SmtGisPrj` | `proj` |
| `SmtTinMesh` | `tin` |
| `SmtStaCore` | `stat` |
| `SmtNetCore` | `net` |
| `SmtRender` | `render` |
| `Smt3DRenderer` | `render3d` |
| `SmtGdiRenderDevice` | `render_gdi` |
| `SmtGdiSimpleRenderDevice` | `render_gdi_simple` |
| `SmtGLRenderDevice` | `render_gl` |
| `Smt3DBaseLib` | `scene3d` |
| `Smt3DMdLib` | `model3d` |
| `Smt3DPointCloud` | `pointcloud` |
| `Smt3DTerrain` | `terrain` |
| `SmtSDEDeviceMgr` | `sde_mgr` |
| `SmtSDEGdalDevice` | `sde_gdal` |
| `SmtSDEMemDevice` | `sde_mem` |
| `SmtSDESmfDevice` | `sde_smf` |
| `SmtSDEWSDevice` | `sde_ws` |
| `SmtToolCore` | `tool` |
| `SmtGroupToolCore` | `tool_group` |
| `SmtGuiCore` | `gui` |
| `SmtMFCExCore` | `mfc_ex` |
| `SmtXViewCore` | `xview` |
| `SmtXCatalogCore` | `xcatalog` |
| `SmtXAMBoxCore` | `xambox` |
| `SmtStaDiagram` | `stat_chart` |
| `SmtAuxModule` | `plugin` |
| `SmtAMDemCreater` | `plugin_dem` |
| `SmtAMOrthogrid` / `SmtAMBAOGridCreater` | `plugin_orthogrid` |
| `SmtAM3DModelCreater` | `plugin_model3d` |
| `SmtAMMapPrint` | `plugin_print` |
| `SmtAMMapProject` | `plugin_proj` |
| `SmtAppCore` | `app_core` |

可执行文件名保持产品品牌：`SmartGis.exe` / `SmartGisViews.exe` 等（非 DLL stem）。

### 导出宏

`Export_SmtFoo` → `<MODULE>_EXPORT`（全大写短名，与新 stem 对应），例如：

- `CORE_EXPORT` → `CORE_EXPORT`
- `STYLE_EXPORT` → `STYLE_EXPORT`
- `GIS_EXPORT` → `GIS_EXPORT`
- `SDE_GDAL_EXPORT` → `SDE_GDAL_EXPORT`

实现仍用 `__declspec(dllexport/dllimport)`；定义宏名与 GN `defines` 同步改掉旧 `Export_Smt*`。

### 插件 / LoadLibrary

- `plugin/legacy_am.cc`（及同类）中旧 DLL 文件名 → 新 stem（含 Debug `D` 规则若运行时拼接）。
- 稳定插件 id（如 `smartgis.dem`）**可保留**（已是 mogu 风格 id）；仅去掉对 `SmtAM*` 文件名的依赖。
- 若存在按导出符号名的 `GetProcAddress`，符号改为新 ABI；本仓调用点同一变更集改完。
- 仓外未跟踪的二进制插件不在验收内；文档注明 **不兼容旧 DLL**。

## §3 大爆炸执行模型

1. **先写死映射表**（本 spec 附录 + `docs/build/abi-rename-map.md`）：include 路径、dll_stem、导出宏、主要命名空间、已知插件 stem。
2. **并行机械改写**（多 agent 按不相交树）：`base`/`sys`、`algorithm`、`sdb`、`render`、`net`/`tool`、`ui`/`plugin`/`app`/`content`/`gpu`、`testing`、GN/`build`。
3. **禁止** 中途恢复模块级 `include_dirs`「先编过」；缺口只能靠补全路径 include 或改名消歧。
4. **收尾一次**：`gn gen` + `build.bat`（`src_all`）→ `build.bat app` / `views` → 相关测试与插件冒烟 → 更新 `docs/build/src-layout.md`、`src/README.md`、根 `README.md`（若模块表/DLL 名变化）、`mogu-mapping.md` 一句对照。
5. 中间 commit 允许红；**合并完成的定义** = 验收 C 全绿 + 映射文档已提交。

不使用 git worktree / 功能分支（仓库 agent 规则）。并行靠 **路径分区**，不是靠分支。

## §4 验收

- [ ] `build.bat` → `//src:src_all` 成功
- [ ] `build.bat app`、`build.bat views` 成功（在本机 MFC / 依赖可用前提下）
- [ ] 仓内插件按新 `dll_stem` 可被 host 解析/加载（至少 `plugin` host 测试 + 文档中的冒烟步骤）
- [ ] `build.bat te` 或计划列出的 `*_test` 目标绿
- [ ] 仓库内无产品代码再 `#include` 扁平遗留头（允许脚本/扫描证明）；无 `Export_Smt*`、无 `dll_stem = "Smt…"`
- [ ] `docs/build/abi-rename-map.md` 完整；`src-layout.md` / README 已改 DLL 名叙述

## §5 明确不做

- 不引入 Qt；不把 UI 终局改成非 Views+Skia。
- 不搬 `third_party/.src`；不改 GDAL/OGR 上游符号。
- 不把产品树抬到仓库根（保持 `src/`；仅 include 语义对齐 mogu）。
- 不保留双 ABI / 转发头「兼容旧插件」。
- 不自动 CBM 全量重建索引（用户未要求不跑 `index_repository`）。

## 风险

- 大爆炸期间 `master` 长期不可编；需约定并行 agent **只改分配路径**，收尾一人集成。
- 同名头与宏会漏改；应用扫描（rg / 脚本）作门禁，而非靠记忆。
- MFC `IMPLEMENT_*` / 资源与类名绑定处需手工核。
- 外部文档或用户机器上的旧 `Smt*D.dll` 全部失效——预期行为，须在根 README 或 release note 写明。

## 附录 A — Include 路径模式

| 物理路径 | Include |
| --- | --- |
| `src/base/core/log.h` | `"base/core/log.h"` |
| `src/base/style/envelope.h` | `"base/style/envelope.h"` |
| `base/ipc/channel.h` | `"base/ipc/channel.h"` |
| `src/algorithm/geo/geometry.h` | `"algorithm/geo/geometry.h"` |
| `src/sdb/map/map.h` | `"sdb/map/map.h"` |
| `src/sdb/datasource/gdal/…` | `"sdb/datasource/gdal/….h"` |
| `src/render/rhi/rhi.h` | `"render/rhi/rhi.h"` |
| `src/content/public/view_host.h` | `"content/public/view_host.h"` |
| `src/net/http/http.h` | `"net/http/http.h"` |
| `src/ui/views/view.h` | `"ui/views/view.h"` |
| `src/ui/gui/….h` | `"ui/gui/….h"` |
| `src/plugin/host.h` 等 | `"plugin/….h"` |

子目录内相对 include（同目录 `"foo.h"`）在切断后 **一律改为** 从 `src/` 起的完整路径，避免再依赖「当前模块在 include 路径上」。

## 附录 B — 命名空间目标（示意）

| 旧 | 新（公共） |
| --- | --- |
| `SmtCore` / 杂散全局 | `base`（core 实现细节进 `base::detail` 或文件级） |
| `SmtBaseLib` 风格类型 | `base` 或保持类型在 `base` + `style` 头路径 |
| GIS 要素/图层/地图 | `sdb` |
| 几何算法 | `geo` |
| 渲染 | `render` |
| 网络 | `net` |
| 工具分发 | `tool` |
| 插件宿主 | `plugin` |
| content API | `content` |
| 产品壳 | `app` |

具体类型改名表在实现时写入 `abi-rename-map.md`，本 spec 锁规则不锁每一个类名。

---

**最后更新：** 2026-09-13
