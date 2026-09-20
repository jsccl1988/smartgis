<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plan: Atmosphere / Ocean / Cloud — 升级对齐（业务 GIS 三维 + 游戏中上外观）

**Date:** 2026-09-20  
**Status:** active  
**Spec (living, accepted):** [`../specs/2026-09-19-atmosphere-ocean-cloud-design.md`](../specs/2026-09-19-atmosphere-ocean-cloud-design.md)  
**Predecessor plan (v1 scaffold landed):** [`2026-09-19-atmosphere-ocean-cloud.md`](2026-09-19-atmosphere-ocean-cloud.md)  
**Related:** RHI / GpuScene [`../specs/2026-09-13-render-rhi-scene-design.md`](../specs/2026-09-13-render-rhi-scene-design.md)；World / GpuScene [`../specs/2026-09-19-scene3d-world-gpuscene-design.md`](../specs/2026-09-19-scene3d-world-gpuscene-design.md)

> **For agentic workers:** Implement task-by-task with checkboxes. Do **not** reopen v1 API freeze unless this plan explicitly amends the living spec. Stay on `master`. No Cesium Native, no second render engine, no GCM.

---

## 1. 对齐目标定义

### 1.1 对齐谁（选择性吸收，非整产品）

| 参照 | 吸收的能力条 | 明确不吸收 |
| --- | --- | --- |
| **ArcGIS Pro 多维 + 时间轴** | External 场（U/V、Hs、云量等）按时间维插值播放；风/流矢量或流线叠图 | 完整多维分析工具箱、Portal 分布式栅格、业务预报闭环 |
| **超图三维水面观感** | 可读的水面折射/菲涅尔、深浅色、岸线裁剪；时序属性驱动外观 | S3M 水场瓦片管线、WebGPU 引擎切换 |
| **UE / Unity HDRP 中档海面** | GPU 谱波（JONSWAP/Phillips）+ chop + Fresnel；质量档可降 Gerstner | Fluid Flux / 影视碎浪粒子、船尾全流体、焦散全链路 |

**产品档位一句话：** 场驱动可视化（业务 GIS）+ 中上实时外观（游戏中档），**不是** Earth-2 / 完整 GCM，**不是** Shiva / two / bgfx 壳。

### 1.2 成功判据（可验收，3–5 条）

1. **场驱动可见：** 至少一组 External GeoTIFF（或 GDAL 可开的规则栅格）经 `ingest_gdal_field` / 批量切片写入 `FieldStore` 后，调节 `Environment::time_sec` 能改变海面 Hs 或云量外观（Null RHI 录制不崩；FlyCube 可选门禁有像素差）。  
2. **海面中档外观：** 默认 GPU FFT（JONSWAP-lite）路径在 FlyCube 下可读：岸线 `kSeaMask` 正确、深浅色+Fresnel、chop 位移；`quality==0` / Null / `prefer_gpu_fft=false` 自动回退 CPU FFT 或 Gerstner。  
3. **时间轴语义：** 宿主或 demo 能按时间序列 scrub；`FieldStore::sample(..., time_sec)` 线性插值与文档一致；`--atmosphere-showcase` 或等价自测覆盖「双时刻差」。  
4. **风场叠图（Phase 3 完成才算总验收满分）：** `kWindU/V` 以矢量箭头 **或** 轻量 GPU 粒子之一叠在场景上，与相机/时间同步。  
5. **边界不破：** 大气仍只走 `render::rhi` Facade（`PipelineId` / `ComputePipelineId`）；产品代码不 `#include` FlyCube 头；不引入 Cesium Native / 第二引擎。

---

## 2. 现状 → 目标差距

依据 living spec + 当前树（`src/gis/atmosphere/*`、`src/render/atmosphere/*`、`Scene3dController`、`rhi.h` PipelineId）。v1 scaffold / FFT / 合成顺序已在 predecessor plan 勾完。

