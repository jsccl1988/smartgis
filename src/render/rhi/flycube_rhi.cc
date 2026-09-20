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

struct ColorCb {
  float r;
  float g;
  float b;
  float a;
};

struct OceanCb {
  float deep[4];
  float shallow[4];
  float fresnel_bias;
  float fresnel_power;
  float height_scale;
  float cam_x;
  float cam_y;
  float cam_z;
  float disp_scale;
  float pad1;
};

struct CloudCb {
  float sun_x;
  float sun_y;
  float sun_z;
  float cover;
  float base_m;
  float top_m;
  float extinction;
  float steps;
  float cam_x;
  float cam_y;
  float cam_z;
  float pad0;
};

struct OceanFftCb {
  uint32_t size;
  uint32_t log2_size;
  uint32_t stage;
  uint32_t direction;
  float time_sec;
  float wind_speed;
  float wind_dir_rad;
  float amp_scale;
  float patch_size;
  float height_scale;
  float disp_scale;
  float chop;
  uint32_t spectrum_model;
  uint32_t encode_channel;
  float gamma;
  float pad;
};

struct RecordedDraw {
  Buffer* vertex = nullptr;
  uint32_t vb_offset = 0;
  uint32_t stride = 0;
  Buffer* index = nullptr;
  uint32_t ib_offset = 0;
  Texture* texture = nullptr;
  CameraMatrices camera;
  float solid_r = 0.85f;
  float solid_g = 0.85f;
  float solid_b = 0.90f;
  float solid_a = 1.f;
  PipelineId pipeline = PipelineId::kAuto;
  BlendMode blend = BlendMode::kOpaque;
  DepthMode depth = DepthMode::kDisabled;
  OceanGpuParams ocean;
  CloudGpuParams cloud;
  uint32_t index_count = 0;
  uint32_t instance_count = 1;
  uint32_t first_index = 0;
  int32_t vertex_offset = 0;
  uint32_t first_instance = 0;
};

// One facade begin/end_render_pass maps to one FlyCube Begin/EndRenderPass.
struct PassSegment {
  RenderPassDesc desc;
  std::vector<RecordedDraw> draws;
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
                 std::shared_ptr<View> uav, uint32_t width, uint32_t height,
                 uint32_t bytes, TextureFormat format)
      : resource_(std::move(resource)),
        srv_(std::move(srv)),
        uav_(std::move(uav)),
        w_(width),
        h_(height),
        bytes_(bytes),
        format_(format) {}

  uint32_t width() const override { return w_; }
  uint32_t height() const override { return h_; }
  uint32_t byte_size() const override { return bytes_; }
  TextureFormat format() const { return format_; }
  Resource* resource() const { return resource_.get(); }
  std::shared_ptr<Resource> shared() const { return resource_; }
  std::shared_ptr<View> srv() const { return srv_; }
  std::shared_ptr<View> uav() const { return uav_; }

 private:
  std::shared_ptr<Resource> resource_;
  std::shared_ptr<View> srv_;
  std::shared_ptr<View> uav_;
  uint32_t w_;
  uint32_t h_;
  uint32_t bytes_;
  TextureFormat format_;
};

// One recorded compute dispatch (ocean FFT stages).
struct RecordedDispatch {
  ComputePipelineId pipeline = ComputePipelineId::kNone;
  OceanFftGpuParams params;
  Texture* srv0 = nullptr;
  Texture* uav0 = nullptr;
  Texture* uav1 = nullptr;
  uint32_t groups_x = 1;
  uint32_t groups_y = 1;
  uint32_t groups_z = 1;
  bool barrier_after = false;
};

// Facade list that records per-pass draws for sequential FlyCube passes.
class FlycubeCommandList : public StubCommandList {
 public:
  std::vector<PassSegment> passes;
  std::vector<RecordedDispatch> dispatches;
  CameraMatrices camera;
  Buffer* vb = nullptr;
  uint32_t vb_offset = 0;
  uint32_t stride = 0;
  Buffer* ib = nullptr;
  uint32_t ib_offset = 0;
  Texture* tex = nullptr;
  Texture* compute_srv0_ = nullptr;
  Texture* compute_uav0_ = nullptr;
  Texture* compute_uav1_ = nullptr;
  float solid_r = 0.85f;
  float solid_g = 0.85f;
  float solid_b = 0.90f;
  float solid_a = 1.f;
  PipelineId pipeline = PipelineId::kAuto;
  ComputePipelineId compute_pipeline = ComputePipelineId::kNone;
  BlendMode blend = BlendMode::kOpaque;
  DepthMode depth = DepthMode::kDisabled;
  OceanGpuParams ocean;
  CloudGpuParams cloud;
  OceanFftGpuParams ocean_fft;

  bool had_pass() const { return !passes.empty(); }
  bool has_draws() const {
    for (const PassSegment& p : passes) {
      if (!p.draws.empty()) {
        return true;
      }
    }
    return false;
  }
  bool has_dispatches() const { return !dispatches.empty(); }

  void begin_render_pass(const RenderPassDesc& desc) override {
    PassSegment seg;
    seg.desc = desc;
    passes.push_back(std::move(seg));
    StubCommandList::begin_render_pass(desc);
  }
  void end_render_pass() override { StubCommandList::end_render_pass(); }
  void bind_camera(const CameraMatrices& matrices) override {
    camera = matrices;
    StubCommandList::bind_camera(matrices);
  }
  void set_solid_color(float r, float g, float b, float a) override {
    solid_r = r;
    solid_g = g;
    solid_b = b;
    solid_a = a;
    StubCommandList::set_solid_color(r, g, b, a);
  }
  void set_pipeline(PipelineId id) override {
    pipeline = id;
    StubCommandList::set_pipeline(id);
  }
  void set_blend_mode(BlendMode mode) override {
    blend = mode;
    StubCommandList::set_blend_mode(mode);
  }
  void set_depth_mode(DepthMode mode) override {
    depth = mode;
    StubCommandList::set_depth_mode(mode);
  }
  void set_ocean_params(const OceanGpuParams& params) override {
    ocean = params;
    StubCommandList::set_ocean_params(params);
  }
  void set_cloud_params(const CloudGpuParams& params) override {
    cloud = params;
    StubCommandList::set_cloud_params(params);
  }
  void set_compute_pipeline(ComputePipelineId id) override {
    compute_pipeline = id;
    StubCommandList::set_compute_pipeline(id);
  }
  void set_ocean_fft_params(const OceanFftGpuParams& params) override {
    ocean_fft = params;
    StubCommandList::set_ocean_fft_params(params);
  }
  void bind_compute_srv(Texture* texture, uint32_t slot) override {
    if (slot == 0) {
      compute_srv0_ = texture;
    }
    StubCommandList::bind_compute_srv(texture, slot);
  }
  void bind_compute_uav(Texture* texture, uint32_t slot) override {
    if (slot == 0) {
      compute_uav0_ = texture;
    } else if (slot == 1) {
      compute_uav1_ = texture;
    }
    StubCommandList::bind_compute_uav(texture, slot);
  }
  void dispatch(uint32_t group_count_x, uint32_t group_count_y,
                uint32_t group_count_z) override {
    RecordedDispatch d;
    d.pipeline = compute_pipeline;
    d.params = ocean_fft;
    d.srv0 = compute_srv0_;
    d.uav0 = compute_uav0_;
    d.uav1 = compute_uav1_;
    d.groups_x = group_count_x;
    d.groups_y = group_count_y;
    d.groups_z = group_count_z;
    dispatches.push_back(d);
    StubCommandList::dispatch(group_count_x, group_count_y, group_count_z);
  }
  void uav_barrier() override {
    if (!dispatches.empty()) {
      dispatches.back().barrier_after = true;
    }
    StubCommandList::uav_barrier();
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
    if (passes.empty()) {
      PassSegment seg;
      seg.desc.width = 0;
      seg.desc.height = 0;
      passes.push_back(std::move(seg));
    }
    RecordedDraw draw;
    draw.vertex = vb;
    draw.vb_offset = vb_offset;
    draw.stride = stride;
    draw.index = ib;
    draw.ib_offset = ib_offset;
    draw.texture = tex;
    draw.camera = camera;
    draw.solid_r = solid_r;
    draw.solid_g = solid_g;
    draw.solid_b = solid_b;
    draw.solid_a = solid_a;
    draw.pipeline = pipeline;
    draw.blend = blend;
    draw.depth = depth;
    draw.ocean = ocean;
    draw.cloud = cloud;
    draw.index_count = index_count;
    draw.instance_count = instance_count;
    draw.first_index = first_index;
    draw.vertex_offset = vertex_offset;
    draw.first_instance = first_instance;
    passes.back().draws.push_back(draw);
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
cbuffer ColorCB : register(b1)
{
    float4 color;
};

float4 main() : SV_TARGET
{
    return color;
}
)";

constexpr const char* kVsOcean = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};
cbuffer OceanCB : register(b1)
{
    float4 deep;
    float4 shallow;
    float fresnel_bias;
    float fresnel_power;
    float height_scale;
    float cam_x;
    float cam_y;
    float cam_z;
    float disp_scale;
    float pad1;
};
Texture2D height_map : register(t0);
SamplerState linear_sampler : register(s0);

