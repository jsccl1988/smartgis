<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Legacy Present / Paint Facade（SP2）

**Date:** 2026-09-19  
**Status:** active  
**Related:** 伞状 [`2026-09-19-legacy-deep-abstraction-umbrella-design.md`](2026-09-19-legacy-deep-abstraction-umbrella-design.md)；RHI + GpuScene [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)；leftover 拆分 [`2026-09-13-render-legacy-split-design.md`](2026-09-13-render-legacy-split-design.md)；伞状 [`2026-09-13-model-render-compute-design.md`](2026-09-13-model-render-compute-design.md)。  
**Scope:** `SmtRenderDevice::Init(HWND)` 与 `bind_rhi_present` 的 strangler；像素录制经 `render::rhi` + `GpuScene`；主 GDI / gdi_simple `RenderMap` → `leftover_record_map_frame`；`leftover_mesh` / `leftover_session` 留在 Facade 后。不含 scene3d、tool、ui、host 抽取。

## Goal

1. MFC leftover **present 仍以 HWND 为缝**：`SmtRenderDevice::Init(HWND)` 继续驱动 GDI/GL 本机绘制（BitBlt / SwapBuffers / `InvalidateRect`）。
2. **地图像素（GIS 2D / leftover 3D VB·IB）** 经 `LeftoverRecorder` → `GpuScene` / `leftover_mesh` 录到同一 `render::rhi::Device` + `CommandList`（与终局路径同 Facade）。
3. `bind_rhi_present(void* hwnd)` 是 **strangler 入口**：把 MFC 视口 HWND 接到进程级 `leftover_session()`，**不**在该 HWND 上创建 FlyCube swapchain。
4. 主 GDI（及 gdi_simple）`RenderMap` 经 `leftover_record_map_frame` 做 best-effort Null 录制；BitBlt 仍独占 HWND present。
5. 依赖单向：`legacy/render/bridge` → `render::rhi` / `render::scene::GpuScene`；**禁止** `src/render` 终局依赖 legacy。

## Non-goals

- 不重写 GDI / GL 引擎实现；仅允许 Init / present / RenderMap 录制缝的薄接线。
- 不改 scene3d / tool / ui / app（其他 SP 分区）。
- 不把 FlyCube 类型泄漏进 bridge 公开头。
- 不加 Qt；不复活 D3D9。
- 不破坏 leftover DLL `dll_stem` / `bind_rhi_present` / `smt_leftover_session` 导出名。

## Path ownership

| 可改 | 禁改 |
| --- | --- |
| `src/legacy/render/bridge/**` | `src/legacy/tool/**`、`src/tool/**` |
| `src/legacy/render/{gdi,gdi_simple,gl}/**`（Init / RenderMap / present 适配） | `src/legacy/app/**`、`src/legacy/ui/**` |
| `src/render/rhi/**`（Facade 扩展，本轮未改） | `src/legacy/render/scene3d/**`（SP4） |
| 本 spec / plan、必要 README / `build.bat` te 列表 | 终局 → legacy 新依赖 |

## 背景（为何需要本规格）

历史 Task 7 曾要求 `bind_rhi_present`「优先 preferred GPU，失败再 GDI」。在 **GDI MDI 子窗口 HWND** 上同时挂 `SmtGdiRenderDevice` 与 DX12/Vulkan，实测会在视图拉起阶段触发 `STATUS_FATAL_APP_EXIT`（`0xC000041D`）。因此 `bind_rhi_present` 曾被改为空操作。空操作又切断了 Init → leftover session 的显式接线。SP2 把「安全 present 策略」写成活规格并恢复最小 strangler；随后把主 GDI paint 接到同一 Null 录制路径。

## Decisions（locked）

| Topic | Choice |
| --- | --- |
| Present 所有权 | **双轨**：MFC GDI/GL **独占**其 `Init` HWND 的 present；Views / `gpu` 在 **独立 HWND** 上走 `preferred_gpu_backend()` |
| `bind_rhi_present` | 调用 `leftover_session().bind_present_hwnd(hwnd)`：记录 HWND + 预绑定 **owned Null**（或已有 Device 则只更新 HWND）；**禁止**在此创建 FlyCube |
| `LeftoverRecorder::ensure_device` | 无 Device 时一律 `Backend::kNull`；**不再**因 `native_window_` 非空自动 `preferred_gpu_backend()` |
| GDI `RenderMap` | **先** GDI `RenderLayer` / BitBlt，**再** `leftover_record_map_frame`（Null begin/record_map/finish）；失败不阻断 present。先录再画会使 map buffer 全白 |
| FlyCube 入口 | 仅 `attach(device)`（MapViewport / gpu 自建 Device）或测试显式 `create_device(kDx12)`；与 MFC GDI HWND 解耦 |
| 像素路径 | `record_map` / `record_world` → `GpuScene`；leftover VB/IB → `leftover_mesh`；均在同一 CommandList |
| ABI | 保留导出名 `bind_rhi_present` / `smt_leftover_session`；snake_case 与现桥一致 |