| 子系统 | 现状（已有） | 目标（对齐档） | 差距 |
| --- | --- | --- | --- |
| **FieldStore / 数据** | 通道齐全；`set_layer` / `upload_slice` / 时空采样；`ingest_gdal_field` 单文件单波段；Procedural 底图 | 多切片时间序列 ingest；产品向「目录/清单 → 时间轴」；驱动在则可用 GRIB/NetCDF **不自研解码** | 缺批量时间切片工具与 Views 接线；GRIB/NetCDF 未产品化验收 |
| **海面** | GPU FFT + JONSWAP-lite / Phillips、Dx/Dz chop、Fresnel、海掩膜；Gerstner/CPU 回退 | 同左 + 轻度泡沫/浪尖提示（非影视碎浪）；可选更高 FFT 分辨率档 | 泡沫/浪尖未做；RGBA8 量化噪声仍在 honest limits |
| **云与大气** | 视锥短程 raymarch、单次散射+Beer；太阳方位角 | 云量纹理真正上传调制（非整块 cover）；天空散射可选升档 | cover 多为标量；无 Hillaire/Bruneton LUT；无多次散射（spec 已拒 v1） |
| **风场与叠图** | `kWindU/V` 在场里；海面用风驱动谱 | ArcGIS 级矢量/流线或粒子叠图；雷达/卫星 External 栅格叠图路径说明 | **无**风场绘制 pass；叠图依赖现有影像层、未文档化对接 |
| **RHI / GpuScene** | `kOcean` / `kCloud` + 5 个 ocean compute；load-op / depth 合成已通 | 新叠图最多新增 **窄** PipelineId；编排仍在 `render::atmosphere` | 风场/天空若做需新 Id；禁止透传 FlyCube |
| **Views UI** | `Environment` 挂 `Scene3dController`；demo / showcase / self-test；默认关 | 可选时间滑条或键盘 scrub；开关海洋/云；加载 External 场入口（哪怕 CLI/菜单最小） | **无**产品时间轴控件；ingest 未进常规 UI |

---

## 3. 分阶段路线图

人周为 **单人等效** 量级假设；可并行时注明。

### Phase 0 — 基线冻结与验收脚手架（约 0.5–1 人周）

| | |
| --- | --- |
| **交付物** | 本 plan 生效；整理 `--atmosphere-showcase` / 单测清单为「升级基线」；补视觉金标准目录约定（见 §6）；对 living spec 仅交叉引用，不复制 API |
| **依赖** | Predecessor plan Tasks 0–12 已 landed |
| **风险** | Showcase BMP 不稳定 → 用相对阈值/ROI 差分而非像素全等 |
| **不做** | 新渲染算法；改 FlyCube 公共 API |

- [x] Phase 0.1：文档交叉链（spec ↔ 本 plan ↔ predecessor Deferred）  
- [x] Phase 0.2：列出必跑单测目标名 + `SMT_RUN_FLYCUBE_GPU=1` 可选命令写入本 plan §6  
- [x] Phase 0.3：金标准 BMP 命名与存放约定（`out/` 或测试资产路径，实现时再定，禁止提交巨型二进制除非已有惯例）

### Phase 1 — 场数据产品化（约 1–2 人周）**【P0】**

| | |
| --- | --- |
| **交付物** | 多文件/多时刻 GeoTIFF → `upload_slice` 辅助 API 或小工具；`Scene3dController` / showcase 可加载 External 并 scrub `time_sec`；单测覆盖双时刻插值外观参数变化 |
| **依赖** | Phase 0；GDAL 已链入 gis |
| **风险** | 无 GRIB/NetCDF 驱动的构建机 → **GeoTIFF 为必过门禁**；有驱动时加可选用例，失败则 skip 而非红 |
| **不做** | 自研 GRIB/NetCDF 解码器；完整业务预报客户端；浅水 `ProceduralStep` 实现 |

