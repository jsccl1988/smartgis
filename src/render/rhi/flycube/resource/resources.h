// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_RESOURCE_RESOURCES_H_
#define RENDER_RHI_FLYCUBE_RESOURCE_RESOURCES_H_

#include "render/rhi/flycube/resource/gpu.h"
#include "render/rhi/rhi.h"

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

// Facade buffer. The FlyCube object is GpuBuffer.
class FlycubeBuffer : public Buffer {
 public:
  FlycubeBuffer(std::shared_ptr<Resource> resource, uint32_t size)
      : gpu_(std::move(resource), size) {}

  uint32_t byte_size() const override { return gpu_.byte_size(); }
  GpuBuffer& gpu() { return gpu_; }

 private:
  GpuBuffer gpu_;
};

// Facade texture. Format stays on the facade; pixels live in GpuTexture.
class FlycubeTexture : public Texture {
 public:
  FlycubeTexture(std::shared_ptr<Resource> resource, std::shared_ptr<View> srv,
                 std::shared_ptr<View> uav, uint32_t width, uint32_t height,
                 uint32_t bytes, TextureFormat format)
      : gpu_(std::move(resource), std::move(srv), std::move(uav), width, height,
             bytes),
        format_(format) {}

  uint32_t width() const override { return gpu_.width(); }
  uint32_t height() const override { return gpu_.height(); }
  uint32_t byte_size() const override { return gpu_.byte_size(); }
  TextureFormat format() const { return format_; }
  GpuTexture& gpu() { return gpu_; }

 private:
  GpuTexture gpu_;
  TextureFormat format_ = TextureFormat::kRgba8;
};

inline GpuBuffer* as_gpu_buffer(Buffer* buffer) {
  auto* owned = dynamic_cast<FlycubeBuffer*>(buffer);
  return owned ? &owned->gpu() : nullptr;
}

inline GpuTexture* as_gpu_texture(Texture* texture) {
  auto* owned = dynamic_cast<FlycubeTexture*>(texture);
  return owned ? &owned->gpu() : nullptr;
}

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_RESOURCE_RESOURCES_H_
