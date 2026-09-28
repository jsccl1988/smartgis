<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# `src/render/rhi` 子目录拆分 + 深度抽象 Implementation Plan

**Status:** landed  
**Date:** 2026-09-27  
**Archived:** 2026-09-27 → [`docs/superpowers/archive/plans/`](./)  
**Related:** living RHI contract [`../../specs/2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md)；P0 能力 [`../../specs/2026-09-20-rhi-3d-capability-p0-design.md`](../../specs/2026-09-20-rhi-3d-capability-p0-design.md) / [`../../plans/2026-09-20-rhi-3d-capability-p0.md`](../../plans/2026-09-20-rhi-3d-capability-p0.md)；leftover 迁出 [`../../specs/2026-09-13-render-legacy-split-design.md`](../../specs/2026-09-13-render-legacy-split-design.md)；present strangler [`../../plans/2026-09-19-legacy-render-present-facade.md`](../../plans/2026-09-19-legacy-render-present-facade.md)。  
**As-built pointer:** [`../../../../src/render/README.md`](../../../../src/render/README.md)

> **For agentic workers:** 按 Phase 勾选落地。只在 **master** 改。**不要** `git commit`，除非用户明确要求。**不要**开分支。本 plan 是目录/职责拆分，不是能力升级；不改 `rhi.h` 对外语义。

---

## Goal

把 `src/render/rhi` 从「扁平 4 个假后端 + 1 个 ~2400 行 FlyCube 巨石」收成：

1. **稳定公开 Facade**（`rhi.h` / `rhi.cc`）不变语义；
2. **`stub/`**：合并 Null / GDI / GL 为带 `Backend` 标签的 `StubDevice` + present 策略；
3. **`flycube/`**：按职责切开 `flycube_rhi.cc`（device / resources / commands / pipelines / execute / compute）；
4. 测试与 GN 绿，**不**把 GDI/GL 做成第二套真实画图后端。

## Non-goals

- **不**充实 GDI/GL 为真实 draw/raster 后端（禁止在 `rhi/gdi`、`rhi/gl` 下新增 peer 真后端）。
- **不**把 `src/legacy/render/{gdi,gl,…}` 迁回终局树。
- **不**改 `Backend` 枚举值集合（**保留** `kGdi` / `kGl` 与 factory 分支，供 HWND fallback + 既有测试）。
- **不**改 `create_device` / `preferred_gpu_backend` / `StubCommandList` 对外契约；公开头仍零 FlyCube 类型。
- **不**拆第二份 render DLL；仍是 `//src/render:rhi_sources` → `:render`。
- **不**做 lit/PBR/atmosphere 能力升级（那是 P0 / atmosphere 车道）。
- **不**引入 Qt / 第二图形引擎 / 公开 `#include` FlyCube。

## Locked decisions（禁止反转）

| # | Decision |
| --- | --- |
| 1 | `rhi.h` 合同：GPU **只有 FlyCube**（DX12 / Vulkan）。`kGdi` / `kGl` 是 leftover **HWND 适配器**，不是第二 CommandList 光栅器。 |
| 2 | 现状：`flycube_rhi.cc`（`#ifdef SMT_HAS_FLYCUBE` 真设备 ~L40–2424；`#else` stub ~L2425–2456）是唯一真 device；`null_rhi.cc` / `gdi_rhi.cc` / `gl_rhi.cc` 均用 `StubCommandList`；GDI `present` = `InvalidateRect`；GL `present` 空；`preferred_gpu_backend` Win=`kDx12` else `kVulkan`。真 GDI/GL 在 `src/legacy/render`，**不得**迁回。 |
| 3 | 目标树方向见下；**禁止** `rhi/gdi/`、`rhi/gl/` 作为真后端目录。 |
| 4 | 保留 `enum Backend { … kGdi, kGl }` 与 `create_device` 分支。 |

## Current inventory（CBM / 源码核对）

| 路径 | 角色 | 量级 |
| --- | --- | --- |
| `rhi.h` | 公开 Facade：`Device` / `CommandList` / `Stub*` / `detail::make_stub_*` | 稳定 |
| `rhi.cc` | `create_device`、`preferred_gpu_backend`、`backend_display_name`、camera helpers | ~160 行 |
| `null_rhi.cc` | `NullDevice`：leak destroy；`execute_count` | ~55 行 |
| `gdi_rhi.cc` | `GdiDevice`：HWND + `InvalidateRect` | ~55 行 |
| `gl_rhi.cc` | `GlDevice`：HWND + 空 present | ~50 行 |
| `flycube_rhi.cc` | 真 FlyCube + `#else` stub；内含 Buffer/Texture/CommandList/Device/pipelines/compute | ~2460 行 |
| `rhi_test.cc` | Null 录制 + 公开头无 FlyCube include；可选 `SMT_RUN_FLYCUBE_GPU=1` | 根侧 |

**FlyCube 巨石内已有职责缝（拆分锚点，行号约）：**

| 职责 | 符号 / 区间（约） |
| --- | --- |
| CB POD + shaders | `CameraCb`…`OceanFftCb`；HLSL string literals |
| Resources | `FlycubeBuffer` / `FlycubeTexture`；`create_*` / `upload_*` / `wait_for_idle` |
| Commands | `FlycubeCommandList`（继承 `StubCommandList`）；`RecordedDraw` / `RecordedDispatch` |
| Device lifecycle | `initialize` / `shutdown` / `present` / `execute` 入口 |
| Pipelines + bind sets | `ensure_pipelines`；`*_binding_set`；`write_*_params` |
| Execute / replay | `execute_recorded` / `execute_offscreen` / `replay_draws` |
| Compute | `ensure_compute_pipelines` / `replay_compute` / FFT CB |
| No-FlyCube stub | `#else` 内 `FlycubeDevice` → `StubCommandList` |

**`create_device` 入站调用方（勿断）：** `MapViewport`、`LeftoverRecorder` / `bind_rhi_present`、`Scene3dRhiSession`、atmosphere / scene tests、`rhi_test`、CEF/WinUI/CS hosts 等。

## Target tree

```
src/render/rhi/
  rhi.h                 # 稳定公开 Facade（路径与 include 不变）
  rhi.cc                # factory + camera helpers + display name
  rhi_test.cc           # 仍挂 //src/render:rhi_test
  stub/
    stub_device.cc      # StubDevice(Backend) + present/destroy 策略
                        # 对外仍导出 create_null/gdi/gl_device（薄包装可留在本 TU）
  flycube/
    flycube_device.h    # 内部头：FlycubeDevice / Buffer / Texture / CommandList 声明
                        # （render::rhi::detail 或匿名命名空间可见的内部类型）
    device.cc           # lifecycle + create_flycube_device + #else stub 可同库或旁路
    resources.cc        # buffer/texture create/destroy/upload
    command_list.cc     # FlycubeCommandList + recorded structs
    pipelines.cc        # ensure_pipelines + binding sets + write_* CB + shader sources
    execute.cc          # execute_recorded / offscreen / replay_draws
    compute.cc          # ensure_compute + replay_compute + ocean FFT CB writers
```

**Refine 规则：**

- 公开调用方 **只** `#include "render/rhi/rhi.h"`；`flycube/*.h` **不**进 `src/` 其他层。
- 命名空间：公开 `render::rhi`；跨 TU 内部类型放 `render::rhi::detail`（第三层仅 `detail`）。
- 函数 `snake_case`；类型 PascalCase。
- `StubCommandList` **保留在 `rhi.h`**（测试与 `FlycubeCommandList` 继承依赖；本 plan 不把 recorder 私有化）。

## Module boundaries

```
create_device(Backend)
        |
        +-- kNull / kGdi / kGl  --> stub::StubDevice(tag) + StubCommandList
        |                              present: null=noop | gdi=InvalidateRect | gl=empty
        |                              destroy: null=intentional leak | gdi/gl=delete
        |
        +-- kDx12 / kVulkan     --> flycube::FlycubeDevice
                                       (#ifdef SMT_HAS_FLYCUBE real GPU)
                                       (#else StubCommandList path, backend id 仍为 DX12/Vulkan)
```

| 边界 | 允许 | 禁止 |
| --- | --- | --- |
| `rhi.h` | Facade 类型、`Stub*`、`detail::make_stub_*` | FlyCube / DXGI / Vulkan 类型 |
| `rhi.cc` | factory 转发、camera 数学 | 真 GPU、HWND present 细节 |
| `stub/` | 三标签适配 + stub 资源 | 真 GDI DC 画图、真 GL context |
| `flycube/` | 唯一真 GPU 实现 | 再造 GDI/GL 画路径 |
| `legacy/render` | 真 GDI/GL/`SmtRenderDevice` | 被本 plan 拉回 `src/render/rhi` |

### 为何不充实 GDI/GL（写进验收话术）

1. Living contract 已锁：2D+3D GPU 只走 FlyCube；GDI/GL 是 leftover HWND adapter。
2. 真画图已在 `src/legacy/render`；再在终局树填一套等于双轨引擎，违反 `render → legacy` 禁止反向依赖与终局瘦身。
3. 当前三 stub 仅差 present / destroy / `execute_count`；合并 `StubDevice` 即可，无需平行目录。

## Public API stability

| 符号 / 路径 | 稳定性 |
| --- | --- |
| `#include "render/rhi/rhi.h"` | **冻结** |
| `Backend` 含 `kGdi`/`kGl` | **冻结** |
| `Device* create_device(Backend)` | **冻结** 语义；内部可改调 `create_stub_device` |
| `Backend preferred_gpu_backend()` | **冻结**（Win DX12 / else Vulkan） |
| `backend_display_name` / camera helpers | **冻结** |
| `StubCommandList` 计数器字段 | **兼容**（scene / atmosphere / leftover 测试依赖） |
| `create_null_device` 等内部链接符号 | 可缩成 stub TU 内 `extern`；不进公开头亦可 |

## BUILD.gn impact

- 仍在 `src/render/BUILD.gn` 的 `source_set("rhi_sources")`（**无** `src/render/rhi/BUILD.gn` 除非后续有强理由；本 plan 默认不新建）。
- 更新 `sources = [...]`：删除 `null_rhi.cc` / `gdi_rhi.cc` / `gl_rhi.cc` / `flycube_rhi.cc`；加入 `stub/stub_device.cc` + `flycube/*.cc`（+ 内部头若需进 sources 列表则按仓库惯例）。
- `config("rhi_flycube")` / `smt_has_flycube` / `//third_party:flycube` deps **保持**在 `rhi_sources`（或仅挂 flycube TUs 若 GN 允许同 source_set 条件编译——优先整 set 保持现状，少折腾）。
- `test("rhi_test")` 路径改为 `rhi/rhi_test.cc`（若未动则不变）；`sibling_path` 候选目录若硬编码 `src/render/rhi` 仍有效。
- `group("rhi")` → `:render` 不变；**不**改 `dll_stem`。

## Test strategy

| 阶段 | 命令 / 期望 |
| --- | --- |
| 每次 Phase 后 | `ninja -C out rhi_test`（默认 Null，无 `SMT_RUN_FLYCUBE_GPU`）绿 |
| Stub 合并后 | Null：`execute_count`、leak destroy 行为不变；显式 `create_device(kGdi)` / `kGl`：backend id + initialize(HWND) 行为与今一致（可加最小 assert，勿要求真画） |
| FlyCube 切开后 | 同 Null 套件；可选 `set SMT_RUN_FLYCUBE_GPU=1` → present + lit solid（skip-not-red） |
| 回归邻居 | `scene_gpu_test` / `unified_draw_test` / `leftover_record_test` / atmosphere `*_pass_test`（Null 路径） |
| 公开头门禁 | 现有 `rhi_test`「`rhi.h` 无 FlyCube include」断言仍 PASS；对 `flycube/*.h` **不**做公开扫描要求 |

## Phased checklist

### Phase 0 — 冻结与文档（本文件）

- [x] 确认无既有「rhi 子目录拆分」并行 plan；本文件为权威实现清单
- [x] 锁定决策 1–4；目标树与 non-goals
- [x] （落地时）在 `2026-09-13-render-rhi-scene-design.md` Related 加一行指向本 plan（**不**改合同正文）

### Phase 1 — `stub/` 合并（行为零漂移）

**Files:** create `stub/stub_device.cc`；delete/merge `null_rhi.cc` `gdi_rhi.cc` `gl_rhi.cc`；update `rhi.cc` decls；update `BUILD.gn`.

- [x] **Step 1:** 实现 `StubDevice`：`Backend tag_`；`initialize` / `present` / `destroy_*` / `execute_count` 按标签分支（复制现有注释：Null 故意 leak）。
- [x] **Step 2:** 保留 `create_null_device` / `create_gdi_device` / `create_gl_device` 为 `new StubDevice(k*)` 薄包装（降低 `rhi.cc` 抖动）。
- [x] **Step 3:** `rhi_test` Null 路径绿；手工或加测：`kGdi`/`kGl` backend id。
- [x] **Step 4:** 删除旧三文件；`BUILD.gn` sources 更新。

**Verify:** `ninja -C out rhi_test`；`leftover_record_test`（若时间允许）。

### Phase 2 — `flycube/` 骨架（先平移后切开）

**Files:** `git mv flycube_rhi.cc` → `flycube/device.cc`（或暂名 `flycube_rhi.cc` 在子目录）；抽出 `flycube_device.h`。

- [x] **Step 1:** 子目录 + 单文件仍可编译（路径改 GN only）——**行为零变**。
- [x] **Step 2:** 抽出内部头：类声明 + 成员；实现仍可先全在 `device.cc`。
- [x] **Step 3:** `#else` stub 与 `create_flycube_device` 留在 `device.cc` 底部，避免链接重复。

**Verify:** `rhi_test` +（有 FlyCube 时）一次 GPU smoke。

### Phase 3 — 按职责切开 FlyCube（增量，可并行文件但串行合入）

建议顺序（依赖从叶到根）：

- [x] **3a `command_list.cc`：** `FlycubeCommandList` + recorded structs（最少依赖）
- [x] **3b `resources.cc`：** Buffer/Texture + device 的 create/destroy/upload 实现
- [x] **3c `pipelines.cc`：** shader 源 + `ensure_pipelines` + binding set helpers + `write_*`
- [x] **3d `compute.cc`：** compute PSO + `replay_compute` + FFT writers
- [x] **3e `execute.cc`：** `execute_recorded` / `execute_offscreen` / `replay_draws`
- [x] **3f `device.cc` 瘦身：** 仅 lifecycle + `execute` 分派 + factory

每切一刀：编译 + `rhi_test`。禁止顺手改 shader 语义或 present 时序。

### Phase 4 — 收尾

- [x] `src/render/README.md` 目录表补 `rhi/stub`、`rhi/flycube` 一行（as-built 轻改，非重写）
- [x] living RHI design File map 与本 plan 对齐
- [x] 确认无残留 `#include "render/rhi/flycube_rhi.cc"` / 旧文件名引用（扁平 `*_rhi.cc` 已删；历史 plan 正文可仍提及旧名）
- [x] 全套：`rhi_test`（本机已绿）；`scene_gpu_test` / 可选 GPU env 仍可按需复跑

## Risks / rollback

| 风险 | 缓解 | 回滚 |
| --- | --- | --- |
| Null leak 策略被「统一 delete」破坏 → CI hang | Phase 1 原样迁注释与分支 | 恢复 `null_rhi.cc` 三文件 |
| `FlycubeDevice` 跨 TU 未声明完整 → ODR/链接错 | 单一内部头；成员函数定义分区 | 合回单 `device.cc` |
| `#ifdef SMT_HAS_FLYCUBE` 边界切碎 | stub `#else` 只留 `device.cc` | 同左 |
| HWND present 时序回归 | 禁止改 `present`/`execute_recorded` 逻辑 | `git checkout` 巨石文件 |
| 误建 `rhi/gdi` 真后端 | Code review 对照 Non-goals | 删除目录，保留 stub 标签 |

回滚单位：**按 Phase**。Phase 1 与 Phase 2+ 互不依赖强绑定；Phase 3 可逐步 revert 单个 `.cc`。

## Path ownership（并行时）

**MAY edit:** `src/render/rhi/**`、`src/render/BUILD.gn`（rhi_sources / rhi_test 段）、本 plan、`src/render/README.md` 一行指针。

**MUST NOT edit（除非修编译硬依赖）:** `src/legacy/render/**` 行为、`src/render/atmosphere/**`、`src/render/scene/**` 逻辑、Views/MapViewport（仅当 include 路径误伤时）。

## Constraints（仓库规则）

- master only；无新分支；无主动 commit。
- Copyright 2026 The Mogu Authors；注释英文；对话/本 plan 中文 OK。
- C++23；`render::rhi` + `detail`；snake_case；无 Qt。
- 输出仅 `out/`；构建由用户 / `@auto-build-fix` 触发时再编。