- [x] Phase 1.1：`ingest` 批量时间切片（路径列表 + `time_sec` 列表）→ `ingest_gdal_field_series`  
- [x] Phase 1.2：Views/showcase：`--atmosphere-fields=` → `Scene3dController::load_atmosphere_fields` → `Environment::load_external_series`  
- [x] Phase 1.3：时间 scrub（AtmospherePanel → `set_time_sec` → `scrub_time_sec` + `clamp_time_to_field`）  
- [x] Phase 1.4：单测 +（可选）GPU showcase 双时刻差分（CPU：`field_store` / `field_ingest` / `environment`；GPU 差分留给 render/showcase）

### Phase 2 — 海面 / 云外观对齐中档（约 1–2 人周）**【P0/P1】**

| | |
| --- | --- |
| **交付物** | 海面：可选简易 foam 掩膜（浪尖/折叠近似，屏或纹理调制，**非**粒子碎浪）；云：`FieldTexture` 上传 `kCloudCover` 调制 raymarch；质量档文档化 |
| **依赖** | Phase 1 场可驱动 Hs/cover（否则仍可 Procedural 验收外观） |
| **风险** | FFT 分辨率↑拖垮低端 GPU → 严格绑 `AtmosphereParams::quality`；泡沫过度花哨偏离 GIS → 默认关、demo 开 |
| **不做** | 影视级碎浪/喷溅；完整焦散；Stockham FFT 重写（仍可选优化，非本阶段必达） |

- [x] Phase 2.1：Ocean foam-lite（参数 + shader；可关）— **render 车道声称 landed；本车道未改核验**  
- [x] Phase 2.2：Cloud cover 纹理调制（替换/补充标量 cover）— **render 车道声称 landed；本车道未改核验**  
- [x] Phase 2.3：质量档矩阵（Null / quality0 / GPU FFT）回归  

### Phase 3 — 风场叠图 + UI + 大气可选升档（约 1–2 人周）**【P1】**

| | |
| --- | --- |
| **交付物** | 风场矢量 **或** 轻量粒子 pass（二选一为必达，另一个可 Deferred）；Views 最小时间轴/开关；大气散射 **可选** Hillaire 级 LUT（见 §4 锁定） |
| **依赖** | Phase 1 风场数据可用；若新 PipelineId 则扩 `render::rhi` Facade |
| **风险** | 粒子 pass 与云 alpha 合成顺序错误 → 明确插入点在云后或陆后文档化；Hillaire 工作量超支 → **降级为 Deferred**，不阻塞风场验收 |
| **不做** | Earth-2 / Omniverse；雷达体渲染引擎；完整多次散射云 |

- [x] Phase 3.1：风场可视化（矢量箭头 CPU/GPU **或** `PipelineId` 粒子）— Views：`Scene3dController` GDI 箭头叠图（粒子 Deferred）  
- [x] Phase 3.2：Views 最小时间轴 + ocean/cloud/wind 开关（AtmospherePanel + BrowserView 接线）  
- [ ] Phase 3.3：（可选）Sky / aerial LUT；若超预算则标 Deferred 并保持单次散射云  
- [ ] Phase 3.4：总验收对照 §1.2 五条  

---

## 4. 技术选型锁定

| 议题 | 决议 | 状态 |
| --- | --- | --- |
| FFT vs Gerstner | 默认 GPU FFT（JONSWAP-lite；`use_jonswap=false`→Phillips）；Null / `quality==0` / `prefer_gpu_fft=false` / 显式标志 → CPU FFT 或 Gerstner | **已锁**（living spec §3） |
| GRIB / NetCDF | **本期不自研解码**；经 GDAL `ingest_gdal_field`；GeoTIFF 为 CI 必过；有驱动则可选用例 | **已锁（收窄）**：Phase 1 产品化 GeoTIFF 时间序列；完整驱动矩阵仍 Deferred |
| 大气散射升 Hillaire | **非 Phase 0–2**；Phase 3 **可选**；默认保持太阳方向 + 云单次散射 | **推荐：Phase 3 可选 / 可 Deferred** |
| 粒子风场 | Phase 3 与矢量箭头 **二选一必达**；粒子若做则新窄 `PipelineId`，逻辑在 `render::atmosphere` | **本期可做（Phase 3）** |
| Cesium Native / 第二引擎 | 禁止 | **已锁** |
| 浅水 `ProceduralStep` | 接口可预留，本升级计划不实现 | **已锁（Deferred）** |
| 多次散射云 / 完整 GCM | 不做 | **已锁** |