## Architecture

```
MFC view / SmtGdiRenderDevice::Init(HWND)
        |
        +-- GDI paint (BitBlt / InvalidateRect)     ← HWND present owner
        |
        v
bind_rhi_present(hwnd)
        |
        v
leftover_session()  LeftoverRecorder
        |-- native_window_   (bookkeeping / adapters)
        |-- Device (Null by default; FlyCube only via attach)
        |-- CommandList
        +-- GpuScene + leftover_mesh

SmtGdi*::RenderMap(map)
        |
        +-- existing GDI RenderLayer → map buffer     ← must run first
        |
        v
leftover_record_map_frame(hwnd, w, h, map)  → begin / record_map / finish
        |
        +-- (Null GpuScene only; does not own HWND present)

Views MapViewport / gpu process
        |
        v
create_device(preferred_gpu_backend())  + own HWND
        |
        v
LeftoverRecorder::attach(device)   OR direct GpuScene::record
```

## API surface（bridge）

```cpp
namespace render {

// Strangler: Init(HWND) → leftover_session. Safe on GDI-shared HWND.
RENDER_EXPORT_API void bind_rhi_present(void* native_window);

namespace scene {

class LeftoverRecorder {
 public:
  void set_native_window(void* native_window);
  void* native_window() const;

  // Bind HWND and ensure a recording Device without FlyCube.
  bool bind_present_hwnd(void* native_window);

  bool attach(render::rhi::Device* device);  // FlyCube / test Device
  // ... begin / record_* / finish / release ...
};

LeftoverRecorder& leftover_session();

// Best-effort Null map-frame recording for leftover GDI paint.
bool leftover_record_map_frame(void* native_window, uint32_t width,
                               uint32_t height, const gis::SmtMap* map);

}  // namespace scene
}  // namespace render
```

## Dependency

- 允许：`legacy/render/bridge|gdi|gdi_simple|gl` → `render::rhi` / `render::scene::GpuScene` / `gis::*`
- 禁止：`src/render/**` → `src/legacy/**`

## Testing

| Test | Assert |
| --- | --- |
| `leftover_session_test` | `bind_rhi_present(hwnd)` 后 `native_window()` 等于入参；Device `backend() == kNull`；`leftover_record_map_frame` 空图成功、零尺寸/null map 拒绝 |
| `leftover_record_test` / `leftover_mesh_test` | 仍仅走 Null；HWND  alone 不创建 preferred GPU |
| `gdi_map_paint_test` | Init + paint 仍绿（含 present strangler） |

## Success criteria

1. 本规格为活文档（`Status: active`），与 RHI / legacy-split / 伞状决策一致且纠正「同 HWND 上 FlyCube」错误路径。
2. `bind_rhi_present` 非空操作：接到 `leftover_session`，测试覆盖 HWND + Null backend。
3. `ensure_device` 不再自动创建 preferred GPU。
4. 主 GDI / gdi_simple `RenderMap` 经 `leftover_record_map_frame` 录制；无 FlyCube 同 HWND。
5. 无 `src/render` → legacy 新依赖；无 Qt；注释英文；版权 2026。

## Out of scope / later

- `Backend::kGdi` 作为 bind 默认 Device（InvalidateRect Facade）——可选增强，非锁定默认。
- GL `Draw*` 中途接通 leftover_session（避免 mid-frame 与他 HWND Device 打架；SP4/GL 另轮）。
- scene3d dem / stereo（SP4；以本 Present 缝为闸）。

## Done when

- [x] Status `active`；版权 2026；伞状 Related 齐全
- [x] Locked decisions：双轨 present、`bind_rhi_present` → Null、`ensure_device` 无 preferred_gpu、GDI RenderMap helper
- [x] Path ownership / Dependency / ABI / Non-goals 写清
- [x] `bind_rhi_present` + `leftover_record_map_frame` 可测且已绿
- [x] 主 GDI + gdi_simple 接通；`gdi_map_paint_test` 绿
- [x] SP4 前置「Present 缝可用」已满足（Init strangler + Null 录制 Facade）
- [ ] （later）`Backend::kGdi` bind 默认 / GL mid-frame 录制 — 不阻塞 SP4 启动