struct VSIn
{
    float3 pos : POSITION;
    float2 uv : TEXCOORD;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

VSOut main(VSIn input)
{
    float4 enc = height_map.SampleLevel(linear_sampler, input.uv, 0);
    float height = (enc.r - 0.5) * 2.0 * height_scale;
    float dx = (enc.g - 0.5) * 2.0 * disp_scale;
    float dz = (enc.b - 0.5) * 2.0 * disp_scale;
    float3 world = float3(input.pos.x + dx, height, input.pos.z + dz);
    VSOut output;
    output.world = world;
    output.uv = input.uv;
    float4 view_pos = mul(view, float4(world, 1.0));
    output.pos = mul(proj, view_pos);
    return output;
}
)";

constexpr const char* kPsOcean = R"(
cbuffer OceanCB : register(b1)
{
    float4 deep;
    float4 shallow;
    float fresnel_bias;
    float fresnel_power;
    float height_scale;
    float cam_x;
    float cam_y;
    float cam_z;
    float disp_scale;
    float pad1;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
    float2 uv : TEXCOORD1;
};

float4 main(PSIn input) : SV_TARGET
{
    float3 dx = ddx(input.world);
    float3 dy = ddy(input.world);
    float3 n = normalize(cross(dx, dy));
    float3 cam = float3(cam_x, cam_y, cam_z);
    float3 V = normalize(cam - input.world);
    float ndotv = saturate(dot(n, V));
    float f = fresnel_bias + (1.0 - fresnel_bias) * pow(1.0 - ndotv, fresnel_power);
    float3 rgb = lerp(shallow.rgb, deep.rgb, f);
    return float4(rgb, lerp(shallow.a, deep.a, f));
}
)";

constexpr const char* kVsCloud = R"(
cbuffer CameraCB : register(b0)
{
    float4x4 view;
    float4x4 proj;
};

struct VSOut
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
};

VSOut main(float3 pos : POSITION)
{
    VSOut output;
    output.world = pos;
    float4 view_pos = mul(view, float4(pos, 1.0));
    output.pos = mul(proj, view_pos);
    return output;
}
)";

constexpr const char* kPsCloud = R"(
cbuffer CloudCB : register(b1)
{
    float sun_x;
    float sun_y;
    float sun_z;
    float cover;
    float base_m;
    float top_m;
    float extinction;
    float steps;
    float cam_x;
    float cam_y;
    float cam_z;
    float pad0;
};

struct PSIn
{
    float4 pos : SV_POSITION;
    float3 world : TEXCOORD0;
};

float hash31(float3 p)
{
    p = frac(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return frac((p.x + p.y) * p.z);
}

float density_at(float3 p, float base_y, float top_y, float c)
{
    float lo = min(base_y, top_y);
    float hi = max(base_y, top_y);
    if (p.y < lo || p.y > hi || c <= 0.0)
        return 0.0;
    float t = (p.y - lo) / max(hi - lo, 1e-3);
    float falloff = 4.0 * t * (1.0 - t);
    float n = hash31(p * 0.35);
    return c * falloff * (0.35 + 0.65 * n);
}

float4 main(PSIn input) : SV_TARGET
{
    float3 cam = float3(cam_x, cam_y, cam_z);
    float3 dir = normalize(input.world - cam);
    float lo = min(base_m, top_m);
    float hi = max(base_m, top_m);
    // Slab intersection along Y.
    float t0, t1;
    if (abs(dir.y) < 1e-5)
    {
        if (cam.y < lo || cam.y > hi)
            discard;
        t0 = 0.0;
        t1 = 40.0;
    }
    else
    {
        float ta = (lo - cam.y) / dir.y;
        float tb = (hi - cam.y) / dir.y;
        t0 = max(min(ta, tb), 0.0);
        t1 = max(ta, tb);
        if (t1 <= t0)
            discard;
    }
    int nsteps = max(4, (int)steps);
    float ds = (t1 - t0) / (float)nsteps;
    float T = 1.0;
    float3 L = float3(0.0, 0.0, 0.0);
    float3 sun = normalize(float3(sun_x, sun_y, sun_z));
    for (int i = 0; i < 64; ++i)
    {
        if (i >= nsteps)
            break;
        float t = t0 + (float(i) + 0.5) * ds;
        float3 p = cam + dir * t;
        float dens = density_at(p, base_m, top_m, cover);
        if (dens <= 0.0)
            continue;
        float sigma = dens * max(extinction, 0.0);
        float step_T = exp(-sigma * ds);
        float shadow = exp(-density_at(p + sun * ds * 4.0, base_m, top_m, cover) *
                           max(extinction, 0.0) * ds * 4.0);
        L += T * (1.0 - step_T) * shadow * 0.08;
        T *= step_T;
        if (T < 0.02)
            break;
    }
    float alpha = saturate(1.0 - T);
    if (alpha < 0.01)
        discard;
    float3 rgb = saturate(L + float3(0.75, 0.78, 0.85) * alpha);
    return float4(rgb, alpha);
}
)";

// Ocean GPU FFT compute shaders (JONSWAP/Phillips + radix-2 + displace + encode).
constexpr const char* kCsOceanSpectrum = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

static const float kPi = 3.14159265358979323846;
static const float kG = 9.81;

float hash01(uint i, uint j, uint salt)
{
    uint h = i * 73856093u ^ j * 19349663u ^ salt * 83492791u;
    h ^= h >> 16;
    h *= 0x7feb352du;
    h ^= h >> 15;
    h *= 0x846ca68bu;
    h ^= h >> 16;
    return float(h & 0x00ffffffu) / float(0x01000000u);
}

float phillips(float kx, float ky)
{
    float k2 = kx * kx + ky * ky;
    if (k2 < 1.0e-8)
        return 0.0;
    float k_len = sqrt(k2);
    float L = max((wind_speed * wind_speed) / kG, 1.0e-3);
    float kwx = cos(wind_dir_rad);
    float kwy = sin(wind_dir_rad);
    float k_dot_w = max((kx * kwx + ky * kwy) / k_len, 0.0);
    float damp = exp(-1.0 / (k2 * L * L));
    float suppress = exp(-k2 * L * L * 0.001);
    return damp * suppress * (k_dot_w * k_dot_w) / (k2 * k2);
}

// Directional JONSWAP-lite on the k-grid (shape only; amp_scale sets Hs).
float jonswap(float kx, float ky)
{
    float k2 = kx * kx + ky * ky;
    if (k2 < 1.0e-8)
        return 0.0;
    float k_len = sqrt(k2);
    float omega = sqrt(kG * k_len);
    float U = max(wind_speed, 1.0);
    float omega_p = 0.877 * kG / U;
    float alpha = 0.0081;
    float sigma = (omega <= omega_p) ? 0.07 : 0.09;
    float om_r = omega_p / max(omega, 1.0e-4);
    float pm = alpha * kG * kG * pow(omega, -5.0) * exp(-1.25 * pow(om_r, 4.0));
    float r = (omega - omega_p) / max(sigma * omega_p, 1.0e-4);
    float peak = pow(max(gamma, 1.0), exp(-0.5 * r * r));
    float S_omega = pm * peak;
    float domega_dk = 0.5 * sqrt(kG / k_len);
    float S_k = S_omega * domega_dk / k_len;
    float kwx = cos(wind_dir_rad);
    float kwy = sin(wind_dir_rad);
    float k_dot_w = max((kx * kwx + ky * kwy) / k_len, 0.0);
    return S_k * (k_dot_w * k_dot_w);
}