**与 living spec 冲突：** 无。本 plan 是 spec「Out of scope for later」的有序落地，不修改 v1 公开 API 冻结面；若 Phase 2 foam 需扩 `OceanDrawParams` / `AtmosphereParams` 字段，属向后兼容扩展，在实现 PR 中改 spec 一小段即可。

---

## 5. 与 FlyCube / RHI 边界

| 放入 `render::rhi` Facade | 留在 `render::atmosphere` / `gis::atmosphere` | 禁止 |
| --- | --- | --- |
| 已有 `PipelineId::{kOcean,kCloud}` 与 ocean `ComputePipelineId::*` | `OceanPass` / `CloudPass` / `FieldTexture` 编排、频谱 CPU 归一化、海掩膜逻辑 | 产品 TU `#include` FlyCube / 完整 bgfx 式 API |
| Phase 3 若粒子风场：新增 **一个** `PipelineId`（如 `kWindParticles`）+ 必要 `set_*_params` | 风场采样、粒子生成、与 `FieldStore` 同步 | 在 Facade 外直接 `CreatePipeline` 旁路 |
| Phase 3 若天空 LUT：新增 compute/graphics Id + LUT 纹理绑定钩子 | LUT 填充调度、与太阳参数同步 | 把 Omniverse / Earth-2 SDK 链进 render DLL |
| `ColorLoadOp` / `DepthLoadOp` / blend / depth 模式（已有） | `GpuScene::record_draws` 前后插入点（`Scene3dController`） | 为大气新建第二 `Device` |

**原则：** Facade 只暴露枚举化管线与常量缓冲钩子；大气领域知识不泄漏进 `rhi.h` 以外的 FlyCube 细节。

---

## 6. 测试与验收

### 6.1 单测（Null / CPU，CI 必过）

| 目标 | 断言焦点 |
| --- | --- |
| `field_store_test` | 混合、clamp、**多 time_sec 线性插值**、`timed_slice_range`、`valid_mask` 时间回退 |
| `field_ingest_test` | GeoTIFF / MEM 成功路径；**批量时间切片** `ingest_gdal_field_series`；坏路径 false |
| `ocean_system_test` / `ocean_pass_test` | 参数纯函数；record 不崩；回退路径 |
| `cloud_system_test` / `cloud_pass_test` | Beer / step_count；record kLoad |
| `environment_test` / `scene3d_controller_test` | demo 开关、**scrub / clamp_time_to_field**、present 与大气共存 |

**Phase 0/1 场数据基线命令（仓库根）：**

```bat
build.bat src/gis/atmosphere:field_store_test
build.bat src/gis/atmosphere:field_ingest_test
build.bat src/gis/atmosphere:environment_test
out\field_store_test.exe
out\field_ingest_test.exe
out\environment_test.exe
```

API 缝：`ingest_gdal_field` / `ingest_gdal_field_series` → `FieldStore::upload_slice` / `sample` / `timed_slice_range`；会话级 `Environment::load_external_series` / `scrub_time_sec` / `advance_time_sec` / `clamp_time_to_field`。Views `--atmosphere-fields=` 与键盘 scrub 属宿主车道（非 `src/gis/atmosphere`）。

### 6.2 GPU 可选门禁

