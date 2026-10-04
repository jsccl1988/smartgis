<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/render/atmosphere` 子目录布局 + 剥离解耦 Implementation Plan

**Status:** landed  
**Date:** 2026-09-27  
**Archived:** 2026-09-27 → [`docs/superpowers/archive/plans/`](./)  
**Spec (living, accepted):** [`../../specs/2026-09-27-atmosphere-subdirectory-layout-design.md`](../specs/2026-09-27-atmosphere-subdirectory-layout-design.md)  
**Capability (do not reopen API freeze casually):** [`../../specs/2026-09-19-atmosphere-ocean-cloud-design.md`](../specs/2026-09-19-atmosphere-ocean-cloud-design.md) · upgrade [`../../plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md`](../../plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md)  
**Precedent (landed):** RHI split [`2026-09-27-rhi-subdirectory-split.md`](2026-09-27-rhi-subdirectory-split.md)  
**As-built pointer:** [`../../../../src/render/README.md`](../../../../src/render/README.md)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.  
> Stay on **`master`**. **Do not `git commit`** unless the user explicitly asks. **Do not** open a branch.  
> This plan is **layout + thin compositor extraction**, not ocean/cloud algorithm upgrade.

**Goal:** 把扁平 `src/render/atmosphere` 收成稳定公开头 + 职责子目录，抽出 `AtmosphereFrame` 固化 pass 顺序，并预留 sky/fog 槽位与 GIS 3D 边界——行为与今日 ocean→land→cloud 兼容。

**Architecture:** 公开 `*.h` 留在模块根；`.cc` 迁入 `ocean/` `cloud/` `common/` `frame/`；宿主仍喂 GIS POD，Frame 只编排 `render::rhi`；不新建 DLL；GDI/GL 不做第二大气后端。

**Tech Stack:** C++23、GN `atmosphere_sources`、`render::rhi` Facade（FlyCube / Null）、现有 `OceanPass` / `CloudPass` / `FieldTexture`。

## Global Constraints

- 公开 include（事后改为头源同目录）：`render/atmosphere/ocean/ocean_pass.h`、`cloud/cloud_pass.h`、`common/field_texture.h`、`frame/atmosphere_frame.h`。  
- `render::atmosphere` **仅** deps `rhi_sources`；**禁止** include `gis/`、`legacy/`、FlyCube 公开头。  
- 命名空间两层 `render::atmosphere` + 可选 `detail`；函数 `snake_case`。  
- 与 P0 lit 同轨：陆地继续 `GpuScene` + `kLitSolid`；大气不另开 `Device`。  
- 不改动 FFT/raymarch 数值行为（纯搬移 / 抽编排）。  
- 用户未要求时不 commit。

---

## Locked decisions（禁止反转）

| # | Decision |
| --- | --- |
| 1 | 分层：逻辑场/`Environment` ∈ `gis::atmosphere`；GPU pass ∈ `render::atmosphere`；编排可留 Views，但 **pass 顺序契约** ∈ `AtmosphereFrame`。 |
| 2 | 公开头留模块根；实现进子目录（方案 B）。 |
| 3 | 绘制顺序：`sky? → ocean → opaque(GpuScene) → cloud → fog?`；与今日兼容（无 sky/fog 时 = 现状）。 |
| 4 | GPU 唯一真后端 FlyCube；Null 录制；**禁止** GDI/GL 真大气。 |
| 5 | 新 sky/fog **能力**可 Deferred；本 plan 最多加 **空壳目录/头 + Frame 开关钩子**，不实现完整散射。 |
| 6 | Path ownership 见文末；不与 RHI P0 / FieldStore 算法车道抢写。 |

---

## Non-goals

- 不实现完整 Hillaire/Bruneton、多次散射、浅水、GPU 风场粒子。  
- 不拆 `gis/atmosphere` 子目录（可另开 plan）。  
- 不改 `PipelineId` 集合（除非后续 capability 明确加 sky/fog Id——本 plan Phase 3 默认 **不加**）。  
- 不迁移 leftover `SetFog`。

---

## Current inventory → target（已落地 vs 待做）

| 项 | 状态 |
| --- | --- |
| `gis` / `render` 双模块拆分 | **已落地** |
| Pass → RHI only | **已落地** |
| `Scene3dController` ocean→land→cloud | **已落地**（经 `AtmosphereFrame`） |
| 子目录 `ocean/` `cloud/` `common/` | **已落地** |
| `AtmosphereFrame` | **已落地** |
| `sky/` `fog/` 槽 | **已落地**（README 占位 + Frame 开关） |
| DEM 高度雾 API | **文档映射已有**；代码 **Deferred** |

---

## File structure (after Phase 1–2)

```
src/render/atmosphere/
  BUILD.gn
  ocean_pass.h
  cloud_pass.h
  field_texture.h
  atmosphere_frame.h          # NEW
  ocean/ocean_pass.cc
  cloud/cloud_pass.cc
  common/field_texture.cc
  frame/atmosphere_frame.cc   # NEW
  sky/.gitkeep                # or short README stub (optional)
  fog/.gitkeep
  ocean_pass_test.cc
  cloud_pass_test.cc
  atmosphere_frame_test.cc    # NEW
