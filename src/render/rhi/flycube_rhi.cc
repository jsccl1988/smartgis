// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/rhi.h"

#ifdef SMT_HAS_FLYCUBE
#include "ApiType/ApiType.h"
#include "BindingSet/BindingSet.h"
#include "BindingSetLayout/BindingSetLayout.h"
#include "CommandList/CommandList.h"
#include "CommandQueue/CommandQueue.h"
#include "Device/Device.h"
#include "Fence/Fence.h"
#include "Instance/BaseTypes.h"
#include "Instance/Instance.h"
#include "Pipeline/Pipeline.h"
#include "Resource/Resource.h"
#include "Shader/Shader.h"
#include "Swapchain/Swapchain.h"
#include "Utilities/Common.h"
#include "Utilities/FormatHelper.h"
#include "View/View.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <windows.h>
#endif

namespace render {
namespace rhi {
namespace {

#ifdef SMT_HAS_FLYCUBE

ApiType flycube_api(Backend backend) {
  return backend == Backend::kDx12 ? ApiType::kDX12 : ApiType::kVulkan;
}

struct CameraCb {
  float view[16];
  float proj[16];
};

struct RecordedDraw {
  Buffer* vertex = nullptr;
  uint32_t vb_offset = 0;
  uint32_t stride = 0;
  Buffer* index = nullptr;
  uint32_t ib_offset = 0;
  Texture* texture = nullptr;
  CameraMatrices camera;
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

// Owns a FlyCube upload-heap Resource; facade Buffer hides that type.
class FlycubeBuffer : public Buffer {
 public:
  FlycubeBuffer(std::shared_ptr<Resource> resource, uint32_t size)
      : resource_(std::move(resource)), size_(size) {}

  uint32_t byte_size() const override { return size_; }
  Resource* resource() const { return resource_.get(); }
  std::shared_ptr<Resource> shared() const { return resource_; }

 private:
  std::shared_ptr<Resource> resource_;
  uint32_t size_;
};

// GPU texture plus CPU bytes. FlyCube types stay out of rhi.h.
class FlycubeTexture : public Texture {
 public:
  FlycubeTexture(std::shared_ptr<Resource> resource, std::shared_ptr<View> srv,
                 uint32_t width, uint32_t height, uint32_t bytes)
      : resource_(std::move(resource)),
        srv_(std::move(srv)),
        w_(width),
        h_(height),
        bytes_(bytes) {}

  uint32_t width() const override { return w_; }
  uint32_t height() const override { return h_; }
  uint32_t byte_size() const override { return bytes_; }
  Resource* resource() const { return resource_.get(); }
  std::shared_ptr<Resource> shared() const { return resource_; }
  std::shared_ptr<View> srv() const { return srv_; }

 private:
  std::shared_ptr<Resource> resource_;
  std::shared_ptr<View> srv_;
  uint32_t w_;
  uint32_t h_;
  uint32_t bytes_;
};

// Facade list that records camera / texture / draws for a real FlyCube pass.
class FlycubeCommandList : public StubCommandList {
 public:
  RenderPassDesc last_pass;
  bool had_pass = false;
  CameraMatrices camera;
  Buffer* vb = nullptr;
  uint32_t vb_offset = 0;
  uint32_t stride = 0;
  Buffer* ib = nullptr;
  uint32_t ib_offset = 0;
  Texture* tex = nullptr;
  std::vector<RecordedDraw> draws;

