<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP1b: Tool 行为搬迁（pointer / digitize / select → Interaction）

**Status:** active  
**Date:** 2026-09-19  
**Scope:** 在 SP1（Workspace 激活 strangler）已落地的前提下，把导航 / 选择 / 数字化的**指针与手势逻辑**真正迁入 `src/tool` Interaction；`src/legacy/tool` 收成 bind + 可选 paint / 文档副作用壳。  
**Related:**

| 主题 | Spec |
| --- | --- |
| SP0 伞状契约 | [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md) |
| SP1 激活 strangler | [`2026-09-19-legacy-tool-workspace-strangler-design.md`](2026-09-19-legacy-tool-workspace-strangler-design.md) |
| Tool session dispatch（accepted） | [`2026-09-13-tool-event-dispatch-design.md`](2026-09-13-tool-event-dispatch-design.md) |
| Tool leftover 物理迁出（landed） | [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md) |
| 产品布局 as-built | [`../../build/src-layout.md`](../../build/src-layout.md)、[`src/tool/README.md`](../../../src/tool/README.md) |
| **SP1b 实现计划** | [`../plans/2026-09-19-tool-behavior-migration.md`](../plans/2026-09-19-tool-behavior-migration.md) |

---

## 1. Goal / Non-goals / 与 SP1·SP0 关系

### 1.1 Goal

**行为搬迁（锁定目标 C）**：当 Workspace 已绑定且对应 Interaction 已激活时：

1. **指针序列**（LDown / Move / LUp / RDown / wheel 在导航路径上）只由 `tool::Interaction` + `InputRouter` 消费；leftover `SmtIATool` **不再**实现平行鼠标状态机。
2. Interaction 产出 `tool::Draft`（像素空间）；`Workspace::on_draft` 更新 `last_draft_`、通知 `draft_observer_`、并按 interaction id 前缀走选择 / 视图 / 追加的既有 EventBus / EditSession 通道。
3. leftover 仅保留：**bind_workspace**、菜单 `notify` 转发、`apply_draft` 的**文档 / 相机 / 查询副作用**（DP→LP、Scratch 查询、AppendFeature、Zoom*），以及可选 `AuxDraw` 绘制 `live_preview()`。
4. 过关门 **A = T + H + L**（见第 5–7 节）：测试矩阵绿、ViewHost 可演示 pan+select+append、leftover 变薄且 bound 时禁止第二套指针捕获。

### 1.2 Non-goals

- **不**重开 SP0 依赖方向：终局 `src/tool/**` **禁止** `#include "legacy/**"`；legacy → `tool::Workspace` / `legacy_msg` 单向 OK。
- **不**改 leftover `dll_stem` / LoadLibrary ABI；**不** wholesale 重写全部 `SmtIATool`。
- **不**引入 Qt；**不**把 `EventBus` 放进 `src/tool`。
- **不**吞并 SP2 Present Facade、SP3 Host 行为提取、SP4 Scene3D、SP5 编译闸门（路径所有权见 §1.4）。
- **不**在本阶段从 factory **删除** `SmtInputPointTool` / `SmtInputLineTool` / `SmtInputRegionTool`（见 §7.3 / §8.3；已弱化为 `apply_draft` 壳，ABI 保留）。
- **不**本阶段做 View3D Present / Scene3D 大改（`view3d.*` 指针路径已补齐；设备层归 SP2/SP4）。
- **不**要求 MFC 全绿作为硬门；T 闸门以终局 `*_test` + ViewHost 宿主测试为准。

### 1.3 与 SP1 的关系（扩展 / 部分 supersede）