```

---

## Path ownership（并行安全）

| 路径 | 本 plan |
| --- | --- |
| `src/render/atmosphere/**` | **独占** |
| `src/app/views/scene3d_controller.{h,cc}` | 仅改 atmosphere 录制接线（抽 Frame） |
| `src/render/README.md`、本 spec/plan、`docs/README.md` 索引行 | 文档 |
| `src/render/rhi/**` | **禁改**（本 plan） |
| `src/gis/atmosphere/**` | **禁改**（除非 Frame 测试需要只读常量；默认不动） |
| `src/render/scene/**` | **禁改**（load-op API 已存在） |

**Sibling lanes:** upgrade plan 算法项、RHI P0 lit —— 不要同 PR 混改。

---

## Phase 0 — 文档基线（本轮可视为已完成）

| | |
| --- | --- |
| **交付物** | living spec + 本 plan；交叉链到 2026-09-19 |
| **不做** | 改源码 |

- [x] Phase 0.1：写入 [`../../specs/2026-09-27-atmosphere-subdirectory-layout-design.md`](../specs/2026-09-27-atmosphere-subdirectory-layout-design.md)  
- [x] Phase 0.2：写入本 plan  
- [x] Phase 0.3：在 2026-09-19 spec 增加 Related / 目录表指针（见 Task 0）

### Task 0: 交叉链修订 2026-09-19

**Files:**
- Modify: `docs/superpowers/specs/2026-09-19-atmosphere-ocean-cloud-design.md`（Related + §1 目录表注记）
- Modify: `docs/README.md`（索引两行）
- Modify: `docs/superpowers/plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md`（Related 一行，可选）

- [x] **Step 1:** 在 2026-09-19 文首 Related 增加 layout spec/plan 链接；在「目录与 GN」表下加一句：物理子目录以 2026-09-27 layout spec 为准，能力决议仍以本文为准。  
- [x] **Step 2:** `docs/README.md` 索引表增加 layout design + plan 两行；刷新「最后更新」为 2026-09-27。  
- [x] **Step 3:** 确认无第二份平行空壳 design。

---

## Phase 1 — 物理子目录搬移（行为零变化）

| | |
| --- | --- |
| **交付物** | `.cc` 进入 `ocean/` `cloud/` `common/`；`BUILD.gn` 更新；测试绿 |
| **风险** | 路径漏改 → 编译失败；保持头文件路径不变可降风险 |
| **不做** | 抽函数、改算法、改 Scene3dController |

### Task 1: 移动实现文件并更新 GN

**Files:**
- Move: `src/render/atmosphere/ocean_pass.cc` → `src/render/atmosphere/ocean/ocean_pass.cc`
- Move: `src/render/atmosphere/cloud_pass.cc` → `src/render/atmosphere/cloud/cloud_pass.cc`
- Move: `src/render/atmosphere/field_texture.cc` → `src/render/atmosphere/common/field_texture.cc`
- Modify: `src/render/atmosphere/BUILD.gn`
- Keep: `ocean_pass.h` / `cloud_pass.h` / `field_texture.h` at module root

**Interfaces:**
- Consumes: 现有公开类 API 不变
- Produces: 相同符号，仅 TU 路径变

- [x] **Step 1: 移动三个 `.cc`（git mv 或等价）**

```bat
git mv src/render/atmosphere/ocean_pass.cc src/render/atmosphere/ocean/ocean_pass.cc
git mv src/render/atmosphere/cloud_pass.cc src/render/atmosphere/cloud/cloud_pass.cc
git mv src/render/atmosphere/field_texture.cc src/render/atmosphere/common/field_texture.cc
```

（若目录不存在先 `mkdir`。）

- [x] **Step 2: 更新 `BUILD.gn` `atmosphere_sources` 与 test 目标中的路径**

```gn
sources = [
  "cloud/cloud_pass.cc",
  "cloud_pass.h",
  "common/field_texture.cc",
  "field_texture.h",
  "ocean/ocean_pass.cc",
  "ocean_pass.h",
]
```

`ocean_pass_test` / `cloud_pass_test` 的 `sources` 同步指向新 `.cc` 路径；`#include "render/atmosphere/….h"` **不变**。

- [x] **Step 3: 编译验证**

```bat
build.bat "src/render/atmosphere:ocean_pass_test"
build.bat "src/render/atmosphere:cloud_pass_test"
```

Expected: 链接成功；测试 PASS（Null 路径）。

- [x] **Step 4: 更新 `src/render/README.md` atmosphere 行** — 注明子目录 as-built（实现落地时）。

---

## Phase 2 — `AtmosphereFrame` 编排抽出

| | |
| --- | --- |
| **交付物** | `atmosphere_frame.h` + `frame/atmosphere_frame.cc`；`Scene3dController` 改用 Frame；Null 顺序单测 |
| **依赖** | Phase 1 |
| **风险** | load-op 与 GpuScene 不同步 → 画面闪黑/丢海；对照今日 `record` 顺序逐行移植 |
| **不做** | sky/fog 真着色；风场 GDI 搬迁 |

### Task 2: 定义 `AtmosphereFrame` 公开 API

**Files:**
- Create: `src/render/atmosphere/frame/atmosphere_frame.h`
- Create: `src/render/atmosphere/frame/atmosphere_frame.cc`
- Create: `src/render/atmosphere/atmosphere_frame_test.cc`
- Modify: `src/render/atmosphere/BUILD.gn`

**Interfaces:**
- Consumes: `OceanPass` / `CloudPass`（非拥有或观察指针）、`rhi::Device` / `CommandList` / `CameraMatrices`
- Produces:

```cpp
namespace render::atmosphere {

// Owns pass-order policy for environment draws around opaque geometry.
class RENDER_EXPORT AtmosphereFrame {
 public:
  void set_ocean_pass(OceanPass* pass);
  void set_cloud_pass(CloudPass* pass);

  // Mirrors session toggles projected from gis::AtmosphereParams.
  void set_ocean_enabled(bool on);
  void set_cloud_enabled(bool on);

  // True if pre-opaque pass will clear color (ocean and/or future sky).
  bool clears_color() const;
  bool uses_shared_depth() const;

  // sky (future no-op) + ocean. Does not close the CommandList.
  bool record_pre_opaque(rhi::Device* device, rhi::CommandList* list,
                         uint32_t width, uint32_t height,
                         const rhi::CameraMatrices* camera);

  // clouds (+ future fog). Call after GpuScene::record_draws.
  bool record_post_opaque(rhi::Device* device, rhi::CommandList* list,
                          uint32_t width, uint32_t height,
                          const rhi::CameraMatrices* camera, int cloud_quality);
};

}  // namespace render::atmosphere
```

- [x] **Step 1: 写失败单测（Null Device）** — `atmosphere_frame_test.cc`：ocean 关时 `clears_color()==false`；ocean 开且 `OceanPass::record` 可调时 `clears_color()==true`；`record_pre/post` 在无 pass 指针时安全返回 true（no-op）或文档约定的 false——**选定：enabled 但 pass==nullptr → false；disabled → true no-op**。

```cpp
// atmosphere_frame_test.cc (sketch)
#include "render/atmosphere/frame/atmosphere_frame.h"
#include "render/rhi/rhi.h"

int main() {
  render::atmosphere::AtmosphereFrame frame;
  if (frame.clears_color()) return 1;
  frame.set_ocean_enabled(true);
  // no pass → record_pre_opaque must fail closed
  auto* dev = render::rhi::create_device(render::rhi::Backend::kNull);
  auto* list = dev->create_command_list();
  if (frame.record_pre_opaque(dev, list, 64, 64, nullptr)) return 2;
  frame.set_ocean_enabled(false);
  if (!frame.record_pre_opaque(dev, list, 64, 64, nullptr)) return 3;
  return 0;
}
```

- [x] **Step 2: 最小实现 `AtmosphereFrame`** — 把今日 `Scene3dController::record_atmosphere_ocean/clouds` 中 **与 GIS 无关** 的 `pass.record(...)` 调用迁入；GIS 采样仍留在 Controller（Controller 先 `set_params` 再调 Frame）。

- [x] **Step 3: 接线 `Scene3dController`**

```cpp
// After projecting OceanDrawParams / cloud knobs onto ocean_pass_/cloud_pass_:
gpu_scene_.set_color_load_op(frame_.clears_color() ? ColorLoadOp::kLoad
                                                   : ColorLoadOp::kClear);
gpu_scene_.set_enable_depth(frame_.uses_shared_depth());
gpu_scene_.set_depth_load_op(frame_.clears_color() ? DepthLoadOp::kLoad
                                                   : DepthLoadOp::kClear);
if (!frame_.record_pre_opaque(device, list, w, h, &cam)) return false;
if (!gpu_scene_.record_draws(device, list, w, h)) return false;
if (!frame_.record_post_opaque(device, list, w, h, &cam, quality)) return false;
```

保留 `record_atmosphere_ocean/clouds` 为 private 薄包装或删除（同 PR 内统一，避免双路径）。

- [x] **Step 4: 跑测**

```bat
build.bat "src/render/atmosphere:atmosphere_frame_test"
build.bat "src/render/atmosphere:ocean_pass_test"
build.bat "src/app/views:scene3d_controller_test"
```

Expected: PASS；showcase 行为目测不变（若跑 `--atmosphere-showcase`）。  
**Note (2026-09-27):** atmosphere Null 三测 PASS。`scene3d_controller_test` 当时被并行 `gis/**` 子目录搬迁（GN 未同步）阻塞，未纳入本车道验收门禁。

---

## Phase 3 — GIS 槽位：`sky/` `fog/` 占位（无完整算法）

| | |
| --- | --- |
| **交付物** | 空目录或 `README` 一句；`AtmosphereFrame` 预留 `set_sky_enabled` / `set_fog_enabled`（默认 false，no-op）；spec 映射表可勾「槽已建」 |
| **不做** | 新 `PipelineId`、LUT 烘焙、高度雾 shader |

### Task 3: 占位与开关钩子

**Files:**
- Create: `src/render/atmosphere/sky/README.md`（3–5 行英文：deferred SkyPass；see layout spec §4）
- Create: `src/render/atmosphere/fog/README.md`（同上）
- Modify: `atmosphere_frame.h` / `.cc` — enabled 标志 + no-op 分支
- Modify: layout spec §0「待做」表 — sky/fog 槽改为「目录已占位」

- [x] **Step 1:** 添加 `sky/` `fog/` README 占位。  
- [x] **Step 2:** Frame API 增加 `set_sky_enabled` / `set_fog_enabled`；`clears_color()` = ocean \|\| sky；`record_pre` 在 sky 开且无 pass 时 **仍 no-op 不失败**（与 ocean 不同：ocean 已有 pass；sky 未实现前 enabled 应被宿主保持 false——单测锁默认 false）。  
- [x] **Step 3:** 单测断言默认 sky/fog off；打开 sky 无实现时不崩溃（no-op true）。

---

## Phase 4 — 文档收口 + 验收

| | |
| --- | --- |
| **交付物** | README as-built；upgrade plan Related；本 plan Status 可标 landed 并视情况 archive |
| **不做** | 算法升档 |

- [x] **Step 1:** `src/render/README.md` atmosphere 行与目录表对齐目标树。  
- [x] **Step 2:** 自检：公开 include 无断裂；`render` 无 `gis`/`legacy` include（atmosphere TU）。  
- [x] **Step 3:** 对照 spec §4 映射表，确认「槽位/边界」列与代码一致；能力缺口仍标 Deferred。  
- [x] **Step 4:** 归档本 plan → `docs/superpowers/archive/plans/`（未 commit，待用户要求）。

---

## 测试矩阵（落地后）

| 目标 | 断言 |
| --- | --- |
| `ocean_pass_test` | 搬移后仍 Null 录制不崩 |
| `cloud_pass_test` | 同上 |
| `atmosphere_frame_test` | 开关 / clears_color / nullptr 契约 |
| `scene3d_controller_test` | demo enable / 顺序冒烟 |
| 可选 `SMT_RUN_FLYCUBE_GPU=1` showcase | 像素级非门禁；回归用 |

---

## Spec coverage（自检）

| Spec 要求 | Task |
| --- | --- |
| 剥离分层 / 依赖方向 | Phase 0 文档 + Phase 2 Frame |
| 子目录树 | Phase 1 |
| 稳定公开头 | Phase 1 锁定 |
| GIS 诉求映射 | Spec §4；槽位 Phase 3 |
| 不与 lit 双轨 | Global Constraints |
| 不改算法 | Phase 1–2 约束 |

**Placeholder scan:** 无 TBD 实现步骤；sky/fog **明确** no-op。  
**类型一致性：** `AtmosphereFrame` 方法名以 Task 2 块为准，后续 Task 不得改名。
