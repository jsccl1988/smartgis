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

// Human-readable RHI / present path for HUD / status (e.g. "FlyCube/DX12").
RENDER_EXPORT const char* backend_display_name(Backend backend);

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
  DeviceDesc() : native_window(nullptr), width(0), height(0), adapter_index(0) {}
  void* native_window;
  uint32_t width;
  uint32_t height;
  // DXGI / FlyCube adapter ordinal (GpuDeviceHub AdapterId). 0 = first.
  uint32_t adapter_index;
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

enum class BlendMode : uint32_t {
  kOpaque = 0,
  kSrcAlpha = 1,  // src.rgb * src.a + dst.rgb * (1 - src.a)
};

enum class DepthMode : uint32_t {
  kDisabled = 0,
  kWrite = 1,     // depth test + write
  kTestOnly = 2,  // depth test, no write (soft clouds)
};

enum class ShaderStage : uint32_t { kVertex, kPixel, kCompute };

enum class BindingKind : uint32_t {
  kConstantBuffer,
  kSrv,
  kUav,
  kSampler,
};

// Vertex fetch layouts the product shaders use. FlyCube maps these to input
// elements; the header does not name a backend input-layout type.
enum class VertexLayout : uint32_t {
  kPosition,        // float3
  kPositionUv,      // float3 + float2
  kPositionNormal,  // float3 + float3
};

struct ShaderSource {
  const char* hlsl = nullptr;
  const char* entry = "main";
  const char* profile = "6_0";
};

// slot is the index passed to set_constants / bind_texture / bind_compute_*.
// hlsl_name is the shader bind name (for example "CameraCB"). The same slot
// may be listed twice (vertex and pixel); one set_constants writes every match.
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
  int32_t camera_slot = 0;  // -1: bind_camera does not write a constant
};

struct ComputePipelineDesc {
  ShaderSource compute;
  const BindingSlot* bindings = nullptr;
  uint32_t binding_count = 0;
};

// Opaque program. Callers hold the pointer and compare it in tests.
class Pipeline {
 public:
  virtual ~Pipeline() = default;
};

// CPU stand-in used by null and leftover backends. Records the binding table
// so command lists can drop oversized constant writes at draw time.
class StubPipeline : public Pipeline {
 public:
  StubPipeline(bool compute, std::vector<BindingSlot> bindings,
               int32_t camera_slot)
      : compute_(compute),
        bindings_(std::move(bindings)),
        camera_slot_(camera_slot) {}

  bool is_compute() const { return compute_; }
  int32_t camera_slot() const { return camera_slot_; }
  const std::vector<BindingSlot>& bindings() const { return bindings_; }

  // 0 when this pipeline has no constant buffer at slot.
  uint32_t constant_slot_size(uint32_t slot) const {
    uint32_t size = 0;
    bool found = false;
    for (const BindingSlot& binding : bindings_) {
      if (binding.kind != BindingKind::kConstantBuffer || binding.slot != slot) {
        continue;
      }
      if (!found) {
        size = binding.size_bytes;
        found = true;
      }
    }
    return found ? size : 0;
  }

 private:
  bool compute_ = false;
  std::vector<BindingSlot> bindings_;
  int32_t camera_slot_ = -1;
};

namespace detail {

inline bool binding_slot_named(const BindingSlot& binding) {
  return binding.hlsl_name != nullptr && binding.hlsl_name[0] != '\0';
}

inline bool constant_slots_consistent(const BindingSlot* bindings,
                                      uint32_t binding_count) {
  if (binding_count > 0 && bindings == nullptr) {
    return false;
  }
  for (uint32_t i = 0; i < binding_count; ++i) {
    if (!binding_slot_named(bindings[i])) {
      return false;
    }
    if (bindings[i].kind != BindingKind::kConstantBuffer) {
      continue;
    }
    for (uint32_t j = 0; j < i; ++j) {
      if (bindings[j].kind == BindingKind::kConstantBuffer &&
          bindings[j].slot == bindings[i].slot &&
          bindings[j].size_bytes != bindings[i].size_bytes) {
        return false;
      }
    }
  }
  return true;
}

inline bool camera_slot_ok(const GraphicsPipelineDesc& desc) {
  if (desc.camera_slot < 0) {
    return true;
  }
  const auto slot = static_cast<uint32_t>(desc.camera_slot);
  for (uint32_t i = 0; i < desc.binding_count; ++i) {
    const BindingSlot& binding = desc.bindings[i];
    if (binding.kind == BindingKind::kConstantBuffer && binding.slot == slot &&
        binding.size_bytes >= 128u) {
      return true;
    }
  }
  return false;
}

inline bool graphics_pipeline_desc_ok(const GraphicsPipelineDesc& desc) {
  if (desc.vertex.hlsl == nullptr || desc.vertex.hlsl[0] == '\0' ||
      desc.pixel.hlsl == nullptr || desc.pixel.hlsl[0] == '\0') {
    return false;
  }
  return constant_slots_consistent(desc.bindings, desc.binding_count) &&
         camera_slot_ok(desc);
}

inline bool compute_pipeline_desc_ok(const ComputePipelineDesc& desc) {
  if (desc.compute.hlsl == nullptr || desc.compute.hlsl[0] == '\0') {
    return false;
  }
  return constant_slots_consistent(desc.bindings, desc.binding_count);
}

inline std::vector<BindingSlot> copy_bindings(const BindingSlot* bindings,
                                              uint32_t binding_count) {
  if (bindings == nullptr || binding_count == 0) {
    return {};
  }
  return std::vector<BindingSlot>(bindings, bindings + binding_count);
}

}  // namespace detail

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

