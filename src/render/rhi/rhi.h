// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_RHI_H_
#define RENDER_RHI_RHI_H_

#include <cstdint>
#include <cstring>
#include <vector>

#include "render/render_export.h"

// Facade RHI for 2D + 3D map drawing. GPU backends are FlyCube (DX12 / Vulkan);
// GDI/GL remain leftover HWND adapters. FlyCube types do not appear here.

namespace render {
namespace rhi {

enum class Backend {
  kNull,
  kDx12,
  kVulkan,
  kGdi,
  kGl,
};

enum class CommandListType { kGraphics, kCompute, kCopy };

enum class BufferUsage : uint32_t {
  kVertex = 1,
  kIndex = 2,
};

enum class TextureFormat : uint32_t {
  kRgba8 = 1,
  kRg32Float = 2,    // complex spectrum ping-pong (GPU FFT)
  kRgba32Float = 3,
};

// Bitmask for TextureDesc::usage. Sampled + CopyDest is the raster default.
enum class TextureUsage : uint32_t {
  kSampled = 1u << 0,
  kStorage = 1u << 1,   // UAV / RWTexture
  kCopyDest = 1u << 2,
};

inline TextureUsage operator|(TextureUsage a, TextureUsage b) {
  return static_cast<TextureUsage>(static_cast<uint32_t>(a) |
                                   static_cast<uint32_t>(b));
}
inline TextureUsage operator&(TextureUsage a, TextureUsage b) {
  return static_cast<TextureUsage>(static_cast<uint32_t>(a) &
                                   static_cast<uint32_t>(b));
}
inline bool has_texture_usage(TextureUsage mask, TextureUsage bit) {
  return (static_cast<uint32_t>(mask) & static_cast<uint32_t>(bit)) != 0;
}

struct TextureDesc {
  uint32_t width;
  uint32_t height;
  TextureFormat format;
  TextureUsage usage;
  TextureDesc()
      : width(1),
        height(1),
        format(TextureFormat::kRgba8),
        usage(TextureUsage::kSampled | TextureUsage::kCopyDest) {}
};

struct DeviceDesc {
  void* native_window;
  uint32_t width;
  uint32_t height;
  DeviceDesc() : native_window(nullptr), width(0), height(0) {}
};

// Color attachment load at begin_render_pass. FlyCube executes each
// begin/end_render_pass as a real GPU pass (sequential). kClear clears the
// color target; kLoad preserves prior pass contents for compositing.
enum class ColorLoadOp : uint32_t {
  kClear = 0,
  kLoad = 1,
};

// Depth attachment load when enable_depth is set on RenderPassDesc.
enum class DepthLoadOp : uint32_t {
  kClear = 0,
  kLoad = 1,
};

// Built-in graphics pipelines. FlyCube compiles HLSL; Null records the id.
enum class PipelineId : uint32_t {
  kAuto = 0,      // textured if a texture is bound, else solid
  kSolid = 1,
  kTextured = 2,
  kOcean = 3,     // height-map displace + Fresnel water
  kCloud = 4,     // billowy / short raymarch with alpha
};

// Built-in compute pipelines for ocean GPU FFT (FlyCube); Null records only.
enum class ComputePipelineId : uint32_t {
  kNone = 0,
  kOceanSpectrum = 1,  // JONSWAP (or Phillips) → RG32F height spectrum + seed
  kOceanFftBitReverse = 2,  // 1D bit-reverse along row or column
  kOceanFftButterfly = 3,   // radix-2 Cooley-Tukey stage (ping-pong)
  kOceanHeightEncode = 4,   // complex → RGBA8 (R=h, G=Dx, B=Dz)
  // Height spectrum → Dx/Dz spectrum: -i * chop * (k_axis/|k|) * ĥ
  kOceanDisplacementSpectrum = 5,
};

// Spectrum shape for kOceanSpectrum (energy still normalized to Hs on CPU).
enum class OceanSpectrumModel : uint32_t {
  kPhillips = 0,  // low-quality / legacy fallback
  kJonswap = 1,   // peak-enhanced directional JONSWAP-lite
};

// Constants for ocean compute FFT (set_ocean_fft_params).
struct OceanFftGpuParams {
  uint32_t size = 64;
  uint32_t log2_size = 6;
  uint32_t stage = 0;      // butterfly stage; 0 → len=2
  uint32_t direction = 0;  // 0 = rows/X (or Dx), 1 = columns/Y (or Dz)
  float time_sec = 0.f;
  float wind_speed = 5.f;
  float wind_dir_rad = 0.f;
  float amp_scale = 1.f;  // energy-normalized amplitude (Hs → σ = Hs/4)
  float patch_size = 100.f;
  float height_scale = 1.f;  // RGBA encode scale for height (R)
  float disp_scale = 1.f;    // RGBA encode scale for Dx/Dz (G/B)
  float chop = 1.f;          // Tessendorf horizontal displacement strength
  uint32_t spectrum_model =
      static_cast<uint32_t>(OceanSpectrumModel::kJonswap);
  uint32_t encode_channel = 0;  // 0=R height, 1=G Dx, 2=B Dz
  float gamma = 3.3f;           // JONSWAP peak enhancement
  float pad = 0.f;
};

enum class BlendMode : uint32_t {
  kOpaque = 0,
  kSrcAlpha = 1,  // src.rgb * src.a + dst.rgb * (1 - src.a)
};

enum class DepthMode : uint32_t {
  kDisabled = 0,
  kWrite = 1,     // depth test + write
  kTestOnly = 2,  // depth test, no write (soft clouds)
};

struct RenderPassDesc {
  float clear_r;
  float clear_g;
  float clear_b;
  float clear_a;
  uint32_t width;
  uint32_t height;
  ColorLoadOp load_op;
  // When true, FlyCube attaches a shared depth buffer for this pass.
  bool enable_depth;
  DepthLoadOp depth_load_op;
  float depth_clear;
  RenderPassDesc()
      : clear_r(0),
        clear_g(0),
        clear_b(0),
        clear_a(1),
        width(0),
        height(0),
        load_op(ColorLoadOp::kClear),
        enable_depth(false),
        depth_load_op(DepthLoadOp::kClear),
        depth_clear(1.f) {}
};

// GPU constants for PipelineId::kOcean (uploaded by set_ocean_params).
struct OceanGpuParams {
  float deep_r = 0.02f;
  float deep_g = 0.12f;
  float deep_b = 0.28f;
  float deep_a = 1.f;
  float shallow_r = 0.15f;
  float shallow_g = 0.45f;
  float shallow_b = 0.55f;
  float shallow_a = 1.f;
  float fresnel_bias = 0.04f;
  float fresnel_power = 5.f;
  float height_scale = 1.f;
  float cam_x = 0.f;
  float cam_y = 2.f;
  float cam_z = 4.f;
  float disp_scale = 1.f;  // horizontal chop decode (G/B of height map)
  float pad1 = 0.f;
};

// GPU constants for PipelineId::kCloud (uploaded by set_cloud_params).
struct CloudGpuParams {
  float sun_x = 0.f;
  float sun_y = 0.7071f;
  float sun_z = 0.7071f;
  float cover = 1.f;
  float base_m = 1000.f;
  float top_m = 3000.f;
  float extinction = 0.02f;
  float steps = 16.f;
  float cam_x = 0.f;
  float cam_y = 0.f;
  float cam_z = 4.f;
  float pad0 = 0.f;
};

enum class CameraKind : uint32_t {
  kOrtho = 1,
  kPerspective = 2,
};

inline void set_identity4(float m[16]) {
  for (int i = 0; i < 16; ++i) {
    m[i] = 0.f;
  }
  m[0] = m[5] = m[10] = m[15] = 1.f;
}

// Column-major 4x4 view + projection. 2D GIS uses ortho; leftover 3D uses
// perspective. Null records the bind; FlyCube uploads GPU constants.
struct CameraMatrices {
  float view[16];
  float proj[16];
  CameraKind kind;
  CameraMatrices() : kind(CameraKind::kOrtho) {
    set_identity4(view);
    set_identity4(proj);
  }
};

RENDER_EXPORT CameraMatrices make_ortho_camera(float left, float right,
                                               float bottom, float top,
                                               float near_z, float far_z);
RENDER_EXPORT CameraMatrices make_perspective_camera(float fov_y_radians,
                                                     float aspect,
                                                     float near_z,
                                                     float far_z);
// Orbit / trackball camera looking at the origin (yaw around Y, pitch around X).
RENDER_EXPORT CameraMatrices make_orbit_camera(float yaw_radians,
                                               float pitch_radians,
                                               float distance,
                                               float fov_y_radians,
                                               float aspect, float near_z,
                                               float far_z);

// GPU or CPU heap for vertex/index bytes. FlyCube types stay out of this header.
class Buffer {
 public:
  virtual ~Buffer() = default;
  virtual uint32_t byte_size() const = 0;
};

// CPU-side buffer used by null and leftover backends (and FlyCube before init).
class StubBuffer : public Buffer {
 public:
  std::vector<uint8_t> bytes;
  BufferUsage usage;