RWTexture2D<float2> spectrum_uav : register(u0);
RWTexture2D<float2> spectrum_seed_uav : register(u1);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    int ii = (int)id.x;
    int jj = (int)id.y;
    int half_n = (int)(size >> 1);
    if (ii >= half_n)
        ii -= (int)size;
    if (jj >= half_n)
        jj -= (int)size;
    if (ii == 0 && jj == 0)
    {
        spectrum_uav[id] = float2(0.0, 0.0);
        spectrum_seed_uav[id] = float2(0.0, 0.0);
        return;
    }
    float dk = 2.0 * kPi / max(patch_size, 1.0);
    float kx = float(ii) * dk;
    float ky = float(jj) * dk;
    float p = (spectrum_model == 0u) ? phillips(kx, ky) : jonswap(kx, ky);
    if (p <= 0.0)
    {
        spectrum_uav[id] = float2(0.0, 0.0);
        spectrum_seed_uav[id] = float2(0.0, 0.0);
        return;
    }
    float a = amp_scale * sqrt(p * 0.5);
    float phase0 = hash01(id.x, id.y, 1u) * 2.0 * kPi;
    float k_len = sqrt(kx * kx + ky * ky);
    float omega = sqrt(kG * k_len);
    float phase = phase0 + omega * time_sec;
    float2 h = float2(a * cos(phase), a * sin(phase));
    spectrum_uav[id] = h;
    spectrum_seed_uav[id] = h;
}
)";

constexpr const char* kCsOceanBitReverse = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

uint bit_reverse(uint x, uint bits)
{
    return reversebits(x) >> (32u - bits);
}

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    uint i = (direction == 0u) ? id.x : id.y;
    uint o = (direction == 0u) ? id.y : id.x;
    uint r = bit_reverse(i, log2_size);
    float2 v = (direction == 0u) ? src_tex[uint2(i, o)] : src_tex[uint2(o, i)];
    if (direction == 0u)
        dst_tex[uint2(r, o)] = v;
    else
        dst_tex[uint2(o, r)] = v;
}
)";

constexpr const char* kCsOceanButterfly = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

static const float kPi = 3.14159265358979323846;

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

float2 mul_complex(float2 a, float2 b)
{
    return float2(a.x * b.x - a.y * b.y, a.x * b.y + a.y * b.x);
}

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    // Inverse radix-2 Cooley-Tukey (after bit-reverse), ping-pong.
    if (id.x >= size || id.y >= size)
        return;
    uint i = (direction == 0u) ? id.x : id.y;
    uint o = (direction == 0u) ? id.y : id.x;
    uint len = 2u << stage;
    uint half = len >> 1;
    uint base = (i / len) * len;
    uint j = i % half;
    uint u = base + j;
    uint v = base + j + half;
    bool upper = (i % len) >= half;

    float2 a = (direction == 0u) ? src_tex[uint2(u, o)] : src_tex[uint2(o, u)];
    float2 b = (direction == 0u) ? src_tex[uint2(v, o)] : src_tex[uint2(o, v)];
    float ang = +2.0 * kPi * float(j) / float(len);
    float2 w = float2(cos(ang), sin(ang));
    float2 t = mul_complex(w, b);
    float2 result = upper ? (a - t) : (a + t);
    // Match CPU inverse scale at the final stage of each 1D transform.
    if (stage + 1u == log2_size)
        result *= (1.0 / float(size));
    if (direction == 0u)
        dst_tex[uint2(i, o)] = result;
    else
        dst_tex[uint2(o, i)] = result;
}
)";

// Tessendorf: ĥ_d = -i * chop * (k_axis / |k|) * ĥ
constexpr const char* kCsOceanDisplacementSpectrum = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

static const float kPi = 3.14159265358979323846;

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float2> dst_tex : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    int ii = (int)id.x;
    int jj = (int)id.y;
    int half_n = (int)(size >> 1);
    if (ii >= half_n)
        ii -= (int)size;
    if (jj >= half_n)
        jj -= (int)size;
    if (ii == 0 && jj == 0)
    {
        dst_tex[id] = float2(0.0, 0.0);
        return;
    }
    float dk = 2.0 * kPi / max(patch_size, 1.0);
    float kx = float(ii) * dk;
    float ky = float(jj) * dk;
    float k_len = sqrt(kx * kx + ky * ky);
    float k_hat = (direction == 0u) ? (kx / k_len) : (ky / k_len);
    float2 h = src_tex[id];
    // -i * (a+ib) = b - i a
    float2 disp = float2(h.y, -h.x) * (chop * k_hat);
    dst_tex[id] = disp;
}
)";

constexpr const char* kCsOceanHeightEncode = R"(
cbuffer OceanFftCB : register(b0)
{
    uint size;
    uint log2_size;
    uint stage;
    uint direction;
    float time_sec;
    float wind_speed;
    float wind_dir_rad;
    float amp_scale;
    float patch_size;
    float height_scale;
    float disp_scale;
    float chop;
    uint spectrum_model;
    uint encode_channel;
    float gamma;
    float pad;
};

Texture2D<float2> src_tex : register(t0);
RWTexture2D<float4> height_uav : register(u0);

[numthreads(8, 8, 1)]
void main(uint2 id : SV_DispatchThreadID)
{
    if (id.x >= size || id.y >= size)
        return;
    float v = src_tex[id].x;
    float scale = (encode_channel == 0u) ? height_scale : disp_scale;
    scale = max(scale, 1.0e-3);
    float enc = saturate(0.5 + 0.5 * (v / scale));
    float4 cur = height_uav[id];
    if (encode_channel == 0u)
        height_uav[id] = float4(enc, 0.5, 0.5, 1.0);
    else if (encode_channel == 1u)
    {
        cur.g = enc;
        height_uav[id] = cur;
    }
    else
    {
        cur.b = enc;
        height_uav[id] = cur;
    }
}
)";

// FlyCube adapter: real GPU device from local graphic_engine; facade types only
// in render/rhi/rhi.h. Clear/present uses the swapchain when an HWND is given.
class FlycubeDevice : public Device {
 public:
  explicit FlycubeDevice(Backend backend) : backend_(backend) {}

