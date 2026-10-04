<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# RHI 通用管线：句柄 + 常量槽


> **Status: superseded** (2026-09-28 merge). Merged into `2026-09-13-render-rhi-scene-design.md` §Generic pipeline. Do not revise here except mechanical link fixes.

**Status:** active  
**Date:** 2026-09-27  
**Scope:** 替换 `render::rhi` 里按效果点名的管线与参数。FlyCube 回放改成按 `Pipeline*` 查程序。绘制结果与现在一致。  
**Supersedes:** [`2026-09-13-render-rhi-scene-design.md`](../../specs/2026-09-13-render-rhi-scene-design.md) 里 `PipelineId` / `ComputePipelineId` / `set_ocean_params` / `set_cloud_params` / `set_light_params` / `set_ocean_fft_params` / `set_solid_color` 这一段命令合同。场景双轨、FlyCube 为唯一 GPU 后端、公开头不泄漏 FlyCube 类型，仍以那份规格为准。  
**Also supersedes:** [`2026-09-20-rhi-3d-capability-p0-design.md`](2026-09-20-rhi-3d-capability-p0-design.md) 中「不改 `PipelineId::{kOcean,kCloud}`」这一条。光照与样式能力还在，只是不再通过这两个枚举进入命令列表。

## Goal

1. `rhi.h` 不再出现 ocean、cloud、light、FFT、solid 这些效果名。
2. 调用方用 `Device::create_graphics_pipeline` / `create_compute_pipeline` 拿到 `Pipeline*`，用 `set_pipeline` 和 `set_constants(slot, bytes, size)` 录制。
3. HLSL 与 constant-buffer 结构跟拥有该 pass 的模块走。多处共用的 solid / textured / lit 放在 `src/render/programs/`。
4. FlyCube 的 `GraphicsCache` / `ComputeCache` 和 `DrawKind` 分支删除。回放只做：按句柄取程序、写入常量、绑定调用方已经绑上的纹理、draw 或 dispatch。

## Non-goals

- 不改 swapchain、present、`Backend` 枚举、`create_device` 的分支。
- 不把 GDI / GL 做成第二套光栅后端。
- 不在公开头里暴露 FlyCube 的 `Pipeline`、`BindKey`、`BlendDesc`。
- 不实现动态 blend 切换。`set_blend_mode` 继续被录下来；FlyCube 使用创建管线时写死的 `BlendMode`。这是当前 `replay_draws` 的行为。
- 不新增 sampler 状态。声明了 `kSampler` 的槽得到一只 linear clamp sampler。
- 不改大气场的物理、FFT 算法或云的步进。只搬移它们进入 GPU 的方式。
- 不引入 Qt，也不把 Skia 当作这条 RHI。

## Decisions

| 主题 | 选择 |
| --- | --- |
| 管线身份 | 堆上的 `Pipeline*`。调用方保存指针，用指针相等做测试断言 |
| 效果参数 | `set_constants(slot, data, byte_size)`。slot 由该管线的 `BindingSlot` 声明 |
| 相机 | 保留 `bind_camera`。`GraphicsPipelineDesc::camera_slot >= 0` 时，draw 把 view/proj（128 字节）写入该常量槽 |
| 顶点格式 | 公开枚举三种，覆盖现有 shader：`kPosition`（float3）、`kPositionUv`（float3+float2）、`kPositionNormal`（float3+float3） |
| 深度 | 创建时声明要编译的变体（off / write / test-only）。`set_depth_mode` 在回放时挑选已编译的变体 |
| 混合 | 创建时一份 `BlendMode`。`kOpaque` 或 `kSrcAlpha` |
| 自动选择 | 删除 `PipelineId::kAuto`。有纹理的 draw 由调用方 `set_pipeline` 到 textured 程序 |
| 编译时机 | `initialize()` 成功之后才能创建 FlyCube 管线，颜色格式取交换链，深度格式取设备深度缓冲。失败返回 `nullptr` |
| Null / GDI / GL | `create_*_pipeline` 在源码指针非空时返回 `StubPipeline`，不编译 |
| 销毁 | `destroy_pipeline`。命令列表仍引用该指针时调用方不得销毁 |

## Public API（`render::rhi`）

从 `rhi.h` 删除：`PipelineId`、`ComputePipelineId`、`OceanSpectrumModel`、`OceanGpuParams`、`CloudGpuParams`、`LightParams`、`OceanFftGpuParams`，以及 `set_pipeline(PipelineId)`、`set_compute_pipeline`、`set_ocean_params`、`set_cloud_params`、`set_light_params`、`set_ocean_fft_params`、`set_solid_color`。

保留：viewport、begin/end render pass、`bind_camera`、`bind_vertex_buffer`、`bind_index_buffer`、`bind_texture`、`draw_indexed`、`set_blend_mode`、`set_depth_mode`、`bind_compute_srv`、`bind_compute_uav`、`dispatch`、`uav_barrier`、`close`。