  StubBuffer(uint32_t size, BufferUsage u) : usage(u) { bytes.resize(size); }
  uint32_t byte_size() const override {
    return static_cast<uint32_t>(bytes.size());
  }
};

namespace detail {

inline Buffer* make_stub_buffer(uint32_t byte_size, BufferUsage usage) {
  if (byte_size == 0) {
    return nullptr;
  }
  return new StubBuffer(byte_size, usage);
}

inline void destroy_stub_buffer(Buffer* buffer) { delete buffer; }

inline bool upload_stub_buffer(Buffer* buffer, const void* data,
                               uint32_t byte_size) {
  auto* stub = static_cast<StubBuffer*>(buffer);
  if (!stub || !data || byte_size > stub->byte_size()) {
    return false;
  }
  if (byte_size != 0) {
    std::memcpy(stub->bytes.data(), data, byte_size);
  }
  return true;
}

inline uint32_t texture_bytes_per_pixel(TextureFormat format) {
  switch (format) {
    case TextureFormat::kRg32Float:
      return 8u;
    case TextureFormat::kRgba32Float:
      return 16u;
    case TextureFormat::kRgba8:
    default:
      return 4u;
  }
}

inline uint32_t texture_byte_size(const TextureDesc& desc) {
  const uint32_t bpp = texture_bytes_per_pixel(desc.format);
  if (desc.width == 0 || desc.height == 0) {
    return 0;
  }
  return desc.width * desc.height * bpp;
}

}  // namespace detail

// GPU or CPU 2D image. FlyCube types stay out of this header.
class Texture {
 public:
  virtual ~Texture() = default;
  virtual uint32_t width() const = 0;
  virtual uint32_t height() const = 0;
  virtual uint32_t byte_size() const = 0;
};

// CPU-side texture used by null and leftover backends (and FlyCube before init).
class StubTexture : public Texture {
 public:
  std::vector<uint8_t> bytes;
  uint32_t w;
  uint32_t h;
  TextureFormat format;