  void begin_render_pass(const RenderPassDesc& desc) override {
    last_pass = desc;
    had_pass = true;
    StubCommandList::begin_render_pass(desc);
  }
  void bind_camera(const CameraMatrices& matrices) override {
    camera = matrices;
    StubCommandList::bind_camera(matrices);
  }
  void bind_vertex_buffer(Buffer* buffer, uint32_t offset,
                          uint32_t vertex_stride) override {
    vb = buffer;
    vb_offset = offset;
    stride = vertex_stride;
    StubCommandList::bind_vertex_buffer(buffer, offset, vertex_stride);
  }
  void bind_index_buffer(Buffer* buffer, uint32_t offset) override {
    ib = buffer;
    ib_offset = offset;
    StubCommandList::bind_index_buffer(buffer, offset);
  }
  void bind_texture(Texture* texture, uint32_t slot) override {
    tex = texture;
    StubCommandList::bind_texture(texture, slot);
  }
  void draw_indexed(uint32_t index_count, uint32_t instance_count,
                    uint32_t first_index, int32_t vertex_offset,
                    uint32_t first_instance) override {
    RecordedDraw draw;
    draw.vertex = vb;
    draw.vb_offset = vb_offset;
    draw.stride = stride;
    draw.index = ib;
    draw.ib_offset = ib_offset;
    draw.texture = tex;
    draw.camera = camera;
    draw.index_count = index_count;
    draw.instance_count = instance_count;
    draw.first_index = first_index;
    draw.vertex_offset = vertex_offset;
    draw.first_instance = first_instance;
    draws.push_back(draw);
    StubCommandList::draw_indexed(index_count, instance_count, first_index,
                                  vertex_offset, first_instance);
  }
};

bool file_exists(const char* path) {
  return path && path[0] && GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES;
}

bool copy_if_needed(const char* src, const char* dest) {
  if (file_exists(dest)) {
    return true;
  }
  return file_exists(src) && CopyFileA(src, dest, FALSE) != 0;
}

// FlyCube CompileShader aborts if dxcompiler.dll is not next to the exe.
bool ensure_dxc_beside_exe() {
  char exe[MAX_PATH];
  if (GetModuleFileNameA(nullptr, exe, MAX_PATH) == 0) {
    return false;
  }
  char* slash = strrchr(exe, '\\');
  if (!slash) {
    return false;
  }
  slash[1] = 0;
  const std::string dir(exe);
  const std::string dest_compiler = dir + "dxcompiler.dll";
  const std::string dest_dxil = dir + "dxil.dll";
  static const char* kCompiler[] = {
      "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\D3D\\x64\\"
      "dxcompiler.dll",
      "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.26100.0\\x64\\"
      "dxcompiler.dll",
      nullptr,
  };
  static const char* kDxil[] = {
      "C:\\Program Files (x86)\\Windows Kits\\10\\Redist\\D3D\\x64\\dxil.dll",
      "C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.26100.0\\x64\\"
      "dxil.dll",
      nullptr,
  };
  bool compiler_ok = file_exists(dest_compiler.c_str());
  for (int i = 0; !compiler_ok && kCompiler[i]; ++i) {
    compiler_ok = copy_if_needed(kCompiler[i], dest_compiler.c_str());
  }
  bool dxil_ok = file_exists(dest_dxil.c_str());
  for (int i = 0; !dxil_ok && kDxil[i]; ++i) {
    dxil_ok = copy_if_needed(kDxil[i], dest_dxil.c_str());
  }
  return compiler_ok && dxil_ok;
}

bool write_temp_hlsl(const char* name, const char* source, std::string* path) {
  char dir[MAX_PATH];
  const DWORD n = GetTempPathA(MAX_PATH, dir);
  if (n == 0 || n >= MAX_PATH || !path) {
    return false;
  }
  *path = std::string(dir) + name;
  std::ofstream out(path->c_str(), std::ios::binary | std::ios::trunc);
  if (!out) {
    return false;
  }
  out << source;
  return static_cast<bool>(out);
}

constexpr const char* kVsTextured = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

VSOut main(VSIn input)
{
    VSOut output;
    float4 world = mul(view, float4(input.pos, 1.0));
    output.pos = mul(proj, world);
    output.uv = input.uv;
    return output;
}
)";

constexpr const char* kPsTextured = R"(
Texture2D base_color_texture : register(t0);
SamplerState linear_sampler : register(s0);

struct PSIn
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD;
};

float4 main(PSIn input) : SV_TARGET
{
    return base_color_texture.Sample(linear_sampler, input.uv);
}
)";

constexpr const char* kVsSolid = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

float4 main(float3 pos : POSITION) : SV_POSITION
{
    float4 world = mul(view, float4(pos, 1.0));
    return mul(proj, world);
}
)";