  bool initialize(const DeviceDesc& desc) override {
    // WM_SIZE / Scene3dRhiSession::resize re-call initialize on a live device.
    // DXGI allows only one flip-model swapchain per HWND; leaving the old
    // chain bound makes CreateSwapChainForHwnd fail and FlyCube CHECK abort().
    if (instance_ || fc_device_ || swapchain_) {
      shutdown();
    }
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
                                               kFrameCount, false);
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
    for (uint32_t i = 0; i < kFrameCount; ++i) {
      graphics_lists_[i].reset();
      graphics_fence_values_[i] = 0;
    }
    solid_set_.reset();
    ocean_set_.reset();
    cloud_set_.reset();
    sampled_pipeline_.reset();
    sampled_depth_pipeline_.reset();
    solid_pipeline_.reset();
    solid_depth_pipeline_.reset();
    ocean_pipeline_.reset();
    cloud_pipeline_.reset();
    sampled_layout_.reset();
    solid_layout_.reset();
    ocean_layout_.reset();
    cloud_layout_.reset();
    vs_textured_.reset();
    ps_textured_.reset();
    vs_solid_.reset();
    ps_solid_.reset();
    vs_ocean_.reset();
    ps_ocean_.reset();
    vs_cloud_.reset();
    ps_cloud_.reset();
    sampler_view_.reset();
    sampler_.reset();
    camera_cb_.reset();
    camera_cb_view_.reset();
    color_cb_.reset();
    color_cb_view_.reset();
    ocean_cb_.reset();
    ocean_cb_view_.reset();
    cloud_cb_.reset();
    cloud_cb_view_.reset();
    depth_view_.reset();
    depth_texture_.reset();
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
    depth_w_ = 0;
    depth_h_ = 0;
  }

  void present() override {
    if (!swapchain_ || !command_queue_ || !fence_) {
      return;
    }
    // CPU-wait for GPU work before DXGI Present. FlyCube's Swapchain::Present
    // only inserts a queue Wait then calls Present immediately.
    command_queue_->Signal(fence_, ++fence_value_);
    fence_->Wait(fence_value_);
    swapchain_->Present(fence_, fence_value_);
  }

  Backend backend() const override { return backend_; }
  uint32_t gpu_sampled_draws() const override { return gpu_sampled_draws_; }
  bool supports_compute() const override { return fc_device_ != nullptr; }

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
    if (swapchain_ && fence_ &&
        (fly_list->had_pass() || fly_list->has_draws() ||
         fly_list->has_dispatches())) {
      return execute_recorded(fly_list);
    }
    if (fly_list->has_draws() || fly_list->has_dispatches()) {
      return execute_offscreen(fly_list);
    }
    // Empty close: still serialize before Reset — FlyCube may pool allocators.
    wait_for_idle();
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (fc_list) {
      fc_list->Reset();
      fc_list->Close();
      command_queue_->ExecuteCommandLists({fc_list});
      wait_for_idle();
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
    gli::format fmt = gli::FORMAT_RGBA8_UNORM_PACK8;
    if (desc.format == TextureFormat::kRg32Float) {
      fmt = gli::FORMAT_RG32_SFLOAT_PACK32;
    } else if (desc.format == TextureFormat::kRgba32Float) {
      fmt = gli::FORMAT_RGBA32_SFLOAT_PACK32;
    }
    uint32_t usage = 0;
    if (has_texture_usage(desc.usage, TextureUsage::kSampled)) {
      usage |= BindFlag::kShaderResource;
    }
    if (has_texture_usage(desc.usage, TextureUsage::kStorage)) {
      usage |= BindFlag::kUnorderedAccess;
    }
    if (has_texture_usage(desc.usage, TextureUsage::kCopyDest)) {
      usage |= BindFlag::kCopyDest;
    }
    if (usage == 0) {
      usage = BindFlag::kShaderResource | BindFlag::kCopyDest;
    }
    ::TextureDesc td = {
        .type = TextureType::k2D,
        .format = fmt,
        .width = desc.width,
        .height = desc.height,
        .depth_or_array_layers = 1,
        .mip_levels = 1,
        .sample_count = 1,
        .usage = usage,
    };
    auto resource = fc_device_->CreateTexture(MemoryType::kDefault, td);
    if (!resource) {
      return nullptr;
    }
    std::shared_ptr<View> srv;
    if (usage & BindFlag::kShaderResource) {
      ViewDesc view_desc = {
          .view_type = ViewType::kTexture,
          .dimension = ViewDimension::kTexture2D,
      };
      srv = fc_device_->CreateView(resource, view_desc);
    }
    std::shared_ptr<View> uav;
    if (usage & BindFlag::kUnorderedAccess) {
      ViewDesc uav_desc = {
          .view_type = ViewType::kRWTexture,
          .dimension = ViewDimension::kTexture2D,
      };
      uav = fc_device_->CreateView(resource, uav_desc);
    }
    if ((usage & BindFlag::kShaderResource) && !srv) {
      return nullptr;
    }
    if ((usage & BindFlag::kUnorderedAccess) && !uav) {
      return nullptr;
    }
    return new FlycubeTexture(std::move(resource), std::move(srv),
                              std::move(uav), desc.width, desc.height,
                              detail::texture_byte_size(desc), desc.format);
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
    wait_for_idle();
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
    std::string vs_tex_path, ps_tex_path, vs_solid_path, ps_solid_path;
    std::string vs_ocean_path, ps_ocean_path, vs_cloud_path, ps_cloud_path;
    if (!write_temp_hlsl("smartgis_rhi_vs_tex.hlsl", kVsTextured,
                         &vs_tex_path) ||
        !write_temp_hlsl("smartgis_rhi_ps_tex.hlsl", kPsTextured,
                         &ps_tex_path) ||
        !write_temp_hlsl("smartgis_rhi_vs_solid.hlsl", kVsSolid,
                         &vs_solid_path) ||
        !write_temp_hlsl("smartgis_rhi_ps_solid.hlsl", kPsSolid,
                         &ps_solid_path) ||
        !write_temp_hlsl("smartgis_rhi_vs_ocean.hlsl", kVsOcean,
                         &vs_ocean_path) ||
        !write_temp_hlsl("smartgis_rhi_ps_ocean.hlsl", kPsOcean,
                         &ps_ocean_path) ||
        !write_temp_hlsl("smartgis_rhi_vs_cloud.hlsl", kVsCloud,
                         &vs_cloud_path) ||
        !write_temp_hlsl("smartgis_rhi_ps_cloud.hlsl", kPsCloud,
                         &ps_cloud_path)) {
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
    vs_ocean_ = fc_device_->CompileShader(
        {vs_ocean_path, "main", ShaderType::kVertex, "6_0"});
    ps_ocean_ = fc_device_->CompileShader(
        {ps_ocean_path, "main", ShaderType::kPixel, "6_0"});
    vs_cloud_ = fc_device_->CompileShader(
        {vs_cloud_path, "main", ShaderType::kVertex, "6_0"});
    ps_cloud_ = fc_device_->CompileShader(
        {ps_cloud_path, "main", ShaderType::kPixel, "6_0"});
    if (!vs_textured_ || !ps_textured_ || !vs_solid_ || !ps_solid_ ||
        !vs_ocean_ || !ps_ocean_ || !vs_cloud_ || !ps_cloud_) {
      return false;
    }
    if (vs_textured_->GetBlob().empty() || ps_textured_->GetBlob().empty() ||
        vs_solid_->GetBlob().empty() || ps_solid_->GetBlob().empty() ||
        vs_ocean_->GetBlob().empty() || ps_ocean_->GetBlob().empty() ||
        vs_cloud_->GetBlob().empty() || ps_cloud_->GetBlob().empty()) {
      return false;
    }

    const uint64_t cb_align = fc_device_->GetConstantBufferOffsetAlignment();
    const uint64_t align = cb_align ? cb_align : 256;
    camera_cb_ = fc_device_->CreateBuffer(
        MemoryType::kUpload,
        {.size = Align(sizeof(CameraCb), align),
         .usage = BindFlag::kConstantBuffer});
    color_cb_ = fc_device_->CreateBuffer(
        MemoryType::kUpload,
        {.size = Align(sizeof(ColorCb), align),
         .usage = BindFlag::kConstantBuffer});
    ocean_cb_ = fc_device_->CreateBuffer(
        MemoryType::kUpload,
        {.size = Align(sizeof(OceanCb), align),
         .usage = BindFlag::kConstantBuffer});
    cloud_cb_ = fc_device_->CreateBuffer(
        MemoryType::kUpload,
        {.size = Align(sizeof(CloudCb), align),
         .usage = BindFlag::kConstantBuffer});
    if (!camera_cb_ || !color_cb_ || !ocean_cb_ || !cloud_cb_) {
      return false;
    }
    ViewDesc cb_view = {
        .view_type = ViewType::kConstantBuffer,
        .dimension = ViewDimension::kBuffer,
    };
    camera_cb_view_ = fc_device_->CreateView(camera_cb_, cb_view);
    color_cb_view_ = fc_device_->CreateView(color_cb_, cb_view);
    ocean_cb_view_ = fc_device_->CreateView(ocean_cb_, cb_view);
    cloud_cb_view_ = fc_device_->CreateView(cloud_cb_, cb_view);
    solid_set_.reset();
    ocean_set_.reset();
    cloud_set_.reset();
    sampler_ = fc_device_->CreateSampler({
        .min_filter = SamplerFilter::kLinear,
        .mag_filter = SamplerFilter::kLinear,
        .mip_filter = SamplerFilter::kNearest,
    });
    ViewDesc sampler_view = {.view_type = ViewType::kSampler};
    sampler_view_ = fc_device_->CreateView(sampler_, sampler_view);
    if (!camera_cb_view_ || !color_cb_view_ || !ocean_cb_view_ ||
        !cloud_cb_view_ || !sampler_view_) {
      return false;
    }

    BindKey cam_tex, tex_key, samp_key, cam_solid, color_solid;
    BindKey cam_ocean, ocean_cb_key, ocean_cb_ps_key, ocean_tex, ocean_samp;
    BindKey cam_cloud, cloud_cb_key;
    try {
      cam_tex = vs_textured_->GetBindKey("CameraCB");
      tex_key = ps_textured_->GetBindKey("base_color_texture");
      samp_key = ps_textured_->GetBindKey("linear_sampler");
      cam_solid = vs_solid_->GetBindKey("CameraCB");
      color_solid = ps_solid_->GetBindKey("ColorCB");
      cam_ocean = vs_ocean_->GetBindKey("CameraCB");
      ocean_cb_key = vs_ocean_->GetBindKey("OceanCB");
      ocean_cb_ps_key = ps_ocean_->GetBindKey("OceanCB");
      ocean_tex = vs_ocean_->GetBindKey("height_map");
      ocean_samp = vs_ocean_->GetBindKey("linear_sampler");
      cam_cloud = vs_cloud_->GetBindKey("CameraCB");
      cloud_cb_key = ps_cloud_->GetBindKey("CloudCB");
    } catch (const std::exception&) {
      return false;
    }
    sampled_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {cam_tex, tex_key, samp_key}});
    solid_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {cam_solid, color_solid}});
    // OceanCB is used by both VS (displace) and PS (fresnel). D3D12 root
    // signatures are stage-scoped via BindKey::shader_type, so both keys
    // must be listed or CreatePipelineState fails (abort in debug FlyCube).
    ocean_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {cam_ocean, ocean_cb_key, ocean_cb_ps_key, ocean_tex,
                       ocean_samp}});
    cloud_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {cam_cloud, cloud_cb_key}});
    if (!sampled_layout_ || !solid_layout_ || !ocean_layout_ || !cloud_layout_) {
      return false;
    }

    if (!ensure_depth_buffer(width_, height_)) {
      return false;
    }

    const gli::format color = swapchain_->GetFormat();
    const gli::format depth_fmt = depth_texture_->GetFormat();
    DepthStencilDesc depth_off = {.depth_test_enable = false,
                                  .depth_write_enable = false};
    DepthStencilDesc depth_write = {.depth_test_enable = true,
                                    .depth_write_enable = true,
                                    .depth_func = ComparisonFunc::kLess};
    DepthStencilDesc depth_test = {.depth_test_enable = true,
                                   .depth_write_enable = false,
                                   .depth_func = ComparisonFunc::kLess};
    BlendDesc opaque_blend;
    BlendDesc alpha_blend = {
        .blend_enable = true,
        .src_color_blend_factor = BlendFactor::kSrcAlpha,
        .dst_color_blend_factor = BlendFactor::kOneMinusSrcAlpha,
        .color_blend_op = BlendOp::kAdd,
        .src_alpha_blend_factor = BlendFactor::kOne,
        .dst_alpha_blend_factor = BlendFactor::kOneMinusSrcAlpha,
        .alpha_blend_op = BlendOp::kAdd,
    };

    auto uv_input = std::vector<InputLayoutDesc>{
        {0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32, 5 * sizeof(float), 0},
        {0, "TEXCOORD", gli::FORMAT_RG32_SFLOAT_PACK32, 5 * sizeof(float),
         3 * sizeof(float)}};
    auto pos_input = std::vector<InputLayoutDesc>{
        {0, "POSITION", gli::FORMAT_RGB32_SFLOAT_PACK32, 3 * sizeof(float), 0}};

    GraphicsPipelineDesc sampled_desc = {
        .shaders = {vs_textured_, ps_textured_},
        .layout = sampled_layout_,
        .input = uv_input,
        .color_formats = {color},
        .depth_stencil_format = gli::format::FORMAT_UNDEFINED,
        .depth_stencil_desc = depth_off,
        .blend_desc = opaque_blend,
    };
    GraphicsPipelineDesc sampled_depth_desc = sampled_desc;
    sampled_depth_desc.depth_stencil_format = depth_fmt;
    sampled_depth_desc.depth_stencil_desc = depth_write;
    GraphicsPipelineDesc solid_desc = {
        .shaders = {vs_solid_, ps_solid_},
        .layout = solid_layout_,
        .input = pos_input,
        .color_formats = {color},
        .depth_stencil_format = gli::format::FORMAT_UNDEFINED,
        .depth_stencil_desc = depth_off,
        .blend_desc = opaque_blend,
    };
    GraphicsPipelineDesc solid_depth_desc = solid_desc;
    solid_depth_desc.depth_stencil_format = depth_fmt;
    solid_depth_desc.depth_stencil_desc = depth_write;

    GraphicsPipelineDesc ocean_desc = {
        .shaders = {vs_ocean_, ps_ocean_},
        .layout = ocean_layout_,
        .input = uv_input,
        .color_formats = {color},
        .depth_stencil_format = depth_fmt,
        .depth_stencil_desc = depth_write,
        .blend_desc = opaque_blend,
    };
    GraphicsPipelineDesc cloud_desc = {
        .shaders = {vs_cloud_, ps_cloud_},
        .layout = cloud_layout_,
        .input = pos_input,
        .color_formats = {color},
        .depth_stencil_format = depth_fmt,
        .depth_stencil_desc = depth_test,
        .blend_desc = alpha_blend,
    };

    sampled_pipeline_ = fc_device_->CreateGraphicsPipeline(sampled_desc);
    sampled_depth_pipeline_ =
        fc_device_->CreateGraphicsPipeline(sampled_depth_desc);
    solid_pipeline_ = fc_device_->CreateGraphicsPipeline(solid_desc);
    solid_depth_pipeline_ = fc_device_->CreateGraphicsPipeline(solid_depth_desc);
    ocean_pipeline_ = fc_device_->CreateGraphicsPipeline(ocean_desc);
    cloud_pipeline_ = fc_device_->CreateGraphicsPipeline(cloud_desc);
    pipelines_ready_ = sampled_pipeline_ && sampled_depth_pipeline_ &&
                       solid_pipeline_ && solid_depth_pipeline_ &&
                       ocean_pipeline_ && cloud_pipeline_;
    return pipelines_ready_;
  }

  bool ensure_depth_buffer(uint32_t w, uint32_t h) {
    if (!fc_device_ || w == 0 || h == 0) {
      return false;
    }
    if (depth_texture_ && depth_view_ && depth_w_ == w && depth_h_ == h) {
      return true;
    }
    depth_view_.reset();
    depth_texture_.reset();
    ::TextureDesc td = {
        .type = TextureType::k2D,
        .format = gli::format::FORMAT_D32_SFLOAT_PACK32,
        .width = w,
        .height = h,
        .depth_or_array_layers = 1,
        .mip_levels = 1,
        .sample_count = 1,
        .usage = BindFlag::kDepthStencil,
    };
    depth_texture_ = fc_device_->CreateTexture(MemoryType::kDefault, td);
    if (!depth_texture_) {
      return false;
    }
    ViewDesc vd = {
        .view_type = ViewType::kDepthStencil,
        .dimension = ViewDimension::kTexture2D,
    };
    depth_view_ = fc_device_->CreateView(depth_texture_, vd);
    depth_w_ = w;
    depth_h_ = h;
    return depth_view_ != nullptr;
  }

  void write_camera(const CameraMatrices& camera) {
    CameraCb cb;
    std::memcpy(cb.view, camera.view, sizeof(cb.view));
    std::memcpy(cb.proj, camera.proj, sizeof(cb.proj));
    camera_cb_->UpdateUploadBuffer(0, &cb, sizeof(cb));
  }

  void write_solid_color(float r, float g, float b, float a) {
    ColorCb cb{r, g, b, a};
    color_cb_->UpdateUploadBuffer(0, &cb, sizeof(cb));
  }

  void write_ocean_params(const OceanGpuParams& p) {
    OceanCb cb{};
    cb.deep[0] = p.deep_r;
    cb.deep[1] = p.deep_g;
    cb.deep[2] = p.deep_b;
    cb.deep[3] = p.deep_a;
    cb.shallow[0] = p.shallow_r;
    cb.shallow[1] = p.shallow_g;
    cb.shallow[2] = p.shallow_b;
    cb.shallow[3] = p.shallow_a;
    cb.fresnel_bias = p.fresnel_bias;
    cb.fresnel_power = p.fresnel_power;
    cb.height_scale = p.height_scale;
    cb.cam_x = p.cam_x;
    cb.cam_y = p.cam_y;
    cb.cam_z = p.cam_z;
    cb.disp_scale = p.disp_scale;
    cb.pad1 = p.pad1;
    ocean_cb_->UpdateUploadBuffer(0, &cb, sizeof(cb));
  }

  void write_cloud_params(const CloudGpuParams& p) {
    CloudCb cb{};
    cb.sun_x = p.sun_x;
    cb.sun_y = p.sun_y;
    cb.sun_z = p.sun_z;
    cb.cover = p.cover;
    cb.base_m = p.base_m;
    cb.top_m = p.top_m;
    cb.extinction = p.extinction;
    cb.steps = p.steps;
    cb.cam_x = p.cam_x;
    cb.cam_y = p.cam_y;
    cb.cam_z = p.cam_z;
    cloud_cb_->UpdateUploadBuffer(0, &cb, sizeof(cb));
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

  std::shared_ptr<BindingSet> solid_binding_set() {
    if (solid_set_) {
      return solid_set_;
    }
    BindKey cam = vs_solid_->GetBindKey("CameraCB");
    BindKey color = ps_solid_->GetBindKey("ColorCB");
    solid_set_ = fc_device_->CreateBindingSet(solid_layout_);
    solid_set_->WriteBindings(
        {.bindings = {{cam, camera_cb_view_}, {color, color_cb_view_}}});
    return solid_set_;
  }

  std::shared_ptr<BindingSet> make_ocean_set(FlycubeTexture* height) {
    BindKey cam = vs_ocean_->GetBindKey("CameraCB");
    BindKey ocean_vs = vs_ocean_->GetBindKey("OceanCB");
    BindKey ocean_ps = ps_ocean_->GetBindKey("OceanCB");
    BindKey tex = vs_ocean_->GetBindKey("height_map");
    BindKey samp = vs_ocean_->GetBindKey("linear_sampler");
    auto set = fc_device_->CreateBindingSet(ocean_layout_);
    set->WriteBindings({.bindings = {{cam, camera_cb_view_},
                                     {ocean_vs, ocean_cb_view_},
                                     {ocean_ps, ocean_cb_view_},
                                     {tex, height->srv()},
                                     {samp, sampler_view_}}});
    return set;
  }

  std::shared_ptr<BindingSet> cloud_binding_set() {
    if (cloud_set_) {
      return cloud_set_;
    }
    BindKey cam = vs_cloud_->GetBindKey("CameraCB");
    BindKey cloud = ps_cloud_->GetBindKey("CloudCB");
    cloud_set_ = fc_device_->CreateBindingSet(cloud_layout_);
    cloud_set_->WriteBindings(
        {.bindings = {{cam, camera_cb_view_}, {cloud, cloud_cb_view_}}});
    return cloud_set_;
  }

  void replay_draws(::CommandList* fc_list, const PassSegment& segment,
                    bool pass_has_depth) {
    for (const RecordedDraw& draw : segment.draws) {
      auto* vb = dynamic_cast<FlycubeBuffer*>(draw.vertex);
      auto* ib = dynamic_cast<FlycubeBuffer*>(draw.index);
      if (!vb || !ib || !vb->shared() || !ib->shared() ||
          draw.index_count == 0) {
        continue;
      }
      write_camera(draw.camera);
      auto* tex = dynamic_cast<FlycubeTexture*>(draw.texture);

      PipelineId id = draw.pipeline;
      if (id == PipelineId::kAuto) {
        if (tex && tex->srv() && draw.stride >= 5 * sizeof(float)) {
          id = PipelineId::kTextured;
        } else {
          id = PipelineId::kSolid;
        }
      }

      if (id == PipelineId::kOcean && tex && tex->srv() && ocean_pipeline_) {
        write_ocean_params(draw.ocean);
        fc_list->BindPipeline(ocean_pipeline_);
        fc_list->BindBindingSet(make_ocean_set(tex));
      } else if (id == PipelineId::kCloud && cloud_pipeline_) {
        write_cloud_params(draw.cloud);
        fc_list->BindPipeline(cloud_pipeline_);
        fc_list->BindBindingSet(cloud_binding_set());
      } else if (id == PipelineId::kTextured && tex && tex->srv()) {
        if (pass_has_depth && sampled_depth_pipeline_) {
          fc_list->BindPipeline(sampled_depth_pipeline_);
        } else {
          fc_list->BindPipeline(sampled_pipeline_);
        }
        fc_list->BindBindingSet(make_sampled_set(tex));
        ++gpu_sampled_draws_;
      } else {
        write_solid_color(draw.solid_r, draw.solid_g, draw.solid_b,
                          draw.solid_a);
        if (pass_has_depth &&
            (draw.depth == DepthMode::kWrite ||
             draw.depth == DepthMode::kTestOnly) &&
            solid_depth_pipeline_) {
          fc_list->BindPipeline(solid_depth_pipeline_);
        } else {
          fc_list->BindPipeline(solid_pipeline_);
        }
        fc_list->BindBindingSet(solid_binding_set());
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
    // Per-frame command list: wait for this slot's prior Execute before Reset
    // (avoids D3D12 COMMAND_ALLOCATOR_SYNC when FlyCube pools allocators).
    if (graphics_fence_values_[frame_index] != 0) {
      fence_->Wait(graphics_fence_values_[frame_index]);
    }
    if (!graphics_lists_[frame_index]) {
      graphics_lists_[frame_index] =
          fc_device_->CreateCommandList(::CommandListType::kGraphics);
    }
    auto fc_list = graphics_lists_[frame_index];
    if (!fc_list || !back_buffer) {
      return false;
    }
    const bool want_draw = recorded->has_draws();
    const bool can_draw = want_draw && ensure_pipelines();
    bool need_depth = false;
    for (const PassSegment& seg : recorded->passes) {
      if (seg.desc.enable_depth) {
        need_depth = true;
        break;
      }
    }
    if (need_depth && !ensure_depth_buffer(width_, height_)) {
      return false;
    }

    fc_list->Reset();
    fc_list->SetViewport(0, 0, static_cast<float>(width_),
                         static_cast<float>(height_), 0.0f, 1.0f);
    fc_list->SetScissorRect(0, 0, width_, height_);
    fc_list->ResourceBarrier({{back_buffer, ResourceState::kPresent,
                               ResourceState::kRenderTarget}});

    // Ocean GPU FFT (and other compute) runs before any color pass.
    if (recorded->has_dispatches()) {
      if (!ensure_compute_pipelines()) {
        return false;
      }
      replay_compute(fc_list.get(), recorded->dispatches);
    }

    // Sequential GPU passes: each facade begin/end becomes Begin/EndRenderPass.
    std::vector<PassSegment> segments = recorded->passes;
    if (segments.empty() && want_draw) {
      PassSegment one;
      one.desc.load_op = ColorLoadOp::kClear;
      one.desc.clear_r = recorded->last_pass.clear_r;
      one.desc.clear_g = recorded->last_pass.clear_g;
      one.desc.clear_b = recorded->last_pass.clear_b;
      one.desc.clear_a = recorded->last_pass.clear_a;
      segments.push_back(std::move(one));
    }

    bool depth_cleared = false;
    for (size_t i = 0; i < segments.size(); ++i) {
      const PassSegment& seg = segments[i];
      ::RenderPassDesc pass;
      pass.render_area = {0, 0, width_, height_};
      RenderPassColorDesc color;
      color.view = back_buffer_views_[frame_index];
      color.store_op = RenderPassStoreOp::kStore;
      if (seg.desc.load_op == ColorLoadOp::kClear) {
        color.load_op = RenderPassLoadOp::kClear;
        color.clear_value = {seg.desc.clear_r, seg.desc.clear_g,
                             seg.desc.clear_b, seg.desc.clear_a};
      } else {
        color.load_op = RenderPassLoadOp::kLoad;
      }
      pass.colors.push_back(std::move(color));

      const bool use_depth = seg.desc.enable_depth && depth_view_;
      if (use_depth) {
        pass.depth_stencil_view = depth_view_;
        pass.depth.store_op = RenderPassStoreOp::kStore;
        if (seg.desc.depth_load_op == DepthLoadOp::kClear || !depth_cleared) {
          pass.depth.load_op = RenderPassLoadOp::kClear;
          pass.depth.clear_value = seg.desc.depth_clear;
          depth_cleared = true;
        } else {
          pass.depth.load_op = RenderPassLoadOp::kLoad;
        }
      }

      fc_list->BeginRenderPass(pass);
      if (can_draw) {
        replay_draws(fc_list.get(), seg, use_depth);
      }
      fc_list->EndRenderPass();
    }

    fc_list->ResourceBarrier({{back_buffer, ResourceState::kRenderTarget,
                               ResourceState::kPresent}});
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    command_queue_->Signal(fence_, ++fence_value_);
    graphics_fence_values_[frame_index] = fence_value_;
    return true;
  }

  bool execute_offscreen(FlycubeCommandList* recorded) {
    if (!recorded || !fc_device_ || !command_queue_) {
      return true;
    }
    if (!recorded->has_dispatches()) {
      return true;
    }
    if (!ensure_compute_pipelines()) {
      return false;
    }
    wait_for_idle();
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (!fc_list) {
      return false;
    }
    fc_list->Reset();
    replay_compute(fc_list.get(), recorded->dispatches);
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    wait_for_idle();
    return true;
  }

  void write_ocean_fft_params(const OceanFftGpuParams& p) {
    OceanFftCb cb{};
    cb.size = p.size;
    cb.log2_size = p.log2_size;
    cb.stage = p.stage;
    cb.direction = p.direction;
    cb.time_sec = p.time_sec;
    cb.wind_speed = p.wind_speed;
    cb.wind_dir_rad = p.wind_dir_rad;
    cb.amp_scale = p.amp_scale;
    cb.patch_size = p.patch_size;
    cb.height_scale = p.height_scale;
    cb.disp_scale = p.disp_scale;
    cb.chop = p.chop;
    cb.spectrum_model = p.spectrum_model;
    cb.encode_channel = p.encode_channel;
    cb.gamma = p.gamma;
    cb.pad = p.pad;
    fft_cb_->UpdateUploadBuffer(0, &cb, sizeof(cb));
  }

  bool ensure_compute_pipelines() {
    if (compute_ready_) {
      return true;
    }
    if (!fc_device_ || !ensure_dxc_beside_exe()) {
      return false;
    }
    std::string cs_spec, cs_bitrev, cs_butter, cs_encode, cs_disp;
    if (!write_temp_hlsl("smartgis_rhi_cs_ocean_spectrum.hlsl", kCsOceanSpectrum,
                         &cs_spec) ||
        !write_temp_hlsl("smartgis_rhi_cs_ocean_bitrev.hlsl",
                         kCsOceanBitReverse, &cs_bitrev) ||
        !write_temp_hlsl("smartgis_rhi_cs_ocean_butterfly.hlsl",
                         kCsOceanButterfly, &cs_butter) ||
        !write_temp_hlsl("smartgis_rhi_cs_ocean_encode.hlsl",
                         kCsOceanHeightEncode, &cs_encode) ||
        !write_temp_hlsl("smartgis_rhi_cs_ocean_displace.hlsl",
                         kCsOceanDisplacementSpectrum, &cs_disp)) {
      return false;
    }
    cs_ocean_spectrum_ = fc_device_->CompileShader(
        {cs_spec, "main", ShaderType::kCompute, "6_0"});
    cs_ocean_bitrev_ = fc_device_->CompileShader(
        {cs_bitrev, "main", ShaderType::kCompute, "6_0"});
    cs_ocean_butterfly_ = fc_device_->CompileShader(
        {cs_butter, "main", ShaderType::kCompute, "6_0"});
    cs_ocean_encode_ = fc_device_->CompileShader(
        {cs_encode, "main", ShaderType::kCompute, "6_0"});
    cs_ocean_displace_ = fc_device_->CompileShader(
        {cs_disp, "main", ShaderType::kCompute, "6_0"});
    if (!cs_ocean_spectrum_ || !cs_ocean_bitrev_ || !cs_ocean_butterfly_ ||
        !cs_ocean_encode_ || !cs_ocean_displace_) {
      return false;
    }
    if (cs_ocean_spectrum_->GetBlob().empty() ||
        cs_ocean_bitrev_->GetBlob().empty() ||
        cs_ocean_butterfly_->GetBlob().empty() ||
        cs_ocean_encode_->GetBlob().empty() ||
        cs_ocean_displace_->GetBlob().empty()) {
      return false;
    }

    const uint64_t cb_align = fc_device_->GetConstantBufferOffsetAlignment();
    const uint64_t align = cb_align ? cb_align : 256;
    fft_cb_ = fc_device_->CreateBuffer(
        MemoryType::kUpload,
        {.size = Align(sizeof(OceanFftCb), align),
         .usage = BindFlag::kConstantBuffer});
    if (!fft_cb_) {
      return false;
    }
    ViewDesc cb_view = {
        .view_type = ViewType::kConstantBuffer,
        .dimension = ViewDimension::kBuffer,
    };
    fft_cb_view_ = fc_device_->CreateView(fft_cb_, cb_view);
    if (!fft_cb_view_) {
      return false;
    }

    BindKey spec_cb, spec_uav, spec_seed;
    BindKey bit_cb, bit_srv, bit_uav;
    BindKey but_cb, but_srv, but_uav;
    BindKey enc_cb, enc_srv, enc_uav;
    BindKey disp_cb, disp_srv, disp_uav;
    try {
      spec_cb = cs_ocean_spectrum_->GetBindKey("OceanFftCB");
      spec_uav = cs_ocean_spectrum_->GetBindKey("spectrum_uav");
      spec_seed = cs_ocean_spectrum_->GetBindKey("spectrum_seed_uav");
      bit_cb = cs_ocean_bitrev_->GetBindKey("OceanFftCB");
      bit_srv = cs_ocean_bitrev_->GetBindKey("src_tex");
      bit_uav = cs_ocean_bitrev_->GetBindKey("dst_tex");
      but_cb = cs_ocean_butterfly_->GetBindKey("OceanFftCB");
      but_srv = cs_ocean_butterfly_->GetBindKey("src_tex");
      but_uav = cs_ocean_butterfly_->GetBindKey("dst_tex");
      enc_cb = cs_ocean_encode_->GetBindKey("OceanFftCB");
      enc_srv = cs_ocean_encode_->GetBindKey("src_tex");
      enc_uav = cs_ocean_encode_->GetBindKey("height_uav");
      disp_cb = cs_ocean_displace_->GetBindKey("OceanFftCB");
      disp_srv = cs_ocean_displace_->GetBindKey("src_tex");
      disp_uav = cs_ocean_displace_->GetBindKey("dst_tex");
    } catch (const std::exception&) {
      return false;
    }
    spectrum_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {spec_cb, spec_uav, spec_seed}});
    bitrev_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {bit_cb, bit_srv, bit_uav}});
    butterfly_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {but_cb, but_srv, but_uav}});
    encode_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {enc_cb, enc_srv, enc_uav}});
    displace_layout_ = fc_device_->CreateBindingSetLayout(
        {.bind_keys = {disp_cb, disp_srv, disp_uav}});
    if (!spectrum_layout_ || !bitrev_layout_ || !butterfly_layout_ ||
        !encode_layout_ || !displace_layout_) {
      return false;
    }

    spectrum_pipeline_ = fc_device_->CreateComputePipeline(
        {.shader = cs_ocean_spectrum_, .layout = spectrum_layout_});
    bitrev_pipeline_ = fc_device_->CreateComputePipeline(
        {.shader = cs_ocean_bitrev_, .layout = bitrev_layout_});
    butterfly_pipeline_ = fc_device_->CreateComputePipeline(
        {.shader = cs_ocean_butterfly_, .layout = butterfly_layout_});
    encode_pipeline_ = fc_device_->CreateComputePipeline(
        {.shader = cs_ocean_encode_, .layout = encode_layout_});
    displace_pipeline_ = fc_device_->CreateComputePipeline(
        {.shader = cs_ocean_displace_, .layout = displace_layout_});
    if (!spectrum_pipeline_ || !bitrev_pipeline_ || !butterfly_pipeline_ ||
        !encode_pipeline_ || !displace_pipeline_) {
      return false;
    }
    compute_ready_ = true;
    return true;
  }

  void replay_compute(::CommandList* fc_list,
                      const std::vector<RecordedDispatch>& dispatches) {
    for (const RecordedDispatch& d : dispatches) {
      auto* uav0 = dynamic_cast<FlycubeTexture*>(d.uav0);
      auto* uav1 = dynamic_cast<FlycubeTexture*>(d.uav1);
      auto* srv0 = dynamic_cast<FlycubeTexture*>(d.srv0);
      write_ocean_fft_params(d.params);

      std::shared_ptr<Pipeline> pipe;
      std::shared_ptr<BindingSetLayout> layout;
      std::shared_ptr<Shader> shader;
      if (d.pipeline == ComputePipelineId::kOceanSpectrum) {
        pipe = spectrum_pipeline_;
        layout = spectrum_layout_;
        shader = cs_ocean_spectrum_;
      } else if (d.pipeline == ComputePipelineId::kOceanFftBitReverse) {
        pipe = bitrev_pipeline_;
        layout = bitrev_layout_;
        shader = cs_ocean_bitrev_;
      } else if (d.pipeline == ComputePipelineId::kOceanFftButterfly) {
        pipe = butterfly_pipeline_;
        layout = butterfly_layout_;
        shader = cs_ocean_butterfly_;
      } else if (d.pipeline == ComputePipelineId::kOceanHeightEncode) {
        pipe = encode_pipeline_;
        layout = encode_layout_;
        shader = cs_ocean_encode_;
      } else if (d.pipeline == ComputePipelineId::kOceanDisplacementSpectrum) {
        pipe = displace_pipeline_;
        layout = displace_layout_;
        shader = cs_ocean_displace_;
      } else {
        continue;
      }
      if (!pipe || !layout || !shader) {
        continue;
      }

      // Transition UAVs / SRVs for this dispatch.
      if (uav0 && uav0->shared()) {
        fc_list->ResourceBarrier({{uav0->shared(), ResourceState::kCommon,
                                   ResourceState::kUnorderedAccess}});
      }
      if (uav1 && uav1->shared()) {
        fc_list->ResourceBarrier({{uav1->shared(), ResourceState::kCommon,
                                   ResourceState::kUnorderedAccess}});
      }
      if (srv0 && srv0->shared()) {
        fc_list->ResourceBarrier(
            {{srv0->shared(), ResourceState::kCommon,
              ResourceState::kNonPixelShaderResource}});
      }

      auto set = fc_device_->CreateBindingSet(layout);
      std::vector<BindingDesc> bindings;
      try {
        bindings.push_back({shader->GetBindKey("OceanFftCB"), fft_cb_view_});
        if (d.pipeline == ComputePipelineId::kOceanSpectrum) {
          if (!uav0 || !uav0->uav() || !uav1 || !uav1->uav()) {
            continue;
          }
          bindings.push_back(
              {shader->GetBindKey("spectrum_uav"), uav0->uav()});
          bindings.push_back(
              {shader->GetBindKey("spectrum_seed_uav"), uav1->uav()});
        } else if (d.pipeline == ComputePipelineId::kOceanHeightEncode) {
          if (!srv0 || !srv0->srv() || !uav0 || !uav0->uav()) {
            continue;
          }
          bindings.push_back({shader->GetBindKey("src_tex"), srv0->srv()});
          bindings.push_back({shader->GetBindKey("height_uav"), uav0->uav()});
        } else {
          // Bit-reverse, butterfly, and displacement spectrum share SRV+UAV.
          if (!srv0 || !srv0->srv() || !uav0 || !uav0->uav()) {
            continue;
          }
          bindings.push_back({shader->GetBindKey("src_tex"), srv0->srv()});
          bindings.push_back({shader->GetBindKey("dst_tex"), uav0->uav()});
        }
      } catch (const std::exception&) {
        continue;
      }
      set->WriteBindings({.bindings = bindings});
      fc_list->BindPipeline(pipe);
      fc_list->BindBindingSet(set);
      fc_list->Dispatch(d.groups_x, d.groups_y, d.groups_z);

      if (d.barrier_after) {
        if (uav0 && uav0->shared()) {
          fc_list->UAVResourceBarrier(uav0->shared());
          fc_list->ResourceBarrier(
              {{uav0->shared(), ResourceState::kUnorderedAccess,
                ResourceState::kAllShaderResource}});
        }
        if (uav1 && uav1->shared()) {
          fc_list->UAVResourceBarrier(uav1->shared());
        }
      }
    }
  }

  Backend backend_;
  HWND hwnd_ = nullptr;
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  uint64_t fence_value_ = 0;
  uint64_t graphics_fence_values_[kFrameCount] = {};
  uint32_t gpu_sampled_draws_ = 0;
  bool pipelines_ready_ = false;
  bool compute_ready_ = false;
  uint32_t depth_w_ = 0;
  uint32_t depth_h_ = 0;
  std::shared_ptr<Instance> instance_;
  std::shared_ptr<::Device> fc_device_;
  std::shared_ptr<CommandQueue> command_queue_;
  std::shared_ptr<Swapchain> swapchain_;
  std::shared_ptr<Fence> fence_;
  std::shared_ptr<::CommandList> graphics_lists_[kFrameCount];
  std::vector<std::shared_ptr<View>> back_buffer_views_;
  std::shared_ptr<Resource> depth_texture_;
  std::shared_ptr<View> depth_view_;
  std::shared_ptr<Shader> vs_textured_;
  std::shared_ptr<Shader> ps_textured_;
  std::shared_ptr<Shader> vs_solid_;
  std::shared_ptr<Shader> ps_solid_;
  std::shared_ptr<Shader> vs_ocean_;
  std::shared_ptr<Shader> ps_ocean_;
  std::shared_ptr<Shader> vs_cloud_;
  std::shared_ptr<Shader> ps_cloud_;
  std::shared_ptr<Shader> cs_ocean_spectrum_;
  std::shared_ptr<Shader> cs_ocean_bitrev_;
  std::shared_ptr<Shader> cs_ocean_butterfly_;
  std::shared_ptr<Shader> cs_ocean_encode_;
  std::shared_ptr<Shader> cs_ocean_displace_;
  std::shared_ptr<BindingSetLayout> sampled_layout_;
  std::shared_ptr<BindingSetLayout> solid_layout_;
  std::shared_ptr<BindingSetLayout> ocean_layout_;
  std::shared_ptr<BindingSetLayout> cloud_layout_;
  std::shared_ptr<BindingSetLayout> spectrum_layout_;
  std::shared_ptr<BindingSetLayout> bitrev_layout_;
  std::shared_ptr<BindingSetLayout> butterfly_layout_;
  std::shared_ptr<BindingSetLayout> encode_layout_;
  std::shared_ptr<BindingSetLayout> displace_layout_;
  std::shared_ptr<Pipeline> sampled_pipeline_;
  std::shared_ptr<Pipeline> sampled_depth_pipeline_;
  std::shared_ptr<Pipeline> solid_pipeline_;
  std::shared_ptr<Pipeline> solid_depth_pipeline_;
  std::shared_ptr<Pipeline> ocean_pipeline_;
  std::shared_ptr<Pipeline> cloud_pipeline_;
  std::shared_ptr<Pipeline> spectrum_pipeline_;
  std::shared_ptr<Pipeline> bitrev_pipeline_;
  std::shared_ptr<Pipeline> butterfly_pipeline_;
  std::shared_ptr<Pipeline> encode_pipeline_;
  std::shared_ptr<Pipeline> displace_pipeline_;
  std::shared_ptr<Resource> camera_cb_;
  std::shared_ptr<View> camera_cb_view_;
  std::shared_ptr<Resource> color_cb_;
  std::shared_ptr<View> color_cb_view_;
  std::shared_ptr<Resource> ocean_cb_;
  std::shared_ptr<View> ocean_cb_view_;
  std::shared_ptr<Resource> cloud_cb_;
  std::shared_ptr<View> cloud_cb_view_;
  std::shared_ptr<Resource> fft_cb_;
  std::shared_ptr<View> fft_cb_view_;
  std::shared_ptr<BindingSet> solid_set_;
  std::shared_ptr<BindingSet> ocean_set_;
  std::shared_ptr<BindingSet> cloud_set_;
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