```cpp
enum class ShaderStage : uint32_t { kVertex, kPixel, kCompute };

enum class BindingKind : uint32_t {
  kConstantBuffer,
  kSrv,
  kUav,
  kSampler,
};

enum class VertexLayout : uint32_t {
  kPosition,
  kPositionUv,
  kPositionNormal,
};

struct ShaderSource {
  const char* hlsl = nullptr;
  const char* entry = "main";
  const char* profile = "6_0";
};

// slot 是 set_constants / bind_texture / bind_compute_* 使用的索引。
// hlsl_name 是 FlyCube GetBindKey 的名字（例如 "CameraCB"）。
// 同一 slot 可出现两次（顶点与像素），set_constants 一次写进所有匹配的常量绑定。
struct BindingSlot {
  uint32_t slot = 0;
  BindingKind kind = BindingKind::kConstantBuffer;
  ShaderStage stage = ShaderStage::kVertex;
  uint32_t size_bytes = 0;
  const char* hlsl_name = nullptr;
};

struct GraphicsPipelineDesc {
  ShaderSource vertex;
  ShaderSource pixel;
  VertexLayout vertex_layout = VertexLayout::kPosition;
  const BindingSlot* bindings = nullptr;
  uint32_t binding_count = 0;
  BlendMode blend = BlendMode::kOpaque;
  bool compile_depth_off = true;
  bool compile_depth_write = false;
  bool compile_depth_test = false;
  int32_t camera_slot = 0;  // -1：bind_camera 不写常量
};

struct ComputePipelineDesc {
  ShaderSource compute;
  const BindingSlot* bindings = nullptr;
  uint32_t binding_count = 0;
};

class Pipeline {
 public:
  virtual ~Pipeline() = default;
};

// Device
virtual Pipeline* create_graphics_pipeline(const GraphicsPipelineDesc& desc);
virtual Pipeline* create_compute_pipeline(const ComputePipelineDesc& desc);
virtual void destroy_pipeline(Pipeline* pipeline);

// CommandList
virtual void set_pipeline(Pipeline* pipeline);
virtual void set_constants(uint32_t slot, const void* data, uint32_t byte_size);
```

`Device` 的默认实现：源码指针非空则 `new StubPipeline`，否则 `nullptr`。`destroy_pipeline` 默认 `delete`。FlyCube 覆盖创建函数。

`StubCommandList` 记录 `last_pipeline`、`set_pipeline_calls`、`set_constants_calls`，以及每个 slot 最后一次写入的字节（上限 256 字节，超出的调用记数但不保存正文）。删掉 `last_ocean`、`last_cloud`、`last_light`、`last_ocean_fft` 和对应的 effect 计数器。`set_solid_color_calls` 改为由测试读取 `set_constants_calls`。

## 谁拥有 shader

| 程序 | 位置 | 顶点布局 | 深度变体 | 混合 | 常量槽 |
| --- | --- | --- | --- | --- | --- |
| solid | `render/programs` | `kPosition` | off + write | opaque | 0 相机，1 颜色 float4 |
| textured | `render/programs` | `kPositionUv` | off + write | opaque | 0 相机；SRV 0；sampler |
| lit | `render/programs` | `kPositionNormal` | off + write | opaque | 0 相机，1 颜色 float4，2 光照 |
| ocean 绘制 | `render/atmosphere/ocean` | `kPositionUv` | write | opaque | 0 相机，1 ocean CB；SRV 0 高度图 |
| ocean FFT 五段 | `render/atmosphere/ocean` | — | — | — | 各段自己的 CB / SRV / UAV，结构体留在 ocean |
| cloud 绘制 | `render/atmosphere/cloud` | `kPosition` | test-only | src-alpha | 0 相机，1 cloud CB |

`src/render/programs/` 只依赖 `rhi.h`。提供 `solid_pipeline_desc()`、`textured_pipeline_desc()`、`lit_pipeline_desc()` 和颜色、光照两个 POD。命名空间 `render::programs`。

海洋频谱模型枚举（Phillips / JONSWAP）和 FFT 参数结构从 `rhi.h` 搬到 `render/atmosphere/ocean`。云参数结构搬到 `render/atmosphere/cloud`。

HLSL 字符串从 `flycube/pipeline/cache.cc` 和 `flycube/compute/cache.cc` 原样搬到上述模块。`flycube/pipeline/hlsl.cc` 仍负责把源码写成临时文件并保证 DXC 在 exe 旁边。

## 录制与回放

`draw_indexed` 快照：当前 `Pipeline*`、各常量槽字节、已绑定的纹理、`DepthMode`、顶点/索引缓冲与 draw 参数。`dispatch` 快照：当前计算 `Pipeline*`、常量槽、SRV/UAV、组数、barrier 标志。

