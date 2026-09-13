<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

Status: active

# DLL reorganization Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
>
> **本轮用户指令：** 设计已批准，「并行执行无需再确认」；在 **master** 上改；**不要 commit**（除非用户另行要求）。

**Goal:** 按 Goal D + 粒度 C，将平台碎 DLL 收敛为「一层一 DLL」（`base` / `sdb` / `algorithm` / `render` + app-gated `ui_legacy`），optional leftover 独立；Phase 2 保持每插件一 DLL。

**Architecture:** 只改 `smt_shared_library` / `dll_stem` / export 边界；子模块保留细 `source_set` + 旧 GN 标签以 `group` 转发到新 DLL。迁移期 GN 同时定义旧 `*_EXPORTS` 与新 `FOO_EXPORTS`（见 design）。依赖序：algorithm（或 base）→ sdb（先切断 `gis`→`render3d`）→ render → ui_legacy → optional legacy_* → 文档回写。

**Tech Stack:** GN/`smt_shared_library`（`build/smartgis.gni`）、Ninja `out/`、`build.bat` 针对性目标、MSVC `__declspec(dllexport/dllimport)`。

**Spec:** [`../specs/2026-09-14-dll-reorganization-design.md`](../specs/2026-09-14-dll-reorganization-design.md)

## Global Constraints

- 工作目录 `C:/Dev/src/gis/smartgis`，只在 **`master`** 改；不要开分支、不要 push。
- **不要 commit**，除非用户明确要求。
- 查找代码先 CBM（project=`smartgis`）。
- 禁止：重排源码树、重开全仓 include cutover、把 `legacy_render` 并进 `render`、平台↔插件反向依赖、Qt。
- 插件 `LoadLibrary` 字符串 / stable ids（`smartgis.dem` 等）Phase 2 保持；**不要**把 `geo`/`proj`/`tin`/`stat` 误当成插件 stem。
- Copyright：`Copyright (c) 2026 The Mogu Authors.`；源码注释英文；文档中文。
- 验证：优先 `build.bat` / `ninja -C out <target>` 针对性目标；非必要不跑全量长时间 build。

## File map（按批）

| Batch | Paths | Responsibility |
| --- | --- | --- |
| 0 Prep | `src/sdb/map/BUILD.gn` + 调用方 | 切断 `gis` → `render3d`（sdb 合并前置） |
| 1 algorithm | `src/algorithm/**/BUILD.gn`、export/`#pragma comment(lib)` | `geo`+`proj`+`tin`+`stat` → `dll_stem=algorithm` |
| 2 base | `src/base/**`、`src/sys`、`src/net` | `core`+`style`+`sys`+`net` → `dll_stem=base` |
| 3 sdb | `src/sdb/**/BUILD.gn` | `gis`+`sde_*`+tile/model/scene/edit → `dll_stem=sdb` |
| 4 render | `src/render/**/BUILD.gn` | endgame source_sets → `dll_stem=render` |
| 5 ui_legacy | `src/ui/{gui,mfc_ex,xview,xcatalog,xambox,chart}` | → `dll_stem=ui_legacy`（`smt_build_app`） |
| 6 optional | `src/legacy_render/**`、`src/legacy_tool/**` | → `legacy_render` / `legacy_tool` |
| 7 docs | `abi-rename-map.md`、`src-layout.md`、本 plan 勾选 | 终态 stem 表 + 一层一 DLL 叙述 |

**明确不做清单：**

- 不把 `content` / `tool/dispatch` / `plugin/host` 做成产品 DLL。
- 不强制 `app_core` 并入平台五 DLL。
- 不改插件域 `dll_stem`（Phase 2 仅核对）。
- 不在本 plan 执行全量 include/ABI snake_case cutover。

## 批次依赖与验证口令