| | SP1（激活 strangler） | **本规格 SP1b（行为搬迁）** |
| --- | --- | --- |
| 焦点 | `GT_MSG_*` → `try_execute_gt_msg` → `Workspace::execute`；bound 时跳过 `SetActive` | Interaction **真正拥有**指针；leftover `apply_draft` 只消费 Draft |
| 已有成果 | Flash / ViewCtrl / Select / Append 的 `bind_workspace` + notify 转发 | 保留并**加严**：bound 时禁止 leftover 鼠标捕获与平行 OnLButton* 状态机 |
| SP1 Non-goals 调整 | 「不 rewrite 全部 group tool」仍成立 | **扩展**：对 ViewCtrl / Select / Append **三条主路径**做行为搬迁；Input* / 3D / 其它 group 仍延后 |
| 双路径 | 激活已走 Workspace | **仅绑定开关窗口期**保留双路径；bound = 终局指针，unbound = 旧 Notify + 旧鼠标 |

SP1 规格**不删除**；本文件是其子阶段。SP1 Done when（激活转发）视为 **前置已满足**（见 plan 勾选）；本文件 Done when 覆盖行为与闸门 A。

### 1.4 与 SP0 的关系

- 本规格为伞状 **Child：SP1b**（命名锁定；实现计划另开 `docs/superpowers/plans/`）。
- 遵守 SP0：手法 = Facade strangler + 热点提取；**禁止**终局→legacy；Git 在 **master**；路径所有权：

| 可改 | 禁改（勿 silently 交叉） |
| --- | --- |
| `src/tool/**`（Interaction / Draft / 测试矩阵） | `src/legacy/render/**`（SP2） |
| `src/legacy/tool/group/{viewctrl,select,appendfeature,flash,basetool}.*`（变薄） | `src/legacy/app/**`、`src/legacy/ui/**`（SP3） |
| `src/content/**` 薄接线（ViewHost / draft_observer 演示，不扩 ABI） | `src/legacy/render/scene3d/**`（SP4） |
| `docs/superpowers/specs|plans`、`src/tool/README.md`、必要时 `docs/build/src-layout.md` 一句 | 产品 `BUILD.gn` 编译闸门大改（SP5） |

冲突时：更窄的 accepted dispatch 规格优先于本文件表述；本文件不得与 SP0 §4 依赖规则矛盾。

---

## 2. Locked decisions

| 主题 | 选择 |
| --- | --- |
| 目标 | **C — 行为搬迁** |
| 范围 | **导航 + 选择 + 数字化全部并行**（`view.*` / `select.*` / `draw.*`） |
| 过关门 | **A**：T 测试闸门 + H 宿主可跑 + L leftover 变薄 |
| 手法 | **方案 2**：按能力竖切 + **共用测试矩阵**；双路径仅绑定开关窗口期 |
| 指针所有权 | **bound**：Workspace / Interaction；**unbound**：leftover 旧行为不变 |
| 细类型（spline / arc / anno / circle） | **粗映射 command/interaction id + 细类型进 Draft metadata**（§8） |
| Input* 工具 | **本阶段延后**（§7.3） |
| Git | 在 **master** 改；不新开分支 |

---

## 3. Architecture

### 3.1 竖切并行（方案 2）

三条能力竖切**同时推进**，共享同一测试矩阵与 Draft 契约，避免「先完美导航再碰选择」的长尾：

```
                    ┌─────────────────────────────────────┐
                    │  共用：Draft / legacy_msg / Workspace │
                    │  / draft_observer / 测试矩阵          │
                    └─────────────────────────────────────┘
           view.* │              select.* │         draw.*
                  ▼                       ▼              ▼
         camera_nav +            StrokeInteraction   StrokeInteraction
         make_view_*             (point/rect/poly)   (point/line/poly/rect)
                  │                       │              │
                  └─────────── Draft ─────┴──────────────┘
                                  │
                    leftover apply_draft（副作用壳）
```

- **竖切** = 每条能力独立 PR / agent 可改自己的 Interaction 工厂与对应 leftover 壳方法，但 **Draft 字段、command id、测试用例命名空间** 共用。
- **沉淀重开**：允许行为迁移后靠矩阵快速修 bug；不以「一次语义完美」为合并门槛。

### 3.2 双路径绑定开关（窗口期）

