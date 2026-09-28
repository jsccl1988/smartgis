// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef RENDER_RHI_FLYCUBE_RESOURCE_GPU_H_
#define RENDER_RHI_FLYCUBE_RESOURCE_GPU_H_

#ifdef SMT_HAS_FLYCUBE
#include "Resource/Resource.h"
#include "View/View.h"

#include <cstdint>
#include <memory>
#include <utility>
#endif

namespace render {
namespace rhi {
namespace detail {

#ifdef SMT_HAS_FLYCUBE

// Upload-heap buffer. No RHI facade type.
class GpuBuffer {
 public:
  GpuBuffer(std::shared_ptr<Resource> resource, uint32_t size)
      : resource_(std::move(resource)), size_(size) {}

  uint32_t byte_size() const { return size_; }
  Resource* resource() const { return resource_.get(); }
  std::shared_ptr<Resource> shared() const { return resource_; }

 private:
  std::shared_ptr<Resource> resource_;
  uint32_t size_ = 0;
};

// GPU texture plus shader views. No RHI facade type.
class GpuTexture {
 public:
  GpuTexture(std::shared_ptr<Resource> resource, std::shared_ptr<View> srv,
             std::shared_ptr<View> uav, uint32_t width, uint32_t height,
             uint32_t bytes)
      : resource_(std::move(resource)),
        srv_(std::move(srv)),
        uav_(std::move(uav)),
        w_(width),
        h_(height),
        bytes_(bytes) {}

  uint32_t width() const { return w_; }
  uint32_t height() const { return h_; }
  uint32_t byte_size() const { return bytes_; }
  Resource* resource() const { return resource_.get(); }
  std::shared_ptr<Resource> shared() const { return resource_; }
  std::shared_ptr<View> srv() const { return srv_; }
  std::shared_ptr<View> uav() const { return uav_; }

 private:
  std::shared_ptr<Resource> resource_;
  std::shared_ptr<View> srv_;
  std::shared_ptr<View> uav_;
  uint32_t w_ = 0;
  uint32_t h_ = 0;
  uint32_t bytes_ = 0;
};

#endif  // SMT_HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render

#endif  // RENDER_RHI_FLYCUBE_RESOURCE_GPU_H_