```bat
set SMT_RUN_FLYCUBE_GPU=1
REM 例: build.bat src/app/views:scene3d_controller_test
```

- FlyCube 机器：`--atmosphere-showcase`（ocean / cloud / demo）出 BMP，人工或 ROI 差分。  
- 无 GPU / 驱动失败：**跳过不过红**（与现有惯例一致）。

### 6.3 视觉金标准建议

| 场景 | 期望信号 |
| --- | --- |
| ocean-only | 海面非平涂；岸线内侧为陆/掩膜；有高光或色阶变化 |
| cloud-on | 天空/甲板有半透明云调制，不擦掉陆地 |
| time-A vs time-B | 同相机下 Hs 或 cover 引起可分辨差分（Phase 1+） |
| wind overlay（Phase 3） | 箭头或粒子方向与 `kWindU/V` 一致 |

存放：优先 `out/` 运行产物；若需入库小图，跟现有 testing 资产惯例，避免大图刷仓库。

---

## 7. 明确不做

- 完整海洋/大气模式求解（ROMS/NEMO/FVCOM、WRF/GFS/ECMWF 内嵌）  
- NVIDIA Earth-2 / Omniverse 数字孪生栈  
- Cesium Native 或「引入 Shiva / two / bgfx」平行引擎  
- 影视级碎浪、船尾全流体、完整焦散管线  
- Vendor 第二套大气/海洋引擎；复活 leftover `SmtScene` / `scene3d` 大气  
- Qt；Skia 做 3D 大气；为大气新建 Device  
- 完整多次散射体积云；业务级 GRIB/NetCDF 驱动矩阵（超出 GDAL 有则用）  

---

## 8. 任务勾选总表（实施时用）

### Phase 0
- [x] 0.1 文档交叉链  
- [x] 0.2 测试/门禁命令固化（§6.1）  
- [x] 0.3 金标准约定（§6.3；实现时优先 `out/` 产物）  

### Phase 1
- [x] 1.1 批量时间切片 ingest（`ingest_gdal_field_series`）  
- [x] 1.2 External 加载入口（Views `--atmosphere-fields=` → `load_external_series`）  
- [x] 1.3 时间 scrub（`scrub_time_sec` / `clamp_time_to_field`）  
- [x] 1.4 测试（field_store / field_ingest / environment）  

### Phase 2
- [x] 2.1 foam-lite（render 车道）  
- [x] 2.2 cloud cover 纹理（render 车道）  
- [x] 2.3 质量档回归  

### Phase 3
- [x] 3.1 风场可视化（Views CPU/GDI 矢量箭头；粒子 Deferred）  
- [x] 3.2 Views 最小 UI（AtmospherePanel 时间 scrub + ocean/cloud/wind）  
- [ ] 3.3（可选）天空 LUT  
- [ ] 3.4 §1.2 总验收  

---

## 9. 修订记录

| 日期 | 说明 |
| --- | --- |
| 2026-09-20 | **重写并合入** Phase 1 API（并行冲突后磁盘丢失）：`ingest_gdal_field_series`、`FieldStore::timed_slice_range`、`Environment::{load_external_series,scrub_time_sec,advance_time_sec,clamp_time_to_field,timed_field_range}`；Views 宿主从 shim 改接真 API；CLI `--atmosphere-fields=` 兼容 |
| 2026-09-20 | Phase 3 Views 独占合入：Slider + AtmospherePanel；GDI 风场箭头；当时仍用 ingest shim（已由上条替换） |
| 2026-09-20 | Phase 2 landed：foam-lite + cloud cover FieldTexture 调制 + 质量档 Null 回归 |
| 2026-09-20 | 初稿：相对 v1 landed scaffold 的升级对齐；档位=业务 GIS 三维 + 游戏中上外观 |
| 2026-09-20 | Phase 0+1 gis 车道（曾声称落地后被并行冲掉；见本表最新重写条） |
