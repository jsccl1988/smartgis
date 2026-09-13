// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_RHI_H_
#define RENDER_RHI_RHI_H_

#include <cstdint>
#include <cstring>
#include <vector>

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
};

struct TextureDesc {
  uint32_t width;
  uint32_t height;
  TextureFormat format;
  TextureDesc() : width(1), height(1), format(TextureFormat::kRgba8) {}
};

struct DeviceDesc {
  void* native_window;
  uint32_t width;
  uint32_t height;
  DeviceDesc() : native_window(nullptr), width(0), height(0) {}
};

struct RenderPassDesc {
  float clear_r;
  float clear_g;
  float clear_b;
  float clear_a;
  uint32_t width;
  uint32_t height;
  RenderPassDesc()
      : clear_r(0),
        clear_g(0),
        clear_b(0),
        clear_a(1),
        width(0),
        height(0) {}
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

CameraMatrices make_ortho_camera(float left, float right, float bottom,
                                 float top, float near_z, float far_z);
CameraMatrices make_perspective_camera(float fov_y_radians, float aspect,
                                       float near_z, float far_z);

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

inline uint32_t texture_byte_size(const TextureDesc& desc) {
  const uint32_t bpp = (desc.format == TextureFormat::kRgba8) ? 4u : 4u;
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

  void set_viewport(float, float, float, float, float, float) override {}
  void begin_render_pass(const RenderPassDesc&) override { pass_open = true; }
  void end_render_pass() override { pass_open = false; }
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
};

Device* create_device(Backend backend);
Backend preferred_gpu_backend();

}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_RHI_H_