constexpr const char* kPsSolid = R"(
float4 main() : SV_TARGET
{
    return float4(0.85, 0.85, 0.90, 1.0);
}
)";

// FlyCube adapter: real GPU device from local graphic_engine; facade types only
// in render/rhi/rhi.h. Clear/present uses the swapchain when an HWND is given.
class FlycubeDevice : public Device {
 public:
  explicit FlycubeDevice(Backend backend) : backend_(backend) {}

  bool initialize(const DeviceDesc& desc) override {
    width_ = desc.width;
    height_ = desc.height;
    hwnd_ = static_cast<HWND>(desc.native_window);

    instance_ = CreateInstance(flycube_api(backend_));
    if (!instance_) {
      return false;
    }

    const auto adapters = instance_->EnumerateAdapters();
    if (adapters.empty()) {
      return false;
    }

    fc_device_ = adapters.front()->CreateDevice();
    if (!fc_device_) {
      return false;
    }

    command_queue_ = fc_device_->GetCommandQueue(::CommandListType::kGraphics);
    if (!command_queue_) {
      return false;
    }

    fence_ = fc_device_->CreateFence(0);
    if (!fence_) {
      return false;
    }

    if (hwnd_ && width_ > 0 && height_ > 0) {
      NativeSurface surface = Win32Surface{GetModuleHandleW(nullptr), hwnd_};
      swapchain_ = fc_device_->CreateSwapchain(surface, width_, height_,
                                               kFrameCount, true);
      if (!swapchain_) {
        return false;
      }
      back_buffer_views_.resize(kFrameCount);
      for (uint32_t i = 0; i < kFrameCount; ++i) {
        ViewDesc view_desc = {
            .view_type = ViewType::kRenderTarget,
            .dimension = ViewDimension::kTexture2D,
        };
        back_buffer_views_[i] =
            fc_device_->CreateView(swapchain_->GetBackBuffer(i), view_desc);
        if (!back_buffer_views_[i]) {
          return false;
        }
      }
    }

    return fc_device_ != nullptr;
  }

  void shutdown() override {
    wait_for_idle();
    sampled_pipeline_.reset();
    solid_pipeline_.reset();
    sampled_layout_.reset();
    solid_layout_.reset();
    vs_textured_.reset();
    ps_textured_.reset();
    vs_solid_.reset();
    ps_solid_.reset();
    sampler_view_.reset();
    sampler_.reset();
    camera_cb_.reset();
    camera_cb_view_.reset();
    back_buffer_views_.clear();
    swapchain_.reset();
    fence_.reset();
    command_queue_.reset();
    fc_device_.reset();
    instance_.reset();
    hwnd_ = nullptr;
    fence_value_ = 0;
    gpu_sampled_draws_ = 0;
    pipelines_ready_ = false;
  }

  void present() override {
    if (!swapchain_ || !command_queue_ || !fence_) {
      return;
    }
    command_queue_->Signal(fence_, ++fence_value_);
    swapchain_->Present(fence_, fence_value_);
  }

  Backend backend() const override { return backend_; }
  uint32_t gpu_sampled_draws() const override { return gpu_sampled_draws_; }

  CommandList* create_command_list() override {
    return new FlycubeCommandList();
  }
  void destroy_command_list(CommandList* list) override { delete list; }
  bool execute(CommandList* list) override {
    auto* recorded = static_cast<StubCommandList*>(list);
    if (!recorded || !recorded->closed) {
      return false;
    }
    if (!command_queue_ || !fc_device_) {
      return true;
    }
    auto* fly_list = static_cast<FlycubeCommandList*>(list);
    if (swapchain_ && fence_ && (fly_list->had_pass || !fly_list->draws.empty())) {
      return execute_recorded(fly_list);
    }
    if (!fly_list->draws.empty()) {
      return execute_offscreen(fly_list);
    }
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (fc_list) {
      fc_list->Reset();
      fc_list->Close();
      command_queue_->ExecuteCommandLists({fc_list});
    }
    return true;
  }
  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override {
    if (byte_size == 0) {
      return nullptr;
    }
    if (!fc_device_) {
      return detail::make_stub_buffer(byte_size, usage);
    }
    ::BufferDesc desc;
    desc.size = byte_size;
    desc.usage = (usage == BufferUsage::kIndex) ? BindFlag::kIndexBuffer
                                                : BindFlag::kVertexBuffer;
    auto resource = fc_device_->CreateBuffer(MemoryType::kUpload, desc);
    if (!resource) {
      return nullptr;
    }
    return new FlycubeBuffer(std::move(resource), byte_size);
  }