| 状态 | 激活 | 指针 | Draft 副作用 |
| --- | --- | --- | --- |
| `m_workspace == nullptr` | leftover `notify` 原逻辑（含 `SetActive`） | leftover 鼠标 / 委托 | leftover 自有逻辑 |
| `m_workspace != nullptr`（bound） | `try_execute_gt_msg` → `Workspace::execute`；**禁止** `SetActive` / 捕获 | 仅 `Workspace::dispatch_input` | Interaction → `on_draft` → observer → leftover `apply_draft` |

窗口期结束条件（进入「仅终局指针」）：§9 Done when 中 L 与 T 勾满，且产品壳对 ViewCtrl / Select / Append **均已** `bind_workspace`。未 bind 的调试路径仍可 unbound，但**不得**再给 bound 工具加回平行指针代码。

### 3.3 谁拥有指针（锁定）

1. **Chrome / ViewHost** 把命中地图的指针事件交给 `ViewHost::dispatch_input` → `Workspace::dispatch_input` → `InputRouter`（always-on `wheel.zoom` / `hover.cursor` + `stack.current()`）。
2. **bound leftover 工具禁止**：`OnLButtonDown/Up/Move`、`MouseWeel`（导航 wheel 已由 always-on / Draft）、`SetCapture` / `SetActive` 作为输入入口。
3. **leftover 仍可**：在 `apply_draft` / `notify` 里改 `m_viewMode` / `m_selMode` / `m_digitizeKind` / 细类型字段；在 `AuxDraw` 读 `workspace()->live_preview()` 画橡皮筋（若 chrome 未直接画 AuxOverlay）。

---

## 4. Components

### 4.1 Interaction 工厂（终局，已有 + 本阶段加固）

| Interaction id | 工厂（`gestures.*` / `camera_nav`） | 激活 command id |
| --- | --- | --- |
| `view.zoom_in` / `view.zoom_out` / `view.pan` | `make_view_*`（基于 `StrokeInteraction` + 相机数学） | `view.zoom_in` 等 |
| `select.point` / `select.rect` / `select.polygon` | `make_select_*` | `selection.point` 等 |
| `draw.point` / `draw.linestring` / `draw.polygon` / `draw.rect` | `make_draw_*` | `edit.append.point` / `linestring` / `polygon`（rect 由 polygon/linestring 粗映射 + metadata，见 §8） |
| always-on `wheel.zoom` / `hover.cursor` | Workspace 构造安装 | 不可 `execute` |

本阶段工作：补齐指针语义与测试覆盖，使 bound 路径**不再**依赖 leftover 内嵌状态机；不新增第三层公共命名空间。

### 4.2 Draft

现有（`gestures.h`）：

```cpp
enum class DraftKind { kPoint, kRect, kLineString, kPolygon, kKey, kWheel, kPick };
struct Draft {
  DraftKind kind;
  std::vector<DraftPoint> points;  // 像素
  uint32_t key = 0;
  int32_t wheel = 0;
  uint32_t flags = 0;  // 本阶段：细类型 / 圆选等 metadata 位或枚举码（§8）
};
```

- Interaction **只写**像素 Draft；**不**持有 `SmtMap*` / HWND / `LPRENDERDEVICE`。
- `flags`（或后续同结构体内的显式 `uint16_t subtype`）承载 §8 细类型；**不**为每个 spline/arc 开独立 command id。

### 4.3 Workspace

- `register_builtins` / `bind_activate` / `on_draft` / `set_draft_observer` / `live_preview`：**保持**为组合根。
- `on_draft` 规则不变：`select.*` → `SelectionChanged`；`view.*` / wheel → `ExtentChanged`；`draw.*` → `EditSession::commit` + `EditCommitted`；并**始终**调用 `draft_observer_`（供 leftover / chrome 副作用）。

### 4.4 EditSession / EventBus

