<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# RHI / 3D 能力 P0：样式·材质通路、默认光照、GpuScene 规模与真机 GPU

**Status:** active  
**Date:** 2026-09-20  
**Scope:** 在已有 `render::rhi` Facade + `render::scene::GpuScene` 上补齐产品级 3D 观感与可回归性；**不**引入 Shiva / two / bgfx、不透传完整 FlyCube、不做完整 PBR。  
**Origin:** Shiva / FlyCube 差距分析对话结论（P0：样式/材质、默认光照、GpuScene 规模与真机 GPU greener；不追 Shiva/two/全开 FlyCube）。对照 living RHI [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md)、样式 [`2026-09-14-sdb-style-document-design.md`](2026-09-14-sdb-style-document-design.md)、双场景 [`2026-09-19-scene3d-world-gpuscene-design.md`](2026-09-19-scene3d-world-gpuscene-design.md)。  
**Plan:** [`../plans/2026-09-20-rhi-3d-capability-p0.md`](../plans/2026-09-20-rhi-3d-capability-p0.md)  
**Sibling (do not collide):** 大气升级 [`../plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md`](../plans/2026-09-20-atmosphere-ocean-cloud-upgrade.md) 独占 ocean/cloud pass / FieldStore；本规格 **不改** `PipelineId::{kOcean,kCloud}` 与 ocean compute。

## Goal

1. **样式 / 材质通路（产品缝）** — 2D `ResolvedPaint` → per-mesh solid 已有；P0 补齐 **3D 节点**（`kTerrain` / `kModel` / `kTileset`）的 albedo / tint 通路，以及宿主/录制侧「样式文档 → GpuInstance paint」的稳定接线文档与测试。  
2. **3D 默认光照观感** — 地形与模型默认不再像 flat debug mesh：一条 **方向光 + 环境光** 的 lit solid（可选 lit textured）管线；Null 可录制不崩。  
3. **GpuScene 规模** — 实例增多时 **CPU AABB 视锥剔除**（设计已写 GPU BVH 为 later）；规模 smoke 单测。  
4. **真机 GPU greener** — 在 `SMT_RUN_FLYCUBE_GPU=1` 下，`rhi_test` / `scene_gpu_test`（及必要 showcase）覆盖 lit 路径与基本 present；无适配器仍 skip 不过红。

## Non-goals

- 不引入 Shiva ECS/SFML、two bgfx+UI、树内另一套 RHI。  
- 不开放 FlyCube RT / Mesh shading / VRS / Bindless / 多线程录制。  
- 不做完整 PBR（金属度/粗糙度图、IBL、阴影图、级联阴影）。  
- 不做标注引擎 / MapLibre 完整表达式（仍归 style 规格）。  
- 不改大气 ocean FFT / cloud raymarch 内核（并行车道）。  
- 禁止 Qt；Skia 不做 3D 光照。

## Locked decisions

| Topic | Choice |
| --- | --- |
| 档位 | GIS 工作站「可读 3D」：Lambert / Blinn 级方向光，**不是** two/SponzaPbr |
| 材质 | `GpuMesh` / 实例级 **albedo RGBA + 可选是否 lit**；2D 仍走 `ResolvedPaint`；3D 默认从 paint tint 或固有色推导 |
| 光照 API | Facade：`LightParams`（方向、颜色、环境强度）+ `CommandList::set_light_params`；默认一盏方向光 |
| 管线 | 新增窄 `PipelineId::kLitSolid`（必要时 `kLitTextured`）；**不**改写 ocean/cloud Id |
| 剔除 | P0 = **CPU** 视锥 vs 实例 AABB；GPU occupancy / BVH = Deferred（model-render §7.3） |
| 测试 | CI 默认 Null；本机可选 `SMT_RUN_FLYCUBE_GPU=1`；失败 skip，不红 |
| 路径分区 | 可写：`src/render/rhi/**`（窄 Facade）、`src/render/scene/**`（非 atmosphere）、相关 `*_test`、本 spec/plan；大气 `src/render/atmosphere/**` / FieldStore **禁改** |

## Architecture

```
gis::style::ResolvedPaint / ModelAsset tint
        │
        ▼
 GpuInstance.has_paint / GpuMesh.solid_* + lit flag
        │
        ▼
 GpuScene::record_draws
   ├─ CPU frustum cull (AABB)
   ├─ 2D: existing solid/textured
   └─ 3D terrain/model/tileset: bind kLitSolid + set_light_params
        │
        ▼
 render::rhi Facade (PipelineId, LightParams) → FlycubeDevice HLSL
```

### Approaches considered

| | Approach | Pros | Cons |
| --- | --- | --- | --- |
| **A（推荐）** | 窄 `kLitSolid` + 默认 `LightParams` + CPU cull + GPU smoke | 改动面小、与大气车道隔离、立刻改善观感 | 无阴影/PBR |
| B | 完整材质系统 + 多灯 + 法线贴图 | 接近游戏中档 | 超 P0；易撞 ocean 着色器工作 |
| C | 照搬 two materials/lights | 示例多 | 与 World/GpuScene 模型冲突；规格禁止 bgfx |

**Recommendation:** A.

## Success criteria

1. [x] `kTerrain` / `kModel`（至少其一）在 FlyCube 下用 lit 管线；改 `LightParams` 方向可见明暗变化（Null 录制 `set_light_params` 计数）。  
2. [x] 样式：3D 实例可经 `set_instance_paint`（或等价）改变 albedo；2D `style_document` → paint 回归不回退。  
3. [x] `GpuScene` 在 N 个实例（建议 ≥64 合成 AABB）下，视锥外实例不 `draw_indexed`（Null stub 计数可证）。  
4. [x] `SMT_RUN_FLYCUBE_GPU=1` 时 lit clear/draw 或 `scene_gpu_test` 路径有明确 ok 日志；无 GPU 时 skip。  
5. [x] 不新增对 FlyCube 头的产品 `#include`；不改 `kOcean`/`kCloud` 语义。

**Host paint seam:** 3D albedo caller is `GpuScene::set_instance_paint` **after** `sync_from` (map host may set from `MapLayer::style_document`); render consumes only `ResolvedPaint`.

## Out of scope for later

- GPU BVH / occupancy compute、indirect draw。  
- 多灯、阴影、IBL、法线贴图、完整 glTF PBR。  
- Per-feature 数据驱动样式表达式深化（style 规格 v2）。  
- Vulkan 日常 present 矩阵（代码已有 `kVulkan`，P0 仍以 DX12 为主）。

## Done when

- Living design + plan 勾选完成；相关 Null 单测绿；可选 GPU 门禁在本机至少跑通一次并记入 plan §测试。  
- 大气并行车道文件无无故冲突合入。
