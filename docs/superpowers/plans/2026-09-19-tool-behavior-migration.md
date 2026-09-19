<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# SP1b: Tool 行为搬迁 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Bound 路径下 view/select/draw 指针只由 `tool::Interaction` 消费并产出 `Draft`（含固定 `Draft.flags` 细类型），leftover ViewCtrl/Select/Append 变薄为 bind + notify + `apply_draft`；过关门 A = T+H+L。

**Architecture:** 方案 2 竖切：共用 Draft / Workspace / 测试矩阵；三条能力并行加固。双路径仅绑定开关窗口期。终局禁止 `#include "legacy/**"`。

**Tech Stack:** C++23、`//src/tool:dispatch`、`content::ViewHost`、GN/`build.bat`、gtest-free `*_test` main。

## Global Constraints

- Git：在 **master** 改；本轮 **不 commit**（除非用户另说）。
- SP0：终局 `src/tool/**` ↛ legacy。
- 不改 leftover `dll_stem` / `GT_MSG_*` 数值；不碰 SP2/SP3/SP4 路径。
- 新函数 `snake_case`；注释英文；版权 2026。
- Input* / view3d 行为搬迁 **本 plan Task 1–5 不覆盖**；Task 6 补齐 View3D + Input* 边界；Task 7 清尾 Input* 死路径（保留 factory ABI）。

**Spec:** [`../specs/2026-09-19-tool-behavior-migration-design.md`](../specs/2026-09-19-tool-behavior-migration-design.md)

---

### Task 1: Draft.flags 编码 + Workspace pending

**Files:**
- Modify: `src/tool/gestures.h`
- Modify: `src/tool/gestures.cc`（StrokeInteraction 可选 default flags；point LDown/LUp 不双触发）
- Modify: `src/tool/workspace.h`、`src/tool/workspace.cc`
- Test: `src/tool/gestures_test.cc`（Task 2 创建）

**Interfaces:**
- Produces: `tool::draft_flags::pack/family_of/code_of/is_select_circle`；`Workspace::set_draft_flags` / `draft_flags`
- Consumes: existing `Draft` / `bind_draft` / `on_draft`

- [x] **Step 1: Add encoding helpers to `gestures.h`**
- [x] **Step 2: Workspace pending flags**
- [x] **Step 3: Point stroke — emit once per click**

---

### Task 2: T 闸门 — `gestures_test.cc` + BUILD

**Files:**
- Create: `src/tool/gestures_test.cc`
- Modify: `src/tool/BUILD.gn`
- Modify: `src/tool/dispatch_test.cc`（补 pan+select+append 串联断言，若尚未覆盖）
- Modify: `src/tool/camera_nav_test.cc`（保持 navigate 谓词；可选补一句）

**Interfaces:**
- Consumes: Task 1 flags API、`Workspace::execute` / `dispatch_input` / `live_preview`

- [x] **Step 1: Register test target**

```gn
test("gestures_test") {
  output_name = "gestures_test"
  sources = [ "gestures_test.cc" ]
  include_dirs += [ "//src" ]
  deps = [ ":dispatch" ]
}
```

- [x] **Step 2: Write failing/covering tests in `gestures_test.cc`**

覆盖：

1. `draft_flags::pack` / `is_select_circle` 表驱动。
2. `view.pan`：LDown→Move→LUp → `DraftKind::kRect` + `ExtentChanged`。
3. `selection.rect`：同上 + `SelectionChanged`；拖拽中 `live_preview` 为 `kRect`。
4. `selection.rect` + `set_draft_flags(pack(select,1))` → `last_draft().flags` circle。
5. `edit.append.point`：点击 → commit；`edit.append.linestring`：两点 + RDown → `kLineString`；Esc 取消清空。
6. hover：无 exclusive 时 move 不吞；有 rect 捕获时 move 吞。

- [x] **Step 3: Expand `dispatch_test` 串联**

同一 `Workspace`：`view.pan` 拖拽 → `selection.rect` 拖拽 → `edit.append.point` 点击；断言事件计数与 `last_draft` 种类依次变化。

- [x] **Step 4: Run**

```bat
.\build.bat tool_dispatch_test
.\build.bat gestures_test
.\build.bat camera_nav_test
```

Expected: exit 0；各打印 `*: ok`。

---

### Task 3: L leftover 变薄（ViewCtrl / Select / Append）

**Files:**
- Modify: `src/legacy/tool/group/viewctrltool.cpp`
- Modify: `src/legacy/tool/group/selecttool.cpp`
- Modify: `src/legacy/tool/group/appendfeaturetool.cpp`
- （头文件仅在需声明时改）

**Interfaces:**
- Consumes: `Workspace::set_draft_flags`、`draft_flags::pack`、`try_execute_gt_msg`
- Produces: bound 时无平行指针入口；`apply_draft` 读 flags

- [x] **Step 1: ViewCtrl — bound 禁用 MouseWeel 主路径**

```cpp
int SmtViewCtrlTool::MouseWeel(...) {
  if (m_workspace) {
    return SMT_ERR_NONE;  // wheel via Workspace always-on / Draft
  }
  // existing ApplyWheel path
}
```

保留 `apply_draft` 对 `kWheel` / 拖拽几何的副作用。

- [x] **Step 2: Select — circle 模式 + KeyDown**

`GT_MSG_SET_SEL_MODE`：若 `m_workspace` 且 `ST_Circle`，`set_draft_flags(pack(select,1))` + `activate("select.rect")`；其它 mode 清 flags 并 activate 对应 interaction。

`KeyDown` clear：`if (!m_workspace) SetActive();`

`apply_draft`：若 `draft_flags::is_select_circle(draft.flags) || m_selMode == ST_Circle`，用两点直径建圆查询几何（近似：中心+半径的 `OGRLinearRing` 或已有 OGR 圆构造）。