  void destroy_buffer(Buffer* buffer) override { delete buffer; }

  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override {
    if (!buffer || !data) {
      return false;
    }
    if (auto* gpu = dynamic_cast<FlycubeBuffer*>(buffer)) {
      if (byte_size > gpu->byte_size() || !gpu->resource()) {
        return false;
      }
      gpu->resource()->UpdateUploadBuffer(0, data, byte_size);
      return true;
    }
    return detail::upload_stub_buffer(buffer, data, byte_size);
  }

  Texture* create_texture(const TextureDesc& desc) override {
    if (desc.width == 0 || desc.height == 0) {
      return nullptr;
    }
    if (!fc_device_) {
      return detail::make_stub_texture(desc);
    }
    ::TextureDesc td = {
        .type = TextureType::k2D,
        .format = gli::FORMAT_RGBA8_UNORM_PACK8,
        .width = desc.width,
        .height = desc.height,
        .depth_or_array_layers = 1,
        .mip_levels = 1,
        .sample_count = 1,
        .usage = BindFlag::kShaderResource | BindFlag::kCopyDest,
    };
    auto resource = fc_device_->CreateTexture(MemoryType::kDefault, td);
    if (!resource) {
      return nullptr;
    }
    ViewDesc view_desc = {
        .view_type = ViewType::kTexture,
        .dimension = ViewDimension::kTexture2D,
    };
    auto srv = fc_device_->CreateView(resource, view_desc);
    if (!srv) {
      return nullptr;
    }
    return new FlycubeTexture(std::move(resource), std::move(srv), desc.width,
                              desc.height, detail::texture_byte_size(desc));
  }

  void destroy_texture(Texture* texture) override { delete texture; }

  bool upload_texture(Texture* texture, const void* data,
                      uint32_t byte_size) override {
    if (!texture || !data) {
      return false;
    }
    auto* gpu = dynamic_cast<FlycubeTexture*>(texture);
    if (!gpu || !fc_device_ || !command_queue_ || !fence_) {
      return detail::upload_stub_texture(texture, data, byte_size);
    }
    if (byte_size > gpu->byte_size() || !gpu->resource()) {
      return false;
    }
    return upload_gpu_texture(gpu, data, byte_size);
  }

 private:
  static constexpr uint32_t kFrameCount = 2;

  void wait_for_idle() {
    if (!command_queue_ || !fence_) {
      return;
    }
    command_queue_->Signal(fence_, ++fence_value_);
    fence_->Wait(fence_value_);
  }

  bool upload_gpu_texture(FlycubeTexture* gpu, const void* data,
                          uint32_t byte_size) {
    (void)byte_size;
    const uint32_t w = gpu->width();
    const uint32_t h = gpu->height();
    size_t num_bytes = 0;
    size_t row_bytes = 0;
    size_t num_rows = 0;
    GetFormatInfo(w, h, gli::FORMAT_RGBA8_UNORM_PACK8, num_bytes, row_bytes,
                  num_rows);
    const uint64_t aligned_row =
        Align(row_bytes, fc_device_->GetTextureDataPitchAlignment());
    const uint64_t buffer_size = aligned_row * num_rows;
    ::BufferDesc upload_desc;
    upload_desc.size = buffer_size;
    upload_desc.usage = BindFlag::kCopySource;
    auto upload = fc_device_->CreateBuffer(MemoryType::kUpload, upload_desc);
    if (!upload) {
      return false;
    }
    BufferTextureCopyRegion region = {
        .buffer_offset = 0,
        .buffer_row_pitch = static_cast<uint32_t>(aligned_row),
        .texture_mip_level = 0,
        .texture_array_layer = 0,
        .texture_extent = {.width = w, .height = h, .depth = 1},
    };
    upload->UpdateUploadBufferWithTextureData(
        0, aligned_row, buffer_size, data, row_bytes, row_bytes * num_rows,
        row_bytes, static_cast<uint32_t>(num_rows), 1);
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (!fc_list) {
      return false;
    }
    fc_list->Reset();
    fc_list->ResourceBarrier({{gpu->shared(), ResourceState::kCommon,
                               ResourceState::kCopyDest}});
    fc_list->CopyBufferToTexture(upload, gpu->shared(), {region});
    fc_list->ResourceBarrier({{gpu->shared(), ResourceState::kCopyDest,
                               ResourceState::kAllShaderResource}});
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    wait_for_idle();
    return true;
  }