| 顺序 | 合并目标 | 为何可先做 | 建议验证 |
| --- | --- | --- | --- |
| **1** | `algorithm` | 只 deps 现有 `base:core`/`base:base`；math→geo 已有 precedent；无插件 LoadLibrary 这些 stem | `ninja -C out algorithm geo_ogr_test proj_test tin_delaunay_test stat_expr_test` |
| **2** | `base` | 栈底；`sys`/`net` 无向上环（design 已证） | `ninja -C out base` + 依赖它的小测 |
| **0/3** | 先切断 `gis`→`render3d`，再 `sdb` | 否则 sdb DLL 拉 optional leftover | `ninja -C out sdb` / `sde_gdal_test` |
| **4** | `render` | endgame 今日多为 source_set，改成一 DLL | `ninja -C out render` / `rhi_test` |
| **5** | `ui_legacy` | app-gated，不进默认 `src_all` | `build.bat app`（必要时） |
| **6** | optional leftover | 不进默认 `src_all` | `ninja -C out legacy_render_all` 等 |

---

### Task 1: algorithm — geo/proj/tin/stat → 单一 `algorithm` DLL

**Files:**
- Modify: `src/algorithm/BUILD.gn` — 新增 `smt_shared_library("algorithm")`，`dll_stem = "algorithm"`
- Modify: `src/algorithm/geo/BUILD.gn` — `smt_shared_library("geo")` → `source_set("geo_sources")`；`group("geo")` → `public_deps = [ "//src/algorithm:algorithm" ]`
- Modify: `src/algorithm/proj/BUILD.gn` — 同上（`proj_sources` + group）
- Modify: `src/algorithm/tin/BUILD.gn` — 同上（`tin_sources` + group）
- Modify: `src/algorithm/stat/BUILD.gn` — `stat` 改 `source_set("stat_sources")`；group 转发
- Modify: `src/algorithm/geo/geometry.h`、`proj/projection.h`、`tin/tin.h`、`stat/evaluate.h`（及 tin/xyz 若有）— `#pragma comment(lib, "algorithm[D].lib")`
- 不改：插件 `plugin_*` 的 `dll_stem` / LoadLibrary 字符串

**Interfaces:**
- Consumes: 现有 `GEO_EXPORTS` / `PROJ_EXPORTS` / `TIN_EXPORTS` / `STAT_EXPORTS` 头文件宏
- Produces: 磁盘 `algorithm.dll` / `algorithm_d.dll`；GN 标签 `//src/algorithm:algorithm`；旧 `//src/algorithm/{geo,proj,tin,stat}:…` 经 group 仍可用

- [x] **Step 1: 将四子模块改为 source_set，并在 `algorithm/BUILD.gn` 建唯一 shared_library**

模式（每个子目录）：

```gn
source_set("geo_sources") {
  sources = [ "grid.cpp", "tin.cpp" ]
  public_deps = [ "//third_party:gdal" ]
  deps = [
    "//src/base:base",
    "//src/base:core",
  ]
  # dllexport 必须在编译这些 TU 时可见：
  defines = [ "GEO_EXPORTS" ]
}

group("geo") {
  public_deps = [ "//src/algorithm:algorithm" ]
}
```

聚合（`src/algorithm/BUILD.gn`）：

```gn
import("//build/smartgis.gni")

smt_shared_library("algorithm") {
  dll_stem = "algorithm"
  defines = [
    "ALGORITHM_EXPORTS",
    "GEO_EXPORTS",
    "PROJ_EXPORTS",
    "TIN_EXPORTS",
    "STAT_EXPORTS",
  ]
  deps = [
    "//src/algorithm/geo:geo_sources",
    "//src/algorithm/proj:proj_sources",
    "//src/algorithm/tin:tin_sources",
    "//src/algorithm/stat:stat_sources",
  ]
}

group("geom") {
  public_deps = [ ":algorithm" ]
}
```

注意：GN 的 shared_library `defines` **不会**自动传给 dep 的 source_set，故各 `*_sources` 仍须自带对应 `*_EXPORTS`。`stat_eval` 继续作为 `stat_sources` 的 deps；第三方 `proj.lib` / `proj_d.lib` 挂在 `proj_sources` 或聚合 DLL 上（与今日一致）。

- [x] **Step 2: 更新 `#pragma comment(lib, …)` 到 `algorithm` / `algorithm_d`**

凡 `!defined(XXX_EXPORTS)` 分支，统一：

```cpp
#if defined(_DEBUG)
#pragma comment(lib, "algorithm_d.lib")
#else
#pragma comment(lib, "algorithm.lib")
#endif
```