- 文档写入权威：`gis::EditSession`（ViewHost 默认可 `MemoryEditSession`；leftover Append 壳可继续用 `MapEditSession` 做真实 `SmtMap` 追加）。
- 域事件：`content::EventBus`（不在 `src/tool`）。
- Interaction **禁止**直接 `SmtMap::AppendFeature`。

### 4.5 leftover 壳职责（L）

| 工具 | 保留 | 迁出 / 禁止（bound） |
| --- | --- | --- |
| `SmtViewCtrlTool` | `bind_workspace`；`notify` 同步 `m_viewMode` + `try_execute_gt_msg`；`apply_draft` → Zoom* / ApplyWheel；可选 AuxDraw | bound：**禁止** ZoomIn/Out/Move 的鼠标状态机入口；`MouseWeel` 不作为主路径 |
| `SmtSelectTool` | bind；notify 同步 `m_selMode`；`apply_draft` → 查询几何 + `OnRetDelegate` / scratch | bound：**禁止**平行选择拖拽状态机；圆选细类型见 §8 |
| `SmtAppendFeatureTool` | bind；notify 设 `m_digitizeKind` + 细类型；`apply_draft` → Append* | bound：**禁止** `SetActive` / 委托 Input* 抢指针；细类型进 Draft metadata 后逐步减少对 notify 闭包状态的依赖 |
| `SmtFlashTool` | 已 SP1；本阶段仅保持 overlay 读 `flashing()` | 无指针状态机 |
| `SmtBaseTool` | `apply_draft` 虚接口 | 不新增终局依赖 |

### 4.6 legacy_msg

- 继续唯一 `GT_MSG_*` → string id 表（`command_id_from_gt_msg` / `try_execute_gt_msg`）。
- 粗映射保持：多条 Append GT_MSG → `edit.append.point|linestring|polygon`（已与 dispatch 规格一致）。
- 圆选：见 §8（无独立菜单 GT_MSG 时经 `SET_SEL_MODE` / metadata）。

---

## 5. Data flow

### 5.1 菜单激活（bound）

```
菜单 / 插件 Notify(GT_MSG_*)
        │
        ├─ try_execute_gt_msg(ws) ──► Workspace::execute(command_id)
        │                                    │
        │                                    ▼
        │                             catalog → stack.activate(interaction_id)
        │
        └─ leftover：同步 m_viewMode / m_selMode / m_digitizeKind(+细类型)
                     EndDelegate；禁止 SetActive
```

Chrome 亦可 `ViewHost::execute_legacy(gt_msg)` 或 `execute("selection.rect")`；id 表同一份。

### 5.2 指针 → Draft → 副作用

```
指针事件
  → ViewHost::dispatch_input
  → Workspace::dispatch_input
  → InputRouter（wheel.zoom / hover → current Interaction）
  → Interaction::on_input → DraftCallback
  → Workspace::on_draft
        ├─ last_draft_ + draft_observer_（chrome / leftover apply_draft）
        ├─ select.*  → EventBus SelectionChanged
        ├─ view.* / wheel → ExtentChanged
        └─ draw.* → EditSession::commit + EditCommitted
```

leftover `apply_draft`（observer 内调用）负责：DPToLP、相机、查询、真实要素追加。Memory 路径下宿主测试可不挂 leftover，只断言 EventBus / EditSession。

### 5.3 unbound（窗口期兼容）

保持 SP1 / 2010 行为：`notify` 可 `SetActive`，指针走 `SmtIATool`；**不**要求 observer。新代码不得在 unbound 路径上增加终局依赖。

---

## 6. Testing matrix（过关门 A 的 T）

### 6.1 矩阵维度（共用）