  bool ensure_pipelines() {
    if (pipelines_ready_) {
      return true;
    }
    if (!fc_device_ || !swapchain_ || !ensure_dxc_beside_exe()) {
      return false;
    }
    std::string vs_tex_path;
    std::string ps_tex_path;
    std::string vs_solid_path;
    std::string ps_solid_path;
    if (!write_temp_hlsl("smartgis_rhi_vs_tex.hlsl", kVsTextured,
                         &vs_tex_path) ||
        !write_temp_hlsl("smartgis_rhi_ps_tex.hlsl", kPsTextured,
                         &ps_tex_path) ||
        !write_temp_hlsl("smartgis_rhi_vs_solid.hlsl", kVsSolid,
                         &vs_solid_path) ||
        !write_temp_hlsl("smartgis_rhi_ps_solid.hlsl", kPsSolid,
                         &ps_solid_path)) {
      return false;
    }

    vs_textured_ = fc_device_->CompileShader(
        {vs_tex_path, "main", ShaderType::kVertex, "6_0"});
    ps_textured_ = fc_device_->CompileShader(
        {ps_tex_path, "main", ShaderType::kPixel, "6_0"});
    vs_solid_ = fc_device_->CompileShader(
        {vs_solid_path, "main", ShaderType::kVertex, "6_0"});
    ps_solid_ = fc_device_->CompileShader(
        {ps_solid_path, "main", ShaderType::kPixel, "6_0"});
    if (!vs_textured_ || !ps_textured_ || !vs_solid_ || !ps_solid_) {
      return false;
    }
    if (vs_textured_->GetBlob().empty() || ps_textured_->GetBlob().empty() ||
        vs_solid_->GetBlob().empty() || ps_solid_->GetBlob().empty()) {
      return false;
    }

    const uint64_t cb_align = fc_device_->GetConstantBufferOffsetAlignment();
    const uint64_t cb_size = Align(sizeof(CameraCb), cb_align ? cb_align : 256);
    camera_cb_ = fc_device_->CreateBuffer(
        MemoryType::kUpload, {.size = cb_size, .usage = BindFlag::kConstantBuffer});
    if (!camera_cb_) {
      return false;
    }
    ViewDesc cb_view = {
        .view_type = ViewType::kConstantBuffer,
        .dimension = ViewDimension::kBuffer,
    };
    camera_cb_view_ = fc_device_->CreateView(camera_cb_, cb_view);
    sampler_ = fc_device_->CreateSampler({
        .min_filter = SamplerFilter::kLinear,
        .mag_filter = SamplerFilter::kLinear,
        .mip_filter = SamplerFilter::kNearest,
    });
    ViewDesc sampler_view = {.view_type = ViewType::kSampler};
    sampler_view_ = fc_device_->CreateView(sampler_, sampler_view);
    if (!camera_cb_view_ || !sampler_view_) {
      return false;
    }

    BindKey cam_tex;
    BindKey tex_key;
    BindKey samp_key;
    BindKey cam_solid;
    try {
      cam_tex = vs_textured_->GetBindKey("CameraCB");
      tex_key = ps_textured_->GetBindKey("base_color_texture");
      samp_key = ps_textured_->GetBindKey("linear_sampler");
      cam_solid = vs_solid_->GetBindKey("CameraCB");
    } catch (const std::exception&) {
      return false;
    }
    sampled_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {cam_tex, tex_key, samp_key}});
    solid_layout_ =
        fc_device_->CreateBindingSetLayout({.bind_keys = {cam_solid}});
    if (!sampled_layout_ || !solid_layout_) {
      return false;
    }

    const gli::format color = swapchain_->GetFormat();
    GraphicsPipelineDesc sampled_desc = {
        .shaders = {vs_textured_, ps_textured_},
        .layout = sampled_layout_,
        .input = {{0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32,
                   5 * sizeof(float), 0},
                  {0, "TEXCOORD", gli::FORMAT_RG32_SFLOAT_PACK32,
                   5 * sizeof(float), 3 * sizeof(float)}},
        .color_formats = {color},
        .depth_stencil_desc = {.depth_test_enable = false,
                               .depth_write_enable = false},
    };
    GraphicsPipelineDesc solid_desc = {
        .shaders = {vs_solid_, ps_solid_},
        .layout = solid_layout_,
        .input = {{0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32,
                   3 * sizeof(float), 0}},
        .color_formats = {color},
        .depth_stencil_desc = {.depth_test_enable = false,
                               .depth_write_enable = false},
    };
    sampled_pipeline_ = fc_device_->CreateGraphicsPipeline(sampled_desc);
    solid_pipeline_ = fc_device_->CreateGraphicsPipeline(solid_desc);
    pipelines_ready_ = sampled_pipeline_ && solid_pipeline_;
    return pipelines_ready_;
  }

  void write_camera(const CameraMatrices& camera) {
    CameraCb cb;
    std::memcpy(cb.view, camera.view, sizeof(cb.view));
    std::memcpy(cb.proj, camera.proj, sizeof(cb.proj));
    camera_cb_->UpdateUploadBuffer(0, &cb, sizeof(cb));
  }

  std::shared_ptr<BindingSet> make_sampled_set(FlycubeTexture* tex) {
    BindKey cam = vs_textured_->GetBindKey("CameraCB");
    BindKey tex_key = ps_textured_->GetBindKey("base_color_texture");
    BindKey samp_key = ps_textured_->GetBindKey("linear_sampler");
    auto set = fc_device_->CreateBindingSet(sampled_layout_);
    set->WriteBindings({.bindings = {{cam, camera_cb_view_},
                                     {tex_key, tex->srv()},
                                     {samp_key, sampler_view_}}});
    return set;
  }

  std::shared_ptr<BindingSet> make_solid_set() {
    BindKey cam = vs_solid_->GetBindKey("CameraCB");
    auto set = fc_device_->CreateBindingSet(solid_layout_);
    set->WriteBindings({.bindings = {{cam, camera_cb_view_}}});
    return set;
  }

  void replay_draws(::CommandList* fc_list, FlycubeCommandList* recorded) {
    for (const RecordedDraw& draw : recorded->draws) {
      auto* vb = dynamic_cast<FlycubeBuffer*>(draw.vertex);
      auto* ib = dynamic_cast<FlycubeBuffer*>(draw.index);
      if (!vb || !ib || !vb->shared() || !ib->shared() ||
          draw.index_count == 0) {
        continue;
      }
      write_camera(draw.camera);
      auto* tex = dynamic_cast<FlycubeTexture*>(draw.texture);
      const bool sample = tex && tex->srv() && draw.stride >= 5 * sizeof(float);
      if (sample) {
        fc_list->BindPipeline(sampled_pipeline_);
        fc_list->BindBindingSet(make_sampled_set(tex));
        ++gpu_sampled_draws_;
      } else {
        fc_list->BindPipeline(solid_pipeline_);
        fc_list->BindBindingSet(make_solid_set());
      }
      fc_list->IASetVertexBuffer(0, vb->shared(), draw.vb_offset);
      fc_list->IASetIndexBuffer(ib->shared(), draw.ib_offset,
                                gli::format::FORMAT_R32_UINT_PACK32);
      fc_list->DrawIndexed(draw.index_count, draw.instance_count,
                           draw.first_index, draw.vertex_offset,
                           draw.first_instance);
    }
  }

  bool execute_recorded(FlycubeCommandList* recorded) {
    const uint32_t frame_index = swapchain_->NextImage(fence_, ++fence_value_);
    command_queue_->Wait(fence_, fence_value_);
    std::shared_ptr<Resource> back_buffer =
        swapchain_->GetBackBuffer(frame_index);
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (!fc_list || !back_buffer) {
      return false;
    }
    const bool can_draw = !recorded->draws.empty() && ensure_pipelines();
    fc_list->Reset();
    fc_list->SetViewport(0, 0, static_cast<float>(width_),
                         static_cast<float>(height_), 0.0f, 1.0f);
    fc_list->SetScissorRect(0, 0, width_, height_);
    fc_list->ResourceBarrier({{back_buffer, ResourceState::kPresent,
                               ResourceState::kRenderTarget}});
    ::RenderPassDesc pass;
    pass.render_area = {0, 0, width_, height_};
    RenderPassColorDesc color;
    color.view = back_buffer_views_[frame_index];
    color.load_op = RenderPassLoadOp::kClear;
    color.store_op = RenderPassStoreOp::kStore;
    color.clear_value = {recorded->last_pass.clear_r, recorded->last_pass.clear_g,
                         recorded->last_pass.clear_b,
                         recorded->last_pass.clear_a};
    pass.colors.push_back(std::move(color));
    fc_list->BeginRenderPass(pass);
    if (can_draw) {
      replay_draws(fc_list.get(), recorded);
    }
    fc_list->EndRenderPass();
    fc_list->ResourceBarrier({{back_buffer, ResourceState::kRenderTarget,
                               ResourceState::kPresent}});
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    return true;
  }

  bool execute_offscreen(FlycubeCommandList* recorded) {
    (void)recorded;
    return true;
  }

  Backend backend_;
  HWND hwnd_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint64_t fence_value_ = 0;
  uint32_t gpu_sampled_draws_ = 0;
  bool pipelines_ready_ = false;
  std::shared_ptr<Instance> instance_;
  std::shared_ptr<::Device> fc_device_;
  std::shared_ptr<CommandQueue> command_queue_;
  std::shared_ptr<Swapchain> swapchain_;
  std::shared_ptr<Fence> fence_;
  std::vector<std::shared_ptr<View>> back_buffer_views_;
  std::shared_ptr<Shader> vs_textured_;
  std::shared_ptr<Shader> ps_textured_;
  std::shared_ptr<Shader> vs_solid_;
  std::shared_ptr<Shader> ps_solid_;
  std::shared_ptr<BindingSetLayout> sampled_layout_;
  std::shared_ptr<BindingSetLayout> solid_layout_;
  std::shared_ptr<Pipeline> sampled_pipeline_;
  std::shared_ptr<Pipeline> solid_pipeline_;
  std::shared_ptr<Resource> camera_cb_;
  std::shared_ptr<View> camera_cb_view_;
  std::shared_ptr<Resource> sampler_;
  std::shared_ptr<View> sampler_view_;
};

#else

class FlycubeDevice : public Device {
 public:
  explicit FlycubeDevice(Backend backend) : backend_(backend) {}

  bool initialize(const DeviceDesc&) override { return false; }
  void shutdown() override {}
  void present() override {}
  Backend backend() const override { return backend_; }

  CommandList* create_command_list() override { return new StubCommandList(); }
  void destroy_command_list(CommandList* list) override { delete list; }
  bool execute(CommandList* list) override {
    auto* stub = static_cast<StubCommandList*>(list);
    return stub != nullptr && stub->closed;
  }
  Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) override {
    return detail::make_stub_buffer(byte_size, usage);
  }
  void destroy_buffer(Buffer* buffer) override {
    detail::destroy_stub_buffer(buffer);
  }
  bool upload(Buffer* buffer, const void* data, uint32_t byte_size) override {
    return detail::upload_stub_buffer(buffer, data, byte_size);
  }

 private:
  Backend backend_;
};

#endif  // SMT_HAS_FLYCUBE

}  // namespace

Device* create_flycube_device(Backend backend) {
  return new FlycubeDevice(backend);
}

}  // namespace rhi
}  // namespace render