FlyCube 回放一条路径：

1. 用 `Pipeline*` 找到已编译的程序。空指针或找不到则跳过该 draw / dispatch。
2. 图形：pass 开了深度且 draw 的 `DepthMode` 有对应变体时用该变体；否则用 depth-off。请求的变体没有编译则跳过该 draw。
3. 把快照里的常量写入对应 upload CB。`bind_camera` 的 128 字节在 draw 时写入 `camera_slot`（若 `>= 0`）。
4. SRV / UAV 使用调用方绑定的 `GpuTexture` 视图。sampler 用创建时的那一只。
5. `BindPipeline`、`BindBindingSet`、draw 或 dispatch。

`GpuScene::record` 在第一次需要时用传入的 `Device` 创建 solid、textured、lit，并持有到析构。之后的 `record` 若换了另一个 `Device`，先 `destroy_pipeline` 再按新设备创建。有 `mesh.texture` 时 `set_pipeline(textured)`；lit 种类且无纹理时 `set_pipeline(lit)` 并 `set_constants` 写入光照与颜色；其余无纹理 draw 使用 solid，颜色走 `set_constants`。`GpuScene::set_solid_color` 仍是场景侧 API，内部写入颜色槽，不再调用 RHI 的 `set_solid_color`。

`map2d` 的 `encode_draws` 增加 solid 与 textured 两个 `Pipeline*` 参数，由拥有 `Device` 的 pass 创建并传入。天空与雾通过 `programs::solid_pipeline_desc()` 创建 solid 程序，颜色用 `set_constants`。`atmosphere/detail/raster.h` 的 `RasterState` 改为保存 `Pipeline*`。

海洋 pass 与云 pass 在第一次 record 时创建自己的图形程序。`gpu_fields` 创建五段计算程序并 `set_pipeline` 到对应的那一个，FFT 参数用 `set_constants`。

## 创建失败

`create_graphics_pipeline` / `create_compute_pipeline` 在下列情况返回 `nullptr`：

- 缺少 shader 源码，或 `binding_count > 0` 但 `bindings == nullptr`。
- `camera_slot >= 0` 但没有同号的常量绑定，或该绑定的 `size_bytes < 128`。
- 同一个 slot 上的多个常量绑定 `size_bytes` 不一致。
- FlyCube 设备尚未 `initialize()` 成功、DXC 不可用、编译失败、或任一请求的深度变体创建失败。
- `hlsl_name` 为空的绑定。

`set_constants` 在 `data == nullptr` 或 `byte_size == 0` 时忽略本次写入。它不要求事先 `set_pipeline`。draw / dispatch 快照时，若某槽字节数大于当前管线该槽声明的 `size_bytes`，该槽不上传。`set_pipeline(nullptr)` 允许；随后的 draw / dispatch 在回放时跳过。

Null 与 FlyCube 使用同一条槽大小规则。Null 不编译 shader，只要求创建时源码指针非空。

## FlyCube 内部

- 新增 `flycube/pipeline/program.h`（及 `.cc`）：一个 `Pipeline` 实现，保存 FlyCube shader、layout、各深度变体 PSO、常量 buffer 与 sampler。
- `FlycubeDevice` 用一张以 `Pipeline*` 为键的表替换 `GraphicsCache` 与 `ComputeCache`。
- 删除 `DrawKind`。`Recorder` 的 draw / dispatch 记录句柄和常量字节，不再内嵌 `OceanCb`、`CloudCb`、`LightCb`。
- 相机常量就是 `CameraMatrices` 的 `view[16]` 与 `proj[16]`（128 字节），已在 `rhi.h`。flycube 不再保留 `CameraCb` / `OceanCb` / `CloudCb` / `LightCb` / `OceanFftCb`。`pipeline/types.h` 删除。

## 测试

同一改动里更新：

- `rhi_test`：创建一条带常量槽的图形程序和一条计算程序，断言 `last_pipeline` 指针相等，以及 `set_constants` 记下的字节。删除对枚举数值 `kOcean == 3`、`kCloud == 4`、`kLitSolid == 5` 的断言。
- `scene_test`：lit 网格的 `last_pipeline` 等于 `GpuScene` 持有的 lit 程序；颜色与光照通过 `set_constants` 计数和槽内容检查。
- `map2d/pass_test`、`sky_pass_test`、`ocean_pass_test`、`cloud_pass_test`：比较各自 pass 持有的 `Pipeline*`。
- 无适配器时 FlyCube GPU 冒烟仍 skip，不失败。

## 构建

新增 `source_set("programs_sources")`，`public_deps` 为 `//src/render:rhi_sources`。`//src/render:render` 依赖 `programs_sources`。不新增 DLL。

人类编译：

```bat
build.bat
```