覆盖至少：`geometry.h`、`projection.h`、`tin.h`、`evaluate.h`（扫描 `src/algorithm` 内其它 pragma）。

- [x] **Step 3: 确认调用方仍通过 group 或直接 deps `//src/algorithm:algorithm`**

今日 deps `//src/algorithm/geo:geo` 等处（plugin/dem、plugin/proj、sdb、ui、legacy_render…）在 group 转发后无需改路径。若某处直接依赖已删除的 shared_library 输出名，改为 group 或 `:algorithm`。

- [x] **Step 4: 针对性编译验证**

```bat
ninja -C out algorithm
ninja -C out geo_ogr_test proj_test tin_delaunay_test tin_xyz_test stat_expr_test
```

Expected: 产出 `out/algorithm_d.dll`（debug）；不再产出独立的 `geo_d.dll` / `proj_d.dll` / `tin_d.dll` / `stat_d.dll`。

> **2026-09-14 实测：** `build.bat algorithm` 已绿（`algorithm_d.dll`）。阻塞根因是 `render/math` 的 `include_dirs` 含目录自身，本地 `math.h` 遮蔽系统 `<math.h>`，导致 Eigen/`<cmath>` 在 MSVC 上连锁失败（表象曾为 `quat.h`/C1003）。已去掉该 include 路径后重跑通过。

- [x] **Step 5: 不要 commit**（本轮用户禁止）

---

### Task 2: base — core/style/sys/net → 单一 `base` DLL

**Files:**
- Modify: `src/base/BUILD.gn`、`src/base/core/BUILD.gn`、`src/sys/BUILD.gn`、`src/net/BUILD.gn`
- Modify: 各层 export 头中 `#pragma comment(lib, "core|style|sys|net…")` → `base` / `base_d`
- Modify: 所有 `dll_stem` 引用与 exe/plugin deps（经 group 转发可减量）

**Interfaces:**
- Produces: `dll_stem = "base"`；`BASE_EXPORTS` + 旧 `CORE_EXPORTS`/`STYLE_EXPORTS`/`SYS_EXPORTS`/`NET_EXPORTS` 并存
- `ipc` / `archive` 仍为 source_set，链进该 DLL
- 保留 `//src/base:core`、`//src/base:base`（今日 style）、`//src/sys:sys`、`//src/net:net` 为 **group → `:base` 聚合库**（或统一到 `//src/base:base` 一名，须在本任务内选定并改完所有 GN 引用）

- [x] **Step 1: 盘点四库 sources/deps，确认无 `base`→`sdb`/`algorithm`/`render` 边**

CBM / BUILD：`core_sources` 仅 CxImage；`style_sources` 无外向；`sys_sources` 无 GN deps；`net_sources` 仅 archive + asio/httplib。无 `base`→`sdb`/`algorithm`/`render`。

- [x] **Step 2: 改 source_set + 单一 `smt_shared_library("base")`（注意：今日已有 `smt_shared_library("base")` 且 `dll_stem=style` — 就地升格，把 core/sys/net 源或 source_set 链入，并把 `dll_stem` 改为 `"base"`）**

`dll_stem = "base"`；`core_sources` / `style_sources` / `sys_sources` / `net_sources` / `ipc_sources` 链入；`archive` 为 public_deps。

- [x] **Step 3: 旧标签 group 转发；更新 pragma / 测试 deps**

`//src/base:core`、`//src/base/core:core`、`//src/sys:sys`、`//src/net:net`、`//src/base/ipc:ipc` → `//src/base:base`。头文件 `#pragma comment(lib)` → `base` / `base_d`。

- [x] **Step 4: 验证**

```bat
ninja -C out base
ninja -C out net_test
```

> **2026-09-14 实测：** `build.bat base` 产出 `out/base_d.dll`（约 2.5MB）；`build.bat net_test` 链接通过，`net_test: ok`。旧 `coreD`/`styleD`/`sysD`/`netD` 不再由本图生成（磁盘残留可手清）。source_set 须自带 `//build:legacy` + `*_EXPORTS`（与 algorithm 同模式）。

- [x] **Step 5: 不要 commit**

---

### Task 3: Prep — 切断 `gis` → `render3d`