  explicit StubTexture(const TextureDesc& desc)
      : w(desc.width), h(desc.height), format(desc.format) {
    bytes.resize(detail::texture_byte_size(desc));
  }
  uint32_t width() const override { return w; }
  uint32_t height() const override { return h; }
  uint32_t byte_size() const override {
    return static_cast<uint32_t>(bytes.size());
  }
};

namespace detail {

inline Texture* make_stub_texture(const TextureDesc& desc) {
  if (desc.width == 0 || desc.height == 0) {
    return nullptr;
  }
  return new StubTexture(desc);
}

inline void destroy_stub_texture(Texture* texture) { delete texture; }

inline bool upload_stub_texture(Texture* texture, const void* data,
                                uint32_t byte_size) {
  auto* stub = static_cast<StubTexture*>(texture);
  if (!stub || !data || byte_size > stub->byte_size()) {
    return false;
  }
  if (byte_size != 0) {
    std::memcpy(stub->bytes.data(), data, byte_size);
  }
  return true;
}

}  // namespace detail

class CommandList {
 public:
  virtual ~CommandList() = default;
  virtual void set_viewport(float x, float y, float w, float h, float min_depth,
                             float max_depth) = 0;
  virtual void begin_render_pass(const RenderPassDesc& desc) = 0;
  virtual void end_render_pass() = 0;
  virtual void bind_vertex_buffer(Buffer* buffer, uint32_t offset,
                                  uint32_t stride) = 0;
  virtual void bind_index_buffer(Buffer* buffer, uint32_t offset) = 0;
  virtual void draw_indexed(uint32_t index_count, uint32_t instance_count,
                             uint32_t first_index, int32_t vertex_offset,
                             uint32_t first_instance) = 0;
  virtual void close() = 0;
  virtual void bind_texture(Texture* texture, uint32_t slot) {
    (void)texture;
    (void)slot;
  }
  virtual void bind_camera(const CameraMatrices& camera) { (void)camera; }
  // Solid-color draw path (vector fills / untextured meshes). FlyCube uploads
  // a ColorCB; null/stub backends only record the values.
  virtual void set_solid_color(float r, float g, float b, float a) {
    (void)r;
    (void)g;
    (void)b;
    (void)a;
  }
  // Select a built-in pipeline (ocean/cloud/solid/textured). kAuto picks
  // textured vs solid from the bound texture.
  virtual void set_pipeline(PipelineId id) { (void)id; }
  virtual void set_blend_mode(BlendMode mode) { (void)mode; }
  virtual void set_depth_mode(DepthMode mode) { (void)mode; }
  virtual void set_ocean_params(const OceanGpuParams& params) { (void)params; }
  virtual void set_cloud_params(const CloudGpuParams& params) { (void)params; }