- [x] **Step 3: Append — notify 写入 flags**

`via_ws` 分支：在设 `m_pointType` / `m_lineType` / `m_regionType` 后：

```cpp
m_workspace->set_draft_flags(
    tool::draft_flags::pack(tool::draft_flags::kFamilyPoint, unType));
```

（line/region 同理。）

`apply_draft`：若 `draft.flags != 0`，用 `family_of`/`code_of` 覆盖本地 kind/type 再 Append（双写窗口期）。

- [x] **Step 4: 确认无新增终局→legacy include；不改 defs.h GT_MSG 数值**

---

### Task 4: H 宿主 — `view_host_test` pan+select+append

**Files:**
- Modify: `src/content/view_host_test.cc`

**Interfaces:**
- Consumes: `ViewHost::execute` / `dispatch_input` / `events()` / `edits()`

- [x] **Step 1: Add combined scenario**

```cpp
content::ViewHost host;
int extents = 0, sels = 0, commits = 0;
// subscribe ExtentChanged / SelectionChanged / EditCommitted
host.execute("view.pan");
// LDown(1,1) Move(20,20) LUp → extents >= 1
host.execute("selection.rect");
// drag → sels >= 1
host.execute("edit.append.point");
// LDown → commits >= 1 && edits()->can_undo()
```

- [x] **Step 2: Run**

```bat
.\build.bat content_view_host_test
```

Expected: `content_view_host_test: ok`

---

### Task 5: Docs 收尾勾选 + 验证

**Files:**
- Modify: 本 plan 勾选；spec §14 实现项；必要时 `docs/build/src-layout.md` 一句

- [x] **Step 1: 跑齐闸门目标**

```bat
.\build.bat tool_dispatch_test
.\build.bat gestures_test
.\build.bat camera_nav_test
.\build.bat content_view_host_test
```

- [x] **Step 2: 勾选本 plan Tasks 1–4；spec Done when 实现项**

- [x] **Step 3: 不 commit**（用户未要求）

---

### Task 6: 补齐切片（SP1b 窗口期缝隙 + View3D + Input*）

> 承接 Task 1–5 已绿基线；在 **master** 继续补齐，**不 commit**。

**Files:**
- Modify: `src/legacy/ui/xview/view_2d.cpp`、`view_3d.cpp`
- Modify: `src/legacy/tool/group/{viewctrl,select,appendfeature,3dviewctrl}tool.*`
- Modify: `src/legacy/tool/group/input{point,line,region}tool.h`（边界注释）
- Modify: `src/tool/gestures_test.cc`、`src/tool/README.md`、本 plan、design §12

- [x] **Step 1: P0 chrome bind** — browse `view_2d` 创建 `ViewHost` 并 bind ViewCtrl/Select/Flash；`view_3d` 创建 host + bind 3DViewCtrl
- [x] **Step 2: P0 bound 空实现** — ViewCtrl/Select/Append/3DViewCtrl 的 LButton*/MouseMove(/MouseWeel) 在 `m_workspace` 时 no-op；3D notify 走 `try_execute_gt_msg` 且 bound 禁 `SetActive`
- [x] **Step 3: P1 View3D** — leftover 变薄 + gestures 矩阵补 trackball/orbit/fps/wheel
- [x] **Step 4: P2 Input*** — 头文件标明 unbound-only；测试矩阵对照 `draw.*` ↔ Input*
- [x] **Step 5: 跑闸门测试** — `tool_dispatch_test` / `gestures_test` / `camera_nav_test` / `content_view_host_test`（及可行的 `tool_group`）

---

### Task 7: 清尾切片（Input* 死路径 + leftover 双消费扫尾）

> 承接 Task 6；在 **master** 继续清尾，**不 commit**。不碰 SP2/SP3 大改。

**Files:**
- Modify: `src/legacy/tool/group/input{point,line,region}tool.{h,cpp}`
- Modify: `src/legacy/tool/group/grouptoolfactory.cpp`、`appendfeaturetool.cpp`（注释）
- Modify: `src/tool/gestures_test.cc`、`src/tool/README.md`、本 plan、design §8.3 / §12

**核实（CBM）：** `SmtInput*` 仍被 `SmtGroupToolFactory::CreateGroupTool` 与 orthogrid `CreateIAGetLineTool` 引用 → **保留** `GTT_Input*` 工厂分支与类导出；**不删** LoadLibrary ABI。

- [x] **Step 1: Input* 弱化** — 去掉 `OnSet*Type` 内 `SetActive`（由调用方显式激活）；删 Line/Region 空 `KeyDown` 转发；头文件标明仅 `apply_draft` 几何路径
- [x] **Step 2: leftover 双消费扫** — ViewCtrl/Select/Append/3DViewCtrl bound 指针已 no-op；chrome 未创建 Input*（仅 factory/plugin）；Append unbound 不再 BeginDelegate（注释标明退化）
- [x] **Step 3: 文档** — plan 本 Task；design Out of scope / §8.3；`src/tool/README.md` 清尾一句
- [x] **Step 4: 闸门** — `tool_dispatch_test` / `gestures_test` / `camera_nav_test`

---

## Self-review (plan vs spec)

| Spec 要求 | Task |
| --- | --- |
| Draft.flags 编码表 | Task 1 + spec §9.2 |
| 指针序列 + live_preview 矩阵 | Task 2 |
| ViewHost pan+select+append | Task 4 |
| leftover bound 禁平行指针 / SetActive | Task 3 + Task 6 |
| Input* / view3d 延后 → **部分收口** | Task 6（文档）+ Task 7（死路径弱化；ABI 保留） |
| 终局 ↛ legacy | Task 3 Step 4 + 代码审查 |