| 维度 | 必覆盖用例族 |
| --- | --- |
| **激活** | 每个 mapped `GT_MSG_*` / command id → `stack().current()->id()` 正确；`try_execute_gt_msg(nullptr)` / 未映射 → false |
| **指针序列** | 每类 Interaction：LDown→Move→LUp（及 polygon 的 RDown/LDClick）；中途 RDown/Esc 取消；hover 不吞 move（`hover.cursor`） |
| **Draft** | `kind` + `points` 形状；`live_preview` 在 LUp 前正确；`flags`/subtype 在粗映射细类型场景非零 |
| **legacy_msg** | 数值枚举与 `defs.h` 同步；Append 多 GT_MSG 粗映射；`SMT_MSG_KEY` 低字递归 |
| **Workspace 副作用** | select → SelectionChanged；draw → commit；view/wheel → ExtentChanged；observer 被调用 |

### 6.2 测试文件归属

| 文件 | 归属 | 内容 |
| --- | --- | --- |
| `src/tool/dispatch_test.cc` | 终局 | Catalog / Stack / Router / `try_execute_gt_msg` / 内置 activate |
| `src/tool/camera_nav_test.cc` | 终局 | `wheel_zoom_factor` / `zoom_at_client_point` / `is_navigate_tool` |
| **新建或扩展** `src/tool/gestures_test.cc`（或扩 `dispatch_test` 手势段） | 终局 | 三条竖切的指针序列 + Draft + preview（**推荐独立文件**以免 dispatch_test 膨胀） |
| `src/content/view_host_test.cc` | 宿主 | execute_legacy、dispatch_input、EditCommitted、flashing（扩 pan+select+append 最小路径） |
| leftover group | **不**新建 MFC DLL 测试硬门 | 行为回归靠终局矩阵 + 手工 / 可选 opt-in |

命令闸门（实现计划填写具体 ninja 目标）：`tool_dispatch_test`、`camera_nav_test`、手势测试目标、`view_host_test` 全绿。

---

## 7. Host gate（过关门 A 的 H）

**最小可演示路径**（不依赖完整 MFC 菜单）：

1. 构造 `content::ViewHost`（可注入 `MemoryEditSession`）。
2. `execute("view.pan")` → `dispatch_input` 拖拽序列 → `ExtentChanged` 或 observer 收到 `kRect`/`kWheel` Draft。
3. `execute("selection.rect")` → 拖拽 → `SelectionChanged`。
4. `execute("edit.append.point")` → 点击 → `EditCommitted` 且 `edits()->can_undo()`。

可选增强（非 H 硬门）：Views / `MapViewport` 或 xview `bind_draft_observer` 接到 leftover `apply_draft`，演示真实地图相机与选中闪烁——属 SP3 / chrome 接线，本规格只要求 **ViewHost API 层**可跑。

---

## 8. Leftover thin（过关门 A 的 L）

### 8.1 迁出方法（从 leftover 状态机 → Interaction）

| 区域 | 迁出内容 |
| --- | --- |
| ViewCtrl | `ZoomIn` / `ZoomOut` / `ZoomMove` 的捕获与拖拽几何；wheel 主路径 |
| Select | 点选 / 框选 / 多边形选的拖拽与橡皮筋几何 |
| Append | 点 / 线 / 面数字化的顶点收集与完成手势（RDown / 双击） |

leftover 保留的 `apply_draft`：**消费**已完成 Draft，做坐标变换与文档/查询副作用。

### 8.2 bound 时禁止

- `SetActive()` / `SetCapture` 作为工具激活手段。
- 在 `OnLButton*` / `MouseMove` / `MouseWeel` 中推进数字化或导航状态（bound 工具应空实现或直接转 `dispatch_input` 若仍被错误调用——推荐空实现 + 断言/日志，避免双消费）。
- 新增终局头对 leftover 的 include。

### 8.3 Input* 是否本阶段延后（锁定：**延后删除；行为已弱化**）

`SmtInputPointTool` / `SmtInputLineTool` / `SmtInputRegionTool`：