  // Compute (ocean GPU FFT). Null records counters; FlyCube dispatches on execute.
  virtual void set_compute_pipeline(ComputePipelineId id) { (void)id; }
  virtual void set_ocean_fft_params(const OceanFftGpuParams& params) {
    (void)params;
  }
  virtual void bind_compute_srv(Texture* texture, uint32_t slot) {
    (void)texture;
    (void)slot;
  }
  virtual void bind_compute_uav(Texture* texture, uint32_t slot) {
    (void)texture;
    (void)slot;
  }
  virtual void dispatch(uint32_t group_count_x, uint32_t group_count_y,
                        uint32_t group_count_z) {
    (void)group_count_x;
    (void)group_count_y;
    (void)group_count_z;
  }
  virtual void uav_barrier() {}
};

// Records counters. Used by null and leftover backends.
class StubCommandList : public CommandList {
 public:
  uint32_t draw_indexed_calls = 0;
  uint32_t bind_vertex_calls = 0;
  uint32_t bind_index_calls = 0;
  uint32_t bind_texture_calls = 0;
  uint32_t bind_camera_calls = 0;
  uint32_t set_solid_color_calls = 0;
  Texture* last_texture = nullptr;
  CameraMatrices last_camera;
  float solid_r = 0.85f;
  float solid_g = 0.85f;
  float solid_b = 0.90f;
  float solid_a = 1.f;
  bool closed = false;
  bool pass_open = false;
  std::vector<uint32_t> index_counts;
  // Append-only counters (keep earlier layout stable for cross-TU stubs).
  uint32_t begin_render_pass_calls = 0;
  uint32_t end_render_pass_calls = 0;
  uint32_t clear_load_calls = 0;
  uint32_t load_load_calls = 0;
  uint32_t depth_enabled_pass_calls = 0;
  uint32_t set_pipeline_calls = 0;
  uint32_t set_blend_mode_calls = 0;
  uint32_t set_depth_mode_calls = 0;
  uint32_t set_ocean_params_calls = 0;
  uint32_t set_cloud_params_calls = 0;
  ColorLoadOp last_load_op = ColorLoadOp::kClear;
  PipelineId last_pipeline = PipelineId::kAuto;
  BlendMode last_blend = BlendMode::kOpaque;
  DepthMode last_depth = DepthMode::kDisabled;
  OceanGpuParams last_ocean;
  CloudGpuParams last_cloud;
  RenderPassDesc last_pass;
  // Append-only compute counters (keep earlier layout stable for cross-TU stubs).
  uint32_t set_compute_pipeline_calls = 0;
  uint32_t set_ocean_fft_params_calls = 0;
  uint32_t bind_compute_srv_calls = 0;
  uint32_t bind_compute_uav_calls = 0;
  uint32_t dispatch_calls = 0;
  uint32_t uav_barrier_calls = 0;
  ComputePipelineId last_compute_pipeline = ComputePipelineId::kNone;
  OceanFftGpuParams last_ocean_fft;
  uint32_t last_dispatch_x = 0;
  uint32_t last_dispatch_y = 0;
  uint32_t last_dispatch_z = 0;