**Files:**
- Modify: `src/sdb/map/BUILD.gn`（去掉 `//src/legacy_render/render3d:render3d`）
- Modify: 实际引用 leftover 3D 的 `.cpp/.h`（CBM `trace_path` / `search_code` 定位）— 下沉适配、`#if`、或迁到 `legacy_render` / 插件侧

**Interfaces:**
- Produces: `//src/sdb/map:gis`（及随后的 sdb DLL）不再硬依赖 optional leftover
- Consumes: design「Known migration snag」

- [x] **Step 1: CBM 查 `gis` / `map_layer` 对 `render3d` / `Smt3D` 符号的引用**

CBM：`src/sdb` 内仅 `map/BUILD.gn` deps + `feature.cpp` `#include legacy_render/render3d/base.h`（为 `SmtMaterial` 完整类型 / `SMT_SAFE_DELETE`）。`feature.h` 已是前向声明。

- [x] **Step 2: 切断 GN deps；编译失败驱动迁出或抽象**

去掉 `gis`→`render3d`；`feature.cpp` 对 opaque `SmtMaterial*` 用 `::operator delete`（类型 trivially destructible），不再 include leftover 头。

- [x] **Step 3: `ninja -C out gis`（或 map 目标）确认不链 `render3d`**

> **2026-09-14 实测：** `build.bat sdb` 绿；`out/build.ninja` 无 `render3d` / `legacy_render` 边；`gis` group → `sdb_d.dll`。

- [x] **Step 4: 不要 commit**

---

### Task 4: sdb — gis/sde_*/tile/model/scene/edit → 单一 `sdb` DLL

**Files:**
- Modify: `src/sdb/BUILD.gn`、`src/sdb/map/BUILD.gn`、`src/sdb/datasource/mgr/BUILD.gn`、`src/sdb/datasource/gdal/BUILD.gn`、`src/sdb/tile/BUILD.gn`、`model`/`scene`/`edit`
- Export / pragma → `sdb` / `sdb_d`；`SDB_EXPORTS` + 旧 `GIS_EXPORTS`/`SDE_*_EXPORTS`

**前置:** Task 3 完成。

- [x] **Step 1: 各 `smt_shared_library` → `source_set(*_sources)`；`smt_shared_library("sdb")` 聚合**

`gis_sources` / `sde_gdal_sources` / `sde_mgr_sources` / `tile_sources` / `model_sources` / `scene_sources` / `edit_sources` / `map_edit_sources` → `//src/sdb:sdb`（`dll_stem=sdb`）。`ogr_codec` 仍为可复用 source_set。`leftover_attr` 保持独立 DLL。

- [x] **Step 2: group 转发旧标签（`gis` / `sde_mgr` / `sde_gdal`）**

`gis` / `sde_mgr` / `sde_gdal` / `tile` / `model` / `scene` / `edit` / `map_edit` / `datasource` → `//src/sdb:sdb`。pragma：`gis_export.h` / `sde_mgr_export.h` / `ogr_export.h` → `sdb` / `sdb_d`。

- [x] **Step 3: 验证**

```bat
ninja -C out sdb
ninja -C out sde_gdal_test
```

> **2026-09-14 实测：** `build.bat sdb` → `out/sdb_d.dll`；`build.bat sde_gdal_test` 绿，`sde_gdal_test.exe` 输出 `connect checks ok`（GPKG SKIP 因本机 GDAL 无该驱动，与合并无关）。磁盘可能仍有旧 `gisD`/`sde_*D` 残留，非本图产出。

- [x] **Step 4: 不要 commit**

---

### Task 5: render（endgame only）→ 单一 `render` DLL

**Files:**
- Modify: `src/render/BUILD.gn`、`rhi`/`scene`/`math`/`skia` 相关 BUILD
- **不**纳入 `src/legacy_render/**`

**Interfaces:**
- Produces: `dll_stem = "render"`，`RENDER_EXPORTS`
- 今日 `group("render_all")` 改为 deps 该 DLL（或 public_deps）

- [x] **Step 1: 将 endgame source_set 链入 `smt_shared_library("render")`**