// True for stack/heap object addresses. Rejects near-null and small integers
// mistaken for refs (vtable slot skew calling set_*_params with an enum).
inline bool is_plausible_object_pointer(const void* p) {
  return reinterpret_cast<std::uintptr_t>(p) >= 4096u;
}

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
  // Graphics or compute program. nullptr skips the later draw or dispatch.
  virtual void set_pipeline(Pipeline* pipeline) { (void)pipeline; }
  // Latest bytes for one constant slot. Ignored when data is null or size is 0.
  // Does not require a pipeline to be bound first.
  virtual void set_constants(uint32_t slot, const void* data, uint32_t byte_size) {
    (void)slot;
    (void)data;
    (void)byte_size;
  }
  virtual void set_blend_mode(BlendMode mode) { (void)mode; }
  virtual void set_depth_mode(DepthMode mode) { (void)mode; }
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
  uint32_t set_constants_calls = 0;
  Texture* last_texture = nullptr;
  CameraMatrices last_camera;
  bool closed = false;
  bool pass_open = false;
  std::vector<uint32_t> index_counts;
  uint32_t begin_render_pass_calls = 0;
  uint32_t end_render_pass_calls = 0;
  uint32_t clear_load_calls = 0;
  uint32_t load_load_calls = 0;
  uint32_t depth_enabled_pass_calls = 0;
  uint32_t set_pipeline_calls = 0;
  uint32_t set_blend_mode_calls = 0;
  uint32_t set_depth_mode_calls = 0;
  ColorLoadOp last_load_op = ColorLoadOp::kClear;
  Pipeline* last_pipeline = nullptr;
  BlendMode last_blend = BlendMode::kOpaque;
  DepthMode last_depth = DepthMode::kDisabled;
  RenderPassDesc last_pass;
  uint32_t bind_compute_srv_calls = 0;
  uint32_t bind_compute_uav_calls = 0;
  uint32_t dispatch_calls = 0;
  uint32_t uav_barrier_calls = 0;
  uint32_t last_dispatch_x = 0;
  uint32_t last_dispatch_y = 0;
  uint32_t last_dispatch_z = 0;

  // Last set_constants body per slot. Writes larger than 256 bytes increment
  // the counter and record the size, but do not keep the bytes.
  struct ConstantRecord {
    uint32_t slot = 0;
    uint32_t byte_size = 0;
    uint8_t bytes[256] = {};
    bool has_bytes = false;
  };
  std::vector<ConstantRecord> constants;

  const ConstantRecord* constant_at(uint32_t slot) const {
    for (const ConstantRecord& record : constants) {
      if (record.slot == slot) {
        return &record;
      }
    }
    return nullptr;
  }

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
  void set_pipeline(Pipeline* pipeline) override {
    ++set_pipeline_calls;
    last_pipeline = pipeline;
  }
  void set_constants(uint32_t slot, const void* data, uint32_t byte_size) override {
    if (data == nullptr || byte_size == 0) {
      return;
    }
    ++set_constants_calls;
    ConstantRecord* found = nullptr;
    for (ConstantRecord& record : constants) {
      if (record.slot == slot) {
        found = &record;
        break;
      }
    }
    if (found == nullptr) {
      constants.push_back(ConstantRecord{});
      found = &constants.back();
      found->slot = slot;
    }
    found->byte_size = byte_size;
    found->has_bytes = byte_size <= 256u;
    if (found->has_bytes) {
      std::memcpy(found->bytes, data, byte_size);
    }
  }
  void set_blend_mode(BlendMode mode) override {
    ++set_blend_mode_calls;
    last_blend = mode;
  }
  void set_depth_mode(DepthMode mode) override {
    ++set_depth_mode_calls;
    last_depth = mode;
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

  virtual Pipeline* create_graphics_pipeline(const GraphicsPipelineDesc& desc) {
    if (!detail::graphics_pipeline_desc_ok(desc)) {
      return nullptr;
    }
    return new StubPipeline(false,
                            detail::copy_bindings(desc.bindings, desc.binding_count),
                            desc.camera_slot);
  }
  virtual Pipeline* create_compute_pipeline(const ComputePipelineDesc& desc) {
    if (!detail::compute_pipeline_desc_ok(desc)) {
      return nullptr;
    }
    return new StubPipeline(true,
                            detail::copy_bindings(desc.bindings, desc.binding_count),
                            -1);
  }
  virtual void destroy_pipeline(Pipeline* pipeline) { delete pipeline; }

  // Optional DXGI NT shared-handle import for GPU-process present into the
  // browser-facing surface. Default false until a backend implements it.
  // |nt_handle| is a Win32 HANDLE value (void* to avoid windows.h here).
  virtual bool import_shared_nt_handle(void* nt_handle, uint32_t width_px,
                                       uint32_t height_px) {
    (void)nt_handle;
    (void)width_px;
    (void)height_px;
    return false;
  }
  // Copy one BGRA8 buffer into the imported shared texture. Requires a prior
  // successful import_shared_nt_handle on this device.
  virtual bool copy_bgra_to_imported_shared(const uint8_t* bgra,
                                            uint32_t stride_bytes,
                                            uint32_t width_px,
                                            uint32_t height_px) {
    (void)bgra;
    (void)stride_bytes;
    (void)width_px;
    (void)height_px;
    return false;
  }
  // True after import_shared_nt_handle succeeded for the current generation.
  virtual bool has_imported_shared() const { return false; }
  // True when the last successful execute() wrote color into the imported
  // shared texture (compose-direct path). Cleared on the next execute/import.
  virtual bool composed_into_imported_shared() const { return false; }
};

RENDER_EXPORT Device* create_device(Backend backend);
RENDER_EXPORT Backend preferred_gpu_backend();

}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_RHI_H_