  void set_viewport(float, float, float, float, float, float) override {}
  void begin_render_pass(const RenderPassDesc& desc) override {
    pass_open = true;
    last_pass = desc;
    last_load_op = desc.load_op;
    ++begin_render_pass_calls;
    if (desc.load_op == ColorLoadOp::kClear) {
      ++clear_load_calls;
    } else {
      ++load_load_calls;
    }
    if (desc.enable_depth) {
      ++depth_enabled_pass_calls;
    }
  }
  void end_render_pass() override {
    pass_open = false;
    ++end_render_pass_calls;
  }
  void bind_vertex_buffer(Buffer*, uint32_t, uint32_t) override {
    ++bind_vertex_calls;
  }
  void bind_index_buffer(Buffer*, uint32_t) override { ++bind_index_calls; }
  void bind_texture(Texture* texture, uint32_t) override {
    ++bind_texture_calls;
    last_texture = texture;
  }
  void bind_camera(const CameraMatrices& camera) override {
    ++bind_camera_calls;
    last_camera = camera;
  }
  void set_solid_color(float r, float g, float b, float a) override {
    ++set_solid_color_calls;
    solid_r = r;
    solid_g = g;
    solid_b = b;
    solid_a = a;
  }
  void set_pipeline(PipelineId id) override {
    ++set_pipeline_calls;
    last_pipeline = id;
  }
  void set_blend_mode(BlendMode mode) override {
    ++set_blend_mode_calls;
    last_blend = mode;
  }
  void set_depth_mode(DepthMode mode) override {
    ++set_depth_mode_calls;
    last_depth = mode;
  }
  void set_ocean_params(const OceanGpuParams& params) override {
    ++set_ocean_params_calls;
    last_ocean = params;
  }
  void set_cloud_params(const CloudGpuParams& params) override {
    ++set_cloud_params_calls;
    last_cloud = params;
  }
  void set_compute_pipeline(ComputePipelineId id) override {
    ++set_compute_pipeline_calls;
    last_compute_pipeline = id;
  }
  void set_ocean_fft_params(const OceanFftGpuParams& params) override {
    ++set_ocean_fft_params_calls;
    last_ocean_fft = params;
  }
  void bind_compute_srv(Texture* texture, uint32_t) override {
    ++bind_compute_srv_calls;
    last_texture = texture;
  }
  void bind_compute_uav(Texture* texture, uint32_t) override {
    ++bind_compute_uav_calls;
    last_texture = texture;
  }
  void dispatch(uint32_t group_count_x, uint32_t group_count_y,
                uint32_t group_count_z) override {
    ++dispatch_calls;
    last_dispatch_x = group_count_x;
    last_dispatch_y = group_count_y;
    last_dispatch_z = group_count_z;
  }
  void uav_barrier() override { ++uav_barrier_calls; }
  void draw_indexed(uint32_t index_count, uint32_t, uint32_t, int32_t,
                    uint32_t) override {
    ++draw_indexed_calls;
    index_counts.push_back(index_count);
  }
  void close() override { closed = true; }
};

class Device {
 public:
  virtual ~Device() = default;
  virtual bool initialize(const DeviceDesc& desc) = 0;
  virtual void shutdown() = 0;
  virtual void present() = 0;
  virtual Backend backend() const = 0;
  virtual CommandList* create_command_list() = 0;
  virtual void destroy_command_list(CommandList* list) = 0;
  virtual bool execute(CommandList* list) = 0;
  virtual Buffer* create_buffer(uint32_t byte_size, BufferUsage usage) = 0;
  virtual void destroy_buffer(Buffer* buffer) = 0;
  virtual bool upload(Buffer* buffer, const void* data, uint32_t byte_size) = 0;
  virtual Texture* create_texture(const TextureDesc& desc) {
    return detail::make_stub_texture(desc);
  }
  virtual void destroy_texture(Texture* texture) {
    detail::destroy_stub_texture(texture);
  }
  virtual bool upload_texture(Texture* texture, const void* data,
                              uint32_t byte_size) {
    return detail::upload_stub_texture(texture, data, byte_size);
  }
  // FlyCube increments this when execute issues a sampled DrawIndexed.
  virtual uint32_t gpu_sampled_draws() const { return 0; }
  // Null/stub backends increment on each successful execute/submit.
  virtual uint32_t execute_count() const { return 0; }
  // True when the backend can compile/dispatch compute (FlyCube). Null = false.
  virtual bool supports_compute() const { return false; }
};

RENDER_EXPORT Device* create_device(Backend backend);
RENDER_EXPORT Backend preferred_gpu_backend();

}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_RHI_H_