`rhi_sources` / `scene_sources` / `skia_sources` + math/bounds → `dll_stem=render`；旧标签 `rhi`/`scene`/`skia`/`render_all` group 转发。math/bounds 保持可独立链接的 source_set（避免 algorithm 拉 render DLL）。`render_export.h` + `RENDER_EXPORT`；pragma `render_d.lib`。

- [x] **Step 2: 确认无 deps → `legacy_render`**

`out/obj/src/render/render.ninja` 仅链 `base_d` / `sdb_d` + GDAL/FlyCube；无 `legacy_render`。

- [x] **Step 3: 验证 `ninja -C out render` / `rhi_test`**

> **2026-09-14 实测：** `build.bat src/render:render` → `out/render_d.dll`（约 8.5MB）；`rhi_test` / `scene_gpu_test` / `math_test` 链接绿且运行 ok（FlyCube HWND 路径默认 skip）。根 `build.bat render` 仍是 GPU 进程别名，编本 DLL 用 `src/render:render`。pragma 为 `render_d.lib`（无 `*D` 回退）。

- [x] **Step 4: 不要 commit**

---

### Task 6: ui_legacy（app-gated）

**Files:**
- Modify: `src/ui/gui|mfc_ex|xview|xcatalog|xambox|chart/BUILD.gn` + 聚合 BUILD
- `smt_mfc_shared_library` 边界合并为 `dll_stem = "ui_legacy"`
- `UI_LEGACY_EXPORTS` + 旧 `GUI_EXPORTS` 等别名/双 define

**前置:** Task 2–4 平台 DLL 稳定（ui 链 sdb/algorithm/base）。

- [ ] **Step 1: 合并六库为 `ui_legacy`；不进默认 `src_all`**

- [ ] **Step 2: `build.bat app` 或等价门控目标验证（仅当需要）**

- [ ] **Step 3: 不要 commit**

---

### Task 7: optional `legacy_render` / `legacy_tool`

**Files:**
- `src/legacy_render/**/BUILD.gn`、`src/legacy_tool/**/BUILD.gn`
- 终态各一 `dll_stem`；`legacy_*_all` group；默认不进 `src_all`

- [ ] **Step 1: 先 group 聚合验证，再收成单 DLL（可两步）**

- [ ] **Step 2: 确认 `src/BUILD.gn` `src_all` 无 leftover**

- [ ] **Step 3: 不要 commit**

---

### Task 8: Phase 2 核对（无强制改 stem）

**Files:**
- `src/plugin/**/BUILD.gn`、host LoadLibrary / `*.am` 适配路径
- `docs/build/abi-rename-map.md` Plugin stem 段

- [ ] **Step 1: 确认每插件仍一 DLL；host 仍为 source_set**

- [ ] **Step 2: 确认平台合并未改 `plugin_dem` 等 LoadLibrary 字符串**

- [ ] **Step 3: 不要 commit**

---

### Task 9: 文档回写

**Files:**
- Modify: `docs/build/abi-rename-map.md` — 增加 reorg 终态列/附录
- Modify: `docs/build/src-layout.md` — 「一层一 DLL + optional leftover」；删除/覆盖「Deliberately not merged」中过时 DLL 粒度句
- Modify: 本 plan 勾选；spec Status 在全部落地后改 `landed` 并归档（另变更集）

- [ ] **Step 1: 与 design 终态表对齐回写**

- [ ] **Step 2: 不要 commit**（除非用户要求一次文档+代码提交）

---

## Self-review（对照 spec）

| Spec 要求 | Plan 任务 |
| --- | --- |
| Phase 1：`base`/`sdb`/`algorithm`/`render`/`ui_legacy` | Task 2/4/1/5/6 |
| `sys`/`net` 并入 `base` | Task 2 |
| 切断 `gis`→`render3d` | Task 3 |
| optional `legacy_render`/`legacy_tool` | Task 7 |
| Phase 2 插件一 DLL；host source_set | Task 8 |
| export 双 define / 别名 | 各 Task Step 的 defines |
| 回写 abi-rename-map + src-layout | Task 9 |
| 不改目录树 / 不重开 include cutover | Global Constraints |

## 执行说明

本轮可立即执行 **Task 1**（algorithm）。Task 2 与 Task 1 可并行于不同 agent，但勿同时改同一 `BUILD.gn`。Task 4 必须在 Task 3 之后。