- 历史角色是 Append 的 **BeginDelegate** 子工具。
- bound 路径下 Append **不再** `SetActive` 到 Input*；数字化由 `draw.*` Interaction 完成。
- **清尾（Task 7）**：指针状态机已移除；`OnSet*Type` 不再 `SetActive`；几何仅 `apply_draft`。`GTT_Input*` 工厂 / orthogrid 创建路径 **保留**（CBM 仍见引用）。
- **仍未做**：从 factory 删除类型、合并进 draw.*、修 unbound Append 无指针数字化（SP3 / SP5 ABI 清理）。

---

## 9. 细分 append 与 circle select（锁定选择）

### 9.1 推荐（锁定）：粗映射 + 细类型进 Draft metadata

| 菜单 / 模式 | Command id（粗） | Interaction id（粗） | 细类型存放 |
| --- | --- | --- | --- |
| anno / dot / child image | `edit.append.point` | `draw.point` | `Draft.flags`（或 `subtype`）= leftover `PT_*` |
| spline* / arc / ring / line string / line rect | `edit.append.linestring` | `draw.linestring`（线框可用 `draw.rect` 若几何为对角矩形） | `flags` = leftover `LT_*` / 线型枚举 |
| fan / surf rect / polygon | `edit.append.polygon` | `draw.polygon` / `draw.rect` | `flags` = 面型枚举 |
| 圆选 `ST_Circle` | **无新菜单 command**（`defs.h` 无 `GT_MSG_SELECT_CIRCLE*`） | 本阶段：`select.rect` 几何 + `flags` 标记 circle；或注册 `select.circle` **仅** interaction id，**不**新增 GT_MSG | `flags` 区分 rect vs circle；`apply_draft` 按 flags 建圆/椭圆查询几何 |

**明确拒绝本阶段**：为每个 spline/arc/anno 增加独立 `edit.append.spline_b` 等 command id（会炸 catalog 与菜单映射表）。

**激活时**：notify 仍可写 leftover `m_pointType` 等作为窗口期双写；矩阵稳定后以 Draft.flags 为 apply 权威，壳字段只作兼容。

### 9.2 Draft.flags 编码表（锁定）

布局：`flags = (family << 16) | (code & 0xffff)`。`family` / `code` 与 leftover `defs.h` 枚举数值对齐，便于 shell 双写。

| Family 名 | `family` | `code` 来源 | 示例 |
| --- | --- | --- | --- |
| none | `0` | — | 默认几何，无细类型 |
| point | `1` | `ePointType`（`PT_DOT=0` …） | anno → `(1<<16)\|PT_Text` |
| line | `2` | `eLineType`（`LT_Arc=0` …） | spline_b → `(2<<16)\|LT_Spline_B` |
| region | `3` | `eRegionType`（`RT_Fan=0` …） | surf rect → `(3<<16)\|RT_Rect` |
| select | `4` | `0`=默认；`1`=`ST_Circle` | 圆选 → `(4<<16)\|1` |

终局 API（`gestures.h` / `Workspace`）：

- `draft_flags::pack(family, code)` / `family_of` / `code_of` / 谓词（如 `is_select_circle`）。
- `Workspace::set_draft_flags(uint32_t)`：leftover notify / 测试在激活后写入；`on_draft` 若 Interaction 未自带非零 `flags` 则盖上 pending。
- Interaction 也可在构造时带默认 flags（如 `select.circle`）；与 pending 合并时 **非零 Interaction flags 优先**，否则用 pending。

### 9.3 circle select 细节

- `eSelectMode::ST_Circle` 存在，但 CMD 段仅有 point/rect/polygon/clear。
- 本阶段：经 `GT_MSG_SET_SEL_MODE` 设为 circle 时，bound 路径 `activate("select.rect")` 或注册 `select.circle`（同一 `StrokeMode::kRect`），`Draft.flags` 置 `pack(select, 1)`；`apply_draft` 用两点作直径生成圆/椭圆查询几何。
- **不**发明新的 `GT_MSG_SELECT_CIRCLESEL` ABI。

---

## 10. Dependency / ABI / Path ownership

### 10.1 Dependency（重申）

