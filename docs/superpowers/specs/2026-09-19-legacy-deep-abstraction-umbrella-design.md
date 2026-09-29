<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# 遗留层深度抽象（A+B+C）— living umbrella

**Status:** active  
**Date:** 2026-09-19  
**Updated:** 2026-09-29 — §SP1 Layout: `bridge/{abi,msg}` + hoist `group/` capabilities; §13 point-cloud; §11b.
**Scope:** Living design for leftover strangler program **SP0–SP5**: order, technique, dependency direction, parallel rules, ABI, and **locked decisions per SP**. Implementation checklists stay in `docs/superpowers/plans/` (linked below). Physical package splits already done; revise **sections here** — do not open new dated SP / layout twins.  
**Related (accepted / landed — do not reopen):**

| Topic | Spec / as-built |
| --- | --- |
| Render leftover physical split | [`2026-09-13-render-legacy-split-design.md`](../archive/specs/2026-09-13-render-legacy-split-design.md) |
| App / UI leftover physical split | [`2026-09-14-app-legacy-split-design.md`](../archive/specs/2026-09-14-app-legacy-split-design.md) |
| Tool leftover physical split | [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md) |
| RHI + World / GpuScene + frame graph (+ model/compute folded) | [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md) |
| Tool session dispatch + `src/tool` layout | [`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md) |
| `app/views` shell + Views toolkit / MFC migration | [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md) |
| Product as-built | [`../../build/src-layout.md`](../../build/src-layout.md), [`../../build/ui-views-skia.md`](../../build/ui-views-skia.md) |

**Plans (checklists — not second designs):**

| SP | Plan |
| --- | --- |
| SP1 | [`../plans/2026-09-19-legacy-tool-workspace-strangler.md`](../plans/2026-09-19-legacy-tool-workspace-strangler.md) |
| SP1b | [`../plans/2026-09-19-tool-behavior-migration.md`](../plans/2026-09-19-tool-behavior-migration.md) |
| SP2 | [`../plans/2026-09-19-legacy-render-present-facade.md`](../plans/2026-09-19-legacy-render-present-facade.md) · dual-run [`../archive/plans/2026-09-27-legacy-render-subdirectory-dual-run.md`](../archive/plans/2026-09-27-legacy-render-subdirectory-dual-run.md) |
| SP3 | [`../plans/2026-09-19-legacy-host-behavior-extract.md`](../plans/2026-09-19-legacy-host-behavior-extract.md) · app layout [`../archive/plans/2026-09-27-legacy-app-subdirectory-layout.md`](../archive/plans/2026-09-27-legacy-app-subdirectory-layout.md) · UI layout [`../plans/2026-09-29-legacy-ui-subdirectory-layout.md`](../plans/2026-09-29-legacy-ui-subdirectory-layout.md) · **Feature Pack** [`../plans/2026-09-29-legacy-mfc-ex-feature-pack.md`](../plans/2026-09-29-legacy-mfc-ex-feature-pack.md) |
| SP4 | [`../plans/2026-09-19-scene3d-world-gpuscene.md`](../plans/2026-09-19-scene3d-world-gpuscene.md) · SP4b [`../plans/2026-09-28-scene3d-index-octree.md`](../plans/2026-09-28-scene3d-index-octree.md) |
| SP5 | [`../plans/2026-09-19-shell-compile-gate.md`](../plans/2026-09-19-shell-compile-gate.md) |

---

## 1. 问题与目标

物理拆分已把 2010 leftover 赶到 `src/legacy/**`，终局树边界清晰。行为仍大量穿过 leftover：`SmtIATool` / `SmtRenderDevice` / MFC `CView` 与 `scene3d` 相互牵制。

目标（A+B+C）：

1. **A — 依赖方向**：终局 **不得** 依赖 `src/legacy/**`；legacy → Facade（`tool` / `render::rhi` / `content`）单向允许。
2. **B — 热点收口**：工具、Present/Paint、宿主行为三条热路径用 Facade strangler；其余 leftover 可编译、可 opt-in。
3. **C — 场景终局对齐**：`legacy/render/scene3d` GIS/三维语义迁向 `gis::World` + `render::scene::GpuScene`；不在 leftover 长第二套引擎。

---

## 2. 程序表（SP0–SP5）

| ID | 名称 | 职责 | 前置 | 默认路径所有权 |
| --- | --- | --- | --- | --- |
| **SP0** | 伞状契约 | 本文 | — | 本文 + Active 表 |
| **SP1** | Tool → Workspace | 激活经 `tool::Workspace` | SP0 | `legacy/tool/**`、`src/tool/**` |
| **SP1b** | Tool 行为搬迁 | 指针进 Interaction；leftover 变薄 | SP1 激活 | 同上 + `content` 薄接线 |
| **SP2** | Present Facade | HWND present + Null 录制；`rhi2d`/`rhi3d` 布局 | SP0 | `legacy/render/{rhi2d,rhi3d}/**`、`render/rhi` |
| **SP3** | Host 行为 | HWND-free → `content`；`legacy/app` 薄壳 | SP0 | `legacy/app|ui`、`content`、`app/views` |
| **SP4** | Scene3D → World/GpuScene | DEM/AABB 镜像；octree 索引层 | SP2 缝可用 | `legacy/render/scene3d/**`、`gis`、`render/scene` |
| **SP4b** | scene3d `index/` + vendor octree | 去 `bl3d_`、scheme A | SP4 Success | 仅 `scene3d/**` + vendor pin |
| **SP5** | Shell 编译闸门 | 默认壳 / `src_all` 不拉 leftover 聚合 | SP1–SP3 可演示 | 根/`src` GN、文档 |

SP1/SP2/SP3 可并行但路径不重叠。SP4 等 SP2 Present 缝。SP5 不抢业务路径。

---

## 3. 推荐手法（锁定）

| 选择 | 说明 |
| --- | --- |
| 主手法 | Facade strangler；新调用走终局 API；旧导出保持 |
| 提取范围 | 仅热点纯辅助 / 小服务；禁止全局 traits 化 leftover |
| Traits | 推迟到 `algorithm` / `geo` / RHI 终局 |
| 增量 | 抽出 → 一切换点 → 测 → 再扩 |
| 拒绝 | 整仓 rewrite；终局 `#include "legacy/…"`；Qt；第二控件库 |

---

## 4. 依赖规则（锁定）

```
终局 src/{tool,render/…,content,app/views,ui/views,gis,…}
        ▲ 禁止依赖
src/legacy/**
        ▼ 允许单向 → tool::Workspace / render::rhi / content public
```

Chrome 只 include `content/public`。不新增 `render` 终局 → `legacy_render`。

---

## 5. 并行 / ABI / Non-goals

- **并行：** 路径所有权表；共享缝先写清所有权；日常在 **master**。
- **ABI：** 默认不破 leftover `dll_stem` / `Smt_*`；break 必须在对应 SP 节显式授权。
- **Non-goals：** 禁止 Qt；不 wholesale rewrite；不再开「要不要再挪目录」的平行 dated 规格；不把 WinUI/WebView2/MFC Feature Pack 升为终局壳；本伞不要求删除 leftover 源码。

---

## 6. 与既有规格边界

| 既有 | 本伞 |
| --- | --- |
| legacy-split 物理树 | 已接受；行为抽象在本文 SP 节 |
| render-rhi-scene | SP2 present；SP4 World/GpuScene；不另立第二 RHI |
| tool-event-dispatch | SP1/SP1b 终点；布局节见该 living |
| ui-views-mfc-migration / views-desktop-shell | SP3 宿主行为 + `app/views` 能力切分 |
| model-render-compute | 三层语义权威；本伞只管 strangler 入口 |

冲突时：**更窄的已接受实现规格优先**；改锁定决策先改那份 living，再改本文交叉引用。

---

## 7. SP 节成功标准（模板）

每节须可验证：Goal、Non-goals、Locked、Path ownership、Dependency、ABI、Success criteria。细节勾选在对应 **plan**。

---

## 8. SP1 — Tool → Workspace strangler

**Goal:** 映射的 `GT_MSG_*` 激活经 `tool::Workspace::execute`（与 `command_id_from_gt_msg` 同 id）；Flash / ViewCtrl / Select / Append 首波。

**Locked:**

| Topic | Choice |
| --- | --- |
| Pattern | `bind_workspace(Workspace*)` + optional `m_workspace` |
| Msg → id | `tool::command_id_from_gt_msg` only |
| Helper | `tool::try_execute_gt_msg` in endgame `legacy_msg`（无 leftover include） |
| Bound activate | `try_execute_gt_msg`；不 `SetActive` / leftover 捕获 |
| Unbound | 旧 `Notify` 不变 |
| Dependency | `legacy/tool` capability sources 可 dep `//src/tool:dispatch`；终局公开头不 dep leftover |

**Layout (folded, 2026-09-29):** leftover `legacy/tool/{abi,msg,nav,select,draft,base,factory}` — **终局浅镜像**（`nav`←view、`draft`←input；`abi`/`msg` 为 leftover 专用）；两 GN target（abi → `legacy_tool` DLL；msg = `source_set` 不进 DLL）；无 `group/` / `bridge/` / shim。Checklist：[`../plans/2026-09-29-legacy-tool-bridge-capability-layout.md`](../plans/2026-09-29-legacy-tool-bridge-capability-layout.md)。终局 `src/tool/<module>/` 见 tool-event-dispatch living。As-built：`src/legacy/tool/README.md`、`src/tool/README.md`。

**Non-goals:** 不 rewrite 全部 `SmtIATool`；不碰 render/app/ui；不破 `dll_stem`。

---

## 9. SP1b — Tool 行为搬迁

**Goal:** bound 时指针序列只由 `tool::Interaction` + `InputRouter` 消费；leftover 仅 bind / notify / `apply_draft` 文档副作用 + 可选 AuxDraw。过关门 **A = T + H + L**。

**Locked:** 目标 C 行为搬迁；导航+选择+数字化并行；手法 = 按能力竖切 + 共用测试矩阵；bound = 终局指针，unbound = 旧行为。

**Path:** 可改 `src/tool/**`、首波 group tools、content 薄接线；禁改 render present、legacy app/ui、scene3d、SP5 闸门大改。

---

## 10. SP2 — Present / Paint Facade + legacy render 布局

**Goal:** MFC HWND present 仍归 GDI/GL `Init`；像素经 `LeftoverRecorder` → Null/`GpuScene`；`bind_rhi_present` 只记 HWND + Null，**禁止**在该 HWND 上建 FlyCube swapchain。

**Locked:**

| Topic | Choice |
| --- | --- |
| Present | 双轨：MFC 独占其 HWND；Views/gpu 独立 HWND + `preferred_gpu_backend()` |
| `bind_rhi_present` | `leftover_session().bind_present_hwnd`；禁止 FlyCube |
| GDI RenderMap | 先画再 `leftover_record_map_frame` |
| ABI | 保留 `bind_rhi_present` / `smt_leftover_session` |

**Layout (folded, dual-run + rhi2d landed):**

```
src/legacy/render/
  rhi2d/public/device + detail + impl/gdi
  rhi3d/public/{…,bridge/leftover_*} + impl/{gl,d3d}
  scene3d/
```

无顶层 `bridge/` / `gdi/`；无旧路径 shim；`dll_stem=legacy_render`。Dual-run / MapLibre parity 政策细节见 archived dual-run spec；as-built：`src/legacy/render/README.md`。

**Path:** 可改 rhi2d/rhi3d present 缝；禁改 tool、app/ui、scene3d 业务核（SP4）。

---

## 11. SP3 — Host 行为 + `legacy/app` 布局

**Goal:** HWND-free 宿主单元进 `content`（Attribute / Catalog 已落地）；续作 bootstrap / draft-commit / 薄 MFC view。`legacy/app` scheme C：`core` / `shell/{frame,dock,showcase}` / `doc` / `view/{map,edit,datasource,scene3d}`；破 include；`dll_stem=app_core` / `SmartGis.exe` / opt-in `legacy_app` 冻结。（2026-09-30：`shell`+`view` 能力子目录 + 短 stem。）

**Locked:** Facade strangler；控件只传 string/token；chrome 不持 `SmtFeature*`；不另立第二套 SP3。

**Path:** `content/**`、`app/views/**`、`legacy/app|ui` 抽调用点；禁改 SP1/SP2/SP4 默认树。

**`legacy/ui` common（2026-09-29）：** `legacy/ui/widgets/` 持 Feature Pack glue / prop-list；sole AFX `DllMain` = `widgets/widgets_core.cpp`。`SmtAMBoxMgrDocBar`（`ambox/ambox_dock_bar`）基类：历史为 `StackedWndDockBar`，**§11c 改为直接 `CMFCOutlookBar`**。`ui_legacy` 链 `/FORCE:MULTIPLE`（多 PCH AFX 符号）。不迁终局 Views。

**`legacy/ui` subdirectory layout（2026-09-29，Approach C′ — capability + top-level `res/`）：**

| Lock | Choice |
| --- | --- |
| Scope | Capability dirs + **`res/<capability>/`** for all binary resources |
| Technique | Scheme C — break includes, **no** old-path shim |
| Nesting | Cap `legacy/ui/<capability>/`；resources only under `legacy/ui/res/<capability>/` |
| ABI | Freeze `dll_stem=ui_legacy`；per-capability PCH/`*_sources`；keep `*_EXPORTS` macros |
| Behavior | **Out of this wave** — no HWND-free extract / no Views migration |
| Parallel | One owner path per capability; no overlapping writes |

Target (Approach C′ historical; **superseded by §11c B1** below):

```
shell/ viewport/ panels/ ambox/ catalog/ dialogs/ dock/ grid/ widgets/ chart/
res/{shell,catalog,dialogs,ambox,widgets,chart}/
```

(`dock/` + `grid/` removed by §11c waves 1–2.) Checklist: [`../plans/2026-09-29-legacy-ui-subdirectory-layout.md`](../plans/2026-09-29-legacy-ui-subdirectory-layout.md).

### 11c. leftover `grid/` + `dock/` → MFC Feature Pack（2026-09-29）

**Goal:** Stop maintaining in-tree Chris Maunder `CGridCtrl` (`legacy/ui/grid/`) and `StackedWndDockBar` / `TabbedWndDockBar` (`legacy/ui/dock/`) on leftover `SmartGis.exe`. Use MSVC **MFC Feature Pack** only; three UX waves. **Views / `SmartGisViews` out of scope.**

**Locked:**

| Lock | Choice |
| --- | --- |
| Product path | leftover `legacy_app` / `SmartGis.exe` only |
| Toolkit | Feature Pack via `widgets/bcg_cmfc.h` — **not** BCG Pro, not Qt |
| Endgame | Views + Skia; Feature Pack remains leftover bridge |
| Depth | Delete capability dirs `grid/` and `dock/` after call sites move; flatten to Feature Pack types |
| UX tier | controls → Visual Manager / dock flatten → IA (filter / group / search) |
| Post-waves layout | Wave 4 **B1**: reshape to mirror `src/ui/gis` + `ui/views/map` vocabulary. **Must stay under `src/legacy/ui`** — never hoist into `src/ui` or `src/app/views` |
| ABI | Freeze `dll_stem=ui_legacy`; sole AFX `DllMain` = `widgets/widgets_core.cpp` |
| HWND-free | Out of this program |

**Control map:**

| Call site | From | To |
| --- | --- | --- |
| `dialogs/dlg_2d_feature_info` | `CGridCtrl` | `CMFCPropertyGridCtrl` |
| `dialogs/dlg_att_struct_set` | `CGridCtrl` | `CMFCListCtrl` (report) |
| `plugin/dem/views/dlg_tin_loader` | `CGridCtrl` | `CMFCListCtrl` (report) |
| `app/shell/frame/main` Catalog | `TabbedWndDockBar` | `CDockablePane` + `CMFCTabCtrl` (no exported shim) |
| `ambox/ambox_dock_bar` | `: StackedWndDockBar` | `: CMFCOutlookBar` / typedef |

**Waves:**

1. **Controls** — migrate three dialogs; delete `grid/` + `widgets/grid_ctrl_support.h`; strip `../grid` from `widgets_sources`.
2. **Shell look** — flatten Catalog/AMBox; delete `dock/`; tune `OnAppLook`.
3. **IA** — FeatureInfo groups+filter; Catalog search; AMBox grouped pages.
4. **Layout align (after 1–3)** — reshape remaining `legacy/ui` dirs to endgame-like roles (see target below). Scheme C break includes; **no** leave `legacy/`.

**Post-wave target tree (B1 — still `src/legacy/ui`, Feature Pack MFC):**

```
legacy/ui/
  shell/            # ≈ app/views/shell — frame / chrome glue
    ambox/          # ≈ ui/gis/shell AmboxView
    chart/          # ≈ ui/gis/shell ChartView
  map/              # ≈ ui/views/map (ex-viewport/)
  inspect/          # ≈ ui/gis/inspect (ex-panels/)
  catalog/          # ≈ ui/gis/catalog
  dialogs/          # ≈ ui/gis/dialogs + toolkit modals
  widgets/          # FP glue + sole DllMain (bcg_cmfc); leftover-only
  res/
    shell/{,ambox/,chart}/
    catalog/ dialogs/ widgets/
```

No empty `analysis/` / `style/` / `debug/`. Name mapping is **semantic**, not a copy of `ui::views` kernel. Delete empty `grid/` / `dock/` (already gone after 1–2).

**Non-goals:** Views parity; Ribbon rewrite; elevating Feature Pack to endgame; vendoring BCG; moving sources to `src/ui` / `src/app/views`; nesting under `legacy/ui/gis/`.

**Success:** No `CGridCtrl` / `StackedWndDockBar` / `TabbedWndDockBar`; no `legacy/ui/grid` or `legacy/ui/dock`; waves 1–3 UX done; wave 4 **B1** tree landed; `ui_legacy` + `legacy_app` green; as-built README.

**Path ownership:** `legacy/ui/**` + `legacy/app/shell` + `legacy/plugin/product/dem/views` as needed. Do not edit `src/ui/views` / `src/app/views`.

**Checklist:** [`../plans/2026-09-29-legacy-mfc-ex-feature-pack.md`](../plans/2026-09-29-legacy-mfc-ex-feature-pack.md).

---

## 11b. `legacy/core` subdirectory layout（2026-09-29）

**Goal:** Leftover Smt core → tight responsibility dirs; scheme C break includes; **no** migrate out of `legacy/`. Still absorbed into `base.dll` via `core_sources`.

| Lock | Choice |
| --- | --- |
| Depth | Header-only + STL internals — stay under `legacy/core/<module>/` |
| Technique | Scheme C — break includes, **no** old-path shim, **no** `api.h` / `.cpp` |
| Nesting | Cap `legacy/core/<module>/`；helpers 收紧为单一 `util/` |
| Naming | `macros`/`types`/`util`/`listener`/`command`/`msg`/`diag` |
| ABI | Keep class `BASE_EXPORT` where plugins need vtable; free helpers are `inline` |
| Behavior | Out of this wave — no HWND-free extract / no new foundation APIs |

Target:

```
legacy/core/
  README.md  BUILD.gn
  macros/macros.h          # + dEPSILON / dPI / is_equal
  types/{types,env,scalars,point,rect,variant}.h
  util/{string,path,color,image,menu}.h   # header-only; no geom/math/variant
  listener/listener_manager.h
  command/command.h
  msg/msg_def.h
  diag/{assert,exception}.h
```

Checklist: [`../plans/2026-09-29-legacy-core-subdirectory-layout.md`](../plans/2026-09-29-legacy-core-subdirectory-layout.md).

### 11b.1 `types/` — Point/Rect traits + `SmtVariant`（2026-09-29）

**Goal:** Collapse parallel `l*`/`f*`/`dbf*` Point/Rect copies via `Point2`/`Point3`/`Rect` + `point_traits`/`rect_traits`; keep legacy `using` aliases. Modernize `SmtVariant` onto `std::variant` storage (owned lists/strings) with snake_case accessors. `env.h` unchanged. Stay under `legacy/core/types/`.

**Layout:** `scalars.h` · `point.h` · `rect.h` · `variant.h` · umbrella `types.h`.

### 11b.2 util fold — types/macros own geometry + eps（2026-09-29）

**Goal:** Member-first dedupe — `Rect::{normalize,contains,cast_to}`; move `dEPSILON`/`dPI`/`is_equal` into `macros/macros.h` (`SMT_EQUAL` uses them); drop PascalCase `GetAppPath` aliases; delete unused `util/{geom,math,variant}.h` (no call sites for `var_to_*`). Keep `util/` for path/string/color/image/menu only. Scheme C — update call sites in the same change; **no** shim.

---

## 12. SP4 — Scene3D → World / GpuScene

**Goal:** DEM/地图种子 envelope → `gis::World`；掩膜权威 `gis::land_mask`；AABB 镜像；Views 经 `gis::DemRaster`；`present_gpu` → `GpuScene::record` + 外置相机。不改 SP2 bridge present。

**Success (landed waves):** mask 委托、seed DEM、AABB 镜像、Views 无 `dem_height_field_static` — 勾选见 plan；本文不重开已勾选 Success。

---

## 13. SP4b — scene3d `index/` + open-source octree

**Goal (scheme A):** 仅 `legacy/render/scene3d/**`；去 `bl3d_`；`index/` 无 `LP3DRENDERDEVICE`；MIT header-only vendor + 薄适配；保留 `SmtScene` / `SmtSceneOctTree` 导出；无 shim。

**Locked (2026-09-29, approach A + point-cloud upgrade):** 删除手写 `Smt*OctTreeNode` 八叉细分；`SmtSceneOctTree` 扁平物体列表 + unibn；**`SmtVertexOctTree` 仅查询**（`hit_test` / `find_nearest` / `radius_neighbors` + leftover `HitTestOctNode`）；**`Smt3DPointCloud` 拥有 VB**，N≥20万时按空间网格分块并 frustum cull。大场景物体无层级裁剪、点云分块渲染是接受的 leftover 权衡。

**Non-goals:** 不 rewrite SP4 Success；不 wholesale 删 octree 换 World；不破 `dll_stem`；不加深嵌套；不引入 PCL/OpenVDB。

---

## 14. SP5 — Shell 编译闸门

**Goal:** `build.bat app|views` / `//src:src_all` 不拉 `legacy_*_all` / `ui_legacy` / MFC exe；opt-in `legacy_all` 保留；`assert_no_deps` + 文档钉死。

**Locked:** 默认壳 `SmartGisViews`；Views 不链 `dem_height_field_static`；`test_shell` 不含 leftover paint/MFC。

---

## 15. 风险

| 风险 | 缓解 |
| --- | --- |
| 并行改同一缝 | 路径所有权 + SP4 等 SP2 |
| 静默破 ABI | 默认禁止；SP 节显式授权 |
| 新开 SP/layout twin | **禁止** — 改本文对应 § 或 as-built README |

---

## 16. Done when（本文）

- [x] SP0–SP5（含 SP1b/SP4b）锁定决策在**同一 living 文件**
- [x] 原子规格已 archive（superseded/landed）
- [x] Plans 仍为勾选清单真源；不另开设计文件
- [x] Active 表 / `docs/README.md` 指向本文

实现工作只改代码 + 勾选 plan；**不要**为子目录或 SP 切片再开 dated design。

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `§`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-13-code-style-include-abi-cutover-design.md`](../archive/specs/2026-09-13-code-style-include-abi-cutover-design.md) | §ABI / include cutover (folded) |
| [`../archive/specs/2026-09-13-render-legacy-split-design.md`](../archive/specs/2026-09-13-render-legacy-split-design.md) | §SP2 / related render leftover split |
| [`../archive/specs/2026-09-14-app-legacy-split-design.md`](../archive/specs/2026-09-14-app-legacy-split-design.md) | §SP3 / related app leftover split |
| [`../archive/specs/2026-09-14-dll-reorganization-design.md`](../archive/specs/2026-09-14-dll-reorganization-design.md) | §DLL / package stems (folded) |
| [`../archive/specs/2026-09-19-leftover-gdiplus-carto-design.md`](../archive/specs/2026-09-19-leftover-gdiplus-carto-design.md) | §GDI+ carto leftover (folded) |
| [`../archive/specs/2026-09-19-leftover-scene3d-dem-unify-design.md`](../archive/specs/2026-09-19-leftover-scene3d-dem-unify-design.md) | §SP4 scene3d DEM unify (folded) |
| [`../archive/specs/2026-09-19-legacy-two-finger-pan-design.md`](../archive/specs/2026-09-19-legacy-two-finger-pan-design.md) | §two-finger pan (folded) |

