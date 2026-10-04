<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Weather domain boundary：天气仿真与 `render` 解耦


> **Status: superseded** (2026-09-28 merge). Merged into `2026-09-19-atmosphere-ocean-cloud-design.md` §Weather domain. Do not revise here except mechanical link fixes.

**Status:** active  
**Date:** 2026-09-27  
**Scope:** 把「天气系统」定位为 **GIS 领域/仿真/会话抽象**，锁定与 `render::atmosphere`（GPU pass）的边界、依赖方向、POD 契约与渐进迁移；**本轮不实现**新气象算法或新 Pass。  
**Related (layout, landed):** [`2026-09-27-atmosphere-subdirectory-layout-design.md`](2026-09-27-atmosphere-subdirectory-layout-design.md)  
**Related (capability):** [`2026-09-19-atmosphere-ocean-cloud-design.md`](2026-09-19-atmosphere-ocean-cloud-design.md) · sky/fog/LOD plan [`../plans/2026-09-27-sky-fog-terrain-lod.md`](../../plans/2026-09-27-sky-fog-terrain-lod.md)  
**Related (RHI / scene):** [`2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md)

## Goal

1. **原则**：天气（状态机、时间轴、气象场、预报/回放策略）是 **领域态**；`render` 只消费 **窄 POD + 纹理句柄**，不拥有 GRIB/NetCDF、业务类型或会话时钟。  
2. **边界可测**：`render::atmosphere` 单测可在 Null RHI 下无 `gis::` include 跑通；领域层可无 Device 跑通采样/时间轴。  
3. **与现有 ocean/cloud/sky/fog 协作**：Pass 顺序与 Frame 契约不变；天气域只 **投影** 参数，不改 GPU 管线语义。

## Non-goals

- 不本轮新建完整 GCM / 数值预报引擎。  
- 不把天气逻辑写进 `*Pass` / HLSL 编排。  
- 不让 `render` deps `gis`；不复活 leftover GL fog / 双轨 GDI 风场为真后端。  
- 不新建 atmosphere / weather DLL（仍随 `gis` / `render` 现有目标）。  
- 不强制立刻改名命名空间（见 §5 待拍板）。

---

## §0 为何必须与 `render` 解耦

| 理由 | 说明 |
| --- | --- |
| 生命周期不同 | 天气可离线 scrub、批处理 ingest、无窗口跑；GPU pass 绑定 Device / 视口尺寸 / present |
| 数据源不同 | GDAL NetCDF/GRIB/GeoTIFF、procedural、产品开关 vs RHI buffer / texture / pipeline |
| 测试与依赖 | 领域单测不应拉 FlyCube；Pass 单测不应拉 FieldStore / LonLatRing |
| 演进独立 | 加温湿压降水通道、预报状态机 ≠ 加 `PipelineId`；混在一起会逼 render 感知业务枚举 |
| 已有裂痕 | layout 规格已锁 `gis::atmosphere` vs `render::atmosphere`；天气是前者的 **超集语义**，不是新 GPU 车道 |

**一句话原则：** 天气拥有「现在是什么天气」；render 只负责「给定参数怎么画出来」。

---

## §1 现状锚点（2026-09-27 源码）

| 层 | 路径 / 类型 | 已有能力 |
| --- | --- | --- |
| 领域会话 | `src/gis/scene/atmosphere/` · `gis::atmosphere::Environment` | 时间轴、`AtmosphereParams`、`FieldStore`、Ocean/Cloud **System**、External ingest、procedural seed |
| 场通道 | `FieldChannel` | `kWindU/V`、`kWaveHs/Dir`、`kCloud*`、`kSeaMask`（尚无独立温/湿/压/降水枚举） |
| GPU | `src/render/atmosphere/` · `*Pass` + `AtmosphereFrame` | ocean / cloud / sky / fog；公开头 POD（`OceanDrawParams`、`SkyDrawParams`、`FogDrawParams`…） |
| 宿主胶水 | `Scene3dController` | GIS 采样 → pass params → `AtmosphereFrame`；**另有** GDI `paint_wind_arrows` 叠图 |
| UI | `ui::views` Atmosphere 面板 / `BrowserView` | 开关与字段加载入口，不应持有场真源 |

健康点：`render::atmosphere` **不** include `gis/`。  
混合点：投影胶水与 GDI 风场叠图仍堆在 Controller（产品缝可接受，但天气状态机不宜继续长在这里）。

---

## §2 边界画在哪

```
Views / CLI (ui, app)
  → 只改会话意图：开关、scrub、加载路径、质量档
        │
        ▼
gis 天气/环境域 (Environment + FieldStore + Systems)
  → 状态机 / 时间轴 / 预报·回放
  → 风温湿压云量降水场；太阳、能见度、海况语义结果
  → 与 World/DEM：观察者高度、地形遮挡高度采样
        │  project_*_draw_params()（宿主或瘦 Adapter）
        ▼
render::atmosphere (GPU only)
  → AtmosphereFrame: sky → ocean → [opaque] → cloud → fog
  → 只吃 POD + FieldTexture + CameraMatrices
  → 不吃 GRIB / FieldChannel / Environment / Views 类型
        │
        ▼
render::rhi → FlyCube / Null
```

### 归属表

| 概念 | 归属 | 禁止落入 |
| --- | --- | --- |
| 天气状态机 / 情景（晴雨转换、预警） | gis 领域 | `*Pass`、RHI Facade |
| 时间轴 / scrub / clamp to field | `Environment`（或未来 WeatherSession） | render |
| 风温湿压云量降水场 | `FieldStore` + 通道扩展 | render 头文件枚举业务通道 |
| 太阳历 / az·el / 色温策略 | gis（可先留在 `AtmosphereParams`） | Pass 内算历法 |
| 能见度 → 雾密度映射 | gis 策略 → `FogDrawParams` | FogPass 读气象文件 |
| LOD 地形遮挡 / 观察者相对地面高 | gis World/DEM + 宿主相机 | atmosphere Pass 直接 deps DemRaster |
| Pass 顺序 / load-op | `AtmosphereFrame` | Controller 再发明第二套顺序 |
| GRIB/NetCDF/GDAL | `field_ingest` | `render/**` |
| Views 面板控件 | `ui::views` | 领域层持有 HWND；render 持有面板 |

### render 只吃 / 不吃

**吃（POD + 资源句柄）：**

- 开关布尔（已由 Frame 镜像）  
- `OceanDrawParams` / cloud cover·slab 标量 / `SkyDrawParams` / `FogDrawParams`  
- 归一化太阳方向、可选 `FieldTexture`（已上传的 GPU/CPU 副本）  
- `CameraMatrices`、视口宽高、quality int（raymarch 步数预算）

**不吃：**

- `FieldStore`、`FieldChannel`、`LonLatRing`、GDAL 句柄  
- `Environment`、面板模型、产品配置路径字符串（除调试标签外）  
- 天气状态枚举的业务语义（若 Pass 需要档位，用 **匿名 int/float 档**，由领域映射）

---

## §3 与 ocean / cloud / sky / fog 的协作

锁定顺序（与 layout §4 一致）：

```
sky → ocean → opaque(GpuScene lit/DEM) → cloud → fog → present
```

| Pass | 天气域提供 | GPU 侧 |
| --- | --- | --- |
| Sky | 太阳方向 / 可选色温与穹顶半径策略 | `SkyPass` 解析着色（LUT Deferred） |
| Ocean | `OceanSystem` 采样 → `OceanDrawParams` + sea mask 纹理 | `OceanPass` FFT/Gerstner |
| Cloud | `CloudSystem` → cover / base / top / slab | `CloudPass` raymarch |
| Fog | 能见度·湿度·高度策略 → `FogDrawParams` | `FogPass` 指数雾 + 共享 depth |
| （未来粒子/雨） | 降水强度场 / 发射率 POD | 新 Pass 或 compute；仍无 gis 类型 |

DEM / 日照：太阳与时间在领域；地形 mesh/LOD 在 `gis` + `GpuScene`；雾/天空用共享 depth，**不**在 Pass 内打开第二套 DEM 读取。

---

## §4 方案对比与推荐

| 方案 | 做法 | 优点 | 缺点 |
| --- | --- | --- | --- |
| **A. 扩展 `gis::atmosphere`**（推荐） | `Environment` 升级为天气会话真源：状态机 + 通道扩展（温湿压降水）+ 明确 `project_*` API；路径保持 `src/gis/scene/atmosphere/` | 零新模块；FieldStore/时间轴已在此；与 layout/capability 一致 | 「atmosphere」名偏渲染隐喻，需文档说清 = 环境/天气域 |
| **B. 新 `gis::weather`** | 新建命名空间/目录，内嵌或拥有今日 `Environment`；atmosphere 专指海云雾绘制投影 | 产品语义清晰 | 双名并存、迁移 include/GN、易短期双写 |
| **C. `content` 会话层** | 天气会话挂 Map/文档 content，gis 只留 Field 原语 | 多文档/多视图共享会话友好 | 今日 content 并非场仿真家；过早上抬会拖垮 content |

**推荐 A（渐进）：** 先把「天气系统」文档与 API 责任钉在 `gis::atmosphere::Environment`（可加 `WeatherState` / `project_draw_params` 子类型，仍两层命名空间）；若产品坚持「Weather」品牌且模块膨胀，再 **strangler 抽出** `gis::weather`（方案 B），而不是先拆再填。

**不推荐**把天气放进 `render` 或 `content` 作为默认真源。

### 依赖方向（锁定）

```
ui / app/views  ──►  gis::atmosphere (+ World/DEM)
app/views       ──►  render::atmosphere + render::scene
gis::atmosphere ──✗──► render::*
render::atmosphere ──► render::rhi only
render::*       ──✗──► gis::* / legacy::*
```

投影胶水默认在 **app/views**（今日 Controller）；当胶水变厚时抽 `gis::atmosphere::detail` 或同目录 `draw_projection` **纯函数**（仍不 deps RHI），Controller 只调用。

---

## §5 目录映射：留下 vs 迁出

| 留下 `src/render/atmosphere/` | 迁出 / 禁止进入 render |
| --- | --- |
| `OceanPass` / `CloudPass` / `SkyPass` / `FogPass` | `Environment`、状态机、预报策略 |
| `FieldTexture`（上传与绑定） | `FieldStore`、ingest、procedural、GDAL |
| `AtmosphereFrame`（顺序与 load-op） | 太阳历、能见度业务公式（可在 gis 算完再灌 POD） |
| 各 `*DrawParams` POD | `FieldChannel`、温湿压降水语义枚举 |
| Null 单测（无 gis） | GDI 风场箭矢（今日在 Controller；长期 → 可选 Skia/Views overlay 或 GPU 箭头 Pass，**真源仍是 FieldStore**） |

`src/gis/scene/atmosphere/`（`field/` + `systems/`）= 天气/环境领域落点。  
残留扁平 `src/gis/atmosphere/` 若仍存在，应视为 **待收敛副本**，以 `scene/atmosphere` + `//src/gis/scene/atmosphere:atmosphere_sources` 为准（不在本规格强制删树）。

---

## §6 渐进迁移（strangler）

1. **冻结契约（文档）** — 本文 + layout §2：render 无 gis 类型；Controller 为唯一投影缝。  
2. **抽出投影** — 从 `record_atmosphere_*` / prepare 路径抽出 `project_ocean/cloud/sky/fog_draw_params(env, camera, …) → POD`（可测纯函数，优先放 gis 侧无 RHI）。  
3. **领域补齐** — 按需扩展 `FieldChannel`（温/湿/压/降水）与可选 `WeatherState`；UI 只调 Environment API。  
4. **削弱双轨叠图** — GDI `paint_wind_arrows` 标为 debug/HUD；正式风场可视化走 FieldTexture 或未来 GPU/Skia overlay，禁止第二套「业务场」住在 render。  
5. **（可选）改名** — 仅当方案 B 拍板：`gis::weather` 外壳 + `using`/转发一季，再删旧名。  
6. **不改** Frame 顺序与 RHI 单轨；sky/fog LUT、雨粒子等仍走 capability / upgrade 车道。

---

## §7 反模式

- 在 `OceanPass`/`FogPass` 内读文件或 `FieldStore::sample`。  
- `render` BUILD deps `//src/gis/...`。  
- Pass include `atmosphere_params.h` / `environment.h`。  
- 天气状态机写在 `Scene3dController` 私有成员里无限膨胀（面板回调直接改 GPU）。  
- GDI 风场与 GPU 海云各维护一份互不同步的「天气真相」。  
- leftover `SetFog` / 第二 Device 作为并行天气渲染。

---

## §8 成功标准

1. 本文 Status `active`→落地后可标 `accepted`；`docs/README.md` 有索引行。  
2. 依赖箭头与 POD 契约无歧义；与 layout / 2026-09-19 capability **不矛盾**。  
3. 实现阶段（另开 plan）：投影纯函数单测绿；render atmosphere 测试仍无 gis 链接。  
4. 待拍板项（§9）有明确选择后再做改名/抽模块。

## §9 待拍板（产品 / 架构）

1. **模块名**：继续 **A** `gis::atmosphere` 承载天气域，还是新建 **B** `gis::weather`？  
2. **投影代码落点**：长期放 `gis/scene/atmosphere` 纯函数，还是保留在 `app/views` Adapter？  
3. **风场 HUD**：GDI 箭矢仅 debug，还是排期 GPU/Skia 正式可视化？

---

## Out of scope for later

- 完整太阳历、多次散射天空、降水粒子 GPU。  
- clipmap DEM / 屏幕误差细分（见 sky-fog-terrain plan gap）。  
- 多文档共享 WeatherSession（方案 C）——等 content 会话模型稳定后再议。