```
src/tool, src/content/public, …
        ▲  禁止 include legacy
src/legacy/tool/group
        ▼  允许
tool::Workspace / tool::legacy_msg / tool::Draft
```

GN：`//src/legacy/tool/group` → `//src/tool:dispatch` OK；`//src/tool:dispatch` **不得** dep leftover。

### 10.2 ABI

- leftover DLL stem / 导出：**不变**。
- `GT_MSG_*` 数值：**不变**（`legacy_msg.h` 与 `defs.h` 同步）。
- 终局 API：两层命名空间 + `snake_case`；可扩展 `Draft` 字段，保持默认零初始化兼容。

### 10.3 Path ownership

见 §1.4。与 SP1 重叠的 `src/legacy/tool/group` 与 `src/tool`：**本阶段由 SP1b 独占行为搬迁**；SP1 激活相关改动视为已完成基线，避免并行 agent 再改同一 notify 而无协调。

---

## 11. Success criteria（可验证）

1. `src/tool/**` 公共头与实现 **无** `#include "legacy/..."`。
2. bound 下 ViewCtrl / Select / Append：**指针**仅经 Workspace；leftover 无平行捕获状态机（代码审查 + 测试）。
3. 测试矩阵 §6 所列目标全绿。
4. `view_host_test`（或等价）演示 pan + select.rect + append.point。
5. `dll_stem` / GT_MSG 数值未改。
6. `src/tool/README.md`（及若布局表过时则 `docs/build/src-layout.md`）写明 SP1b 行为搬迁与壳职责。
7. Input* factory ABI 保留、指针死路径已弱化；view3d Present 大改仍 Out of scope（§12）。

---

## 12. Out of scope for later SPs

| 项 | 去向 |
| --- | --- |
| Input* **从 factory 删除** / 合并进 draw.*（ABI 清理） | SP5；清尾已弱化为 `apply_draft` 壳，**保留** `GTT_Input*`（orthogrid 等仍 Create） |
| unbound Append **无指针数字化**（已不 BeginDelegate Input*） | SP3 chrome / 插件改走 Workspace；tool 侧仅注释 |
| View3D **Present / Scene3D 大改** | SP2 / SP4；指针→`view3d.*` + leftover `apply_draft` 已在本补齐切片落地 |
| Present / HWND 设备 | SP2 |
| MFC chrome 全面 Views parity（非薄 bind） | SP3 / ui-views-mfc-migration |
| 默认壳去 legacy 编译 | SP5 |

---

## 13. Risks

| 风险 | 缓解 |
| --- | --- |
| bound/unbound 双路径漂移 | 测试矩阵强制 bound 语义；窗口期结束后禁止给 bound 加 leftover 指针代码 |
| Draft.flags 语义含糊导致 apply 错型 | 在 `gestures.h` / 测试中固定 subtype 编码表；append notify 与 flags 双写至矩阵稳定 |
| 与 SP3 同时改 ViewHost / xview 接线冲突 | H 闸门只要求 ViewHost 测试；chrome bind 归 SP3，本规格只留薄可选接线 |

---

## 14. Done when

- [x] Status `active`；Related 链回 SP0 / SP1 / tool-event-dispatch
- [x] Goal / Non-goals / Locked decisions / 竖切架构 / 双路径 / 指针所有权无 TBD
- [x] Components、Data flow、Testing matrix、Host gate、Leftover thin、粗映射+metadata、Input* 延后均已写明
- [x] Success criteria 可勾选；Out of scope 与 SP2–SP5 边界清晰
- [x] §9.2 Draft.flags 编码表锁定（消除 flags 歧义）
- [x] 伞状 SP0 Child 表增加 **SP1b** 行；plan：[`../plans/2026-09-19-tool-behavior-migration.md`](../plans/2026-09-19-tool-behavior-migration.md)
- [x] （实现阶段）T+H+L 闸门实测通过；plan 勾选随实现更新

实现工作不在本文展开；落地以 plan 为准。
