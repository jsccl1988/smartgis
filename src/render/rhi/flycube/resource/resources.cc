// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/device.h"
#include "render/rhi/flycube/resource/resources.h"

#include <cstring>
#include <memory>
#include <utility>

namespace render {
namespace rhi {
namespace detail {

#ifdef SMT_HAS_FLYCUBE

Buffer* FlycubeDevice::create_buffer(uint32_t byte_size, BufferUsage usage) {
    if (byte_size == 0) {
      return nullptr;
    }
    if (!fc_device_) {
      return make_stub_buffer(byte_size, usage);
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

void FlycubeDevice::destroy_buffer(Buffer* buffer) { delete buffer; }

bool FlycubeDevice::upload(Buffer* buffer, const void* data, uint32_t byte_size) {
    if (!buffer || !data) {
      return false;
    }
    if (auto* gpu = as_gpu_buffer(buffer)) {
      if (byte_size > gpu->byte_size() || !gpu->resource()) {
        return false;
      }
      gpu->resource()->UpdateUploadBuffer(0, data, byte_size);
      return true;
    }
    return upload_stub_buffer(buffer, data, byte_size);
  }

Texture* FlycubeDevice::create_texture(const TextureDesc& desc) {
    if (desc.width == 0 || desc.height == 0) {
      return nullptr;
    }
    if (!fc_device_) {
      return make_stub_texture(desc);
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
                              texture_byte_size(desc), desc.format);
  }

void FlycubeDevice::destroy_texture(Texture* texture) { delete texture; }

bool FlycubeDevice::upload_texture(Texture* texture, const void* data,
                      uint32_t byte_size) {
    if (!texture || !data) {
      return false;
    }
    auto* gpu = as_gpu_texture(texture);
    if (!gpu || !fc_device_ || !command_queue_ || !fence_) {
      return upload_stub_texture(texture, data, byte_size);
    }
    if (byte_size > gpu->byte_size() || !gpu->resource()) {
      return false;
    }
    return upload_gpu_texture(gpu, data, byte_size);
  }

void FlycubeDevice::wait_for_idle() {
    if (!command_queue_ || !fence_) {
      return;
    }
    command_queue_->Signal(fence_, ++fence_value_);
    fence_->Wait(fence_value_);
  }

bool FlycubeDevice::upload_gpu_texture(GpuTexture* gpu, const void* data,
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

bool FlycubeDevice::ensure_depth_buffer(uint32_t w, uint32_t h) {
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

#endif  // SMT_HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
