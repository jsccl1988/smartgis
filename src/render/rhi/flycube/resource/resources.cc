// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/rhi/flycube/device.h"
#include "render/rhi/flycube/resource/resources.h"

#include "base/core/log.h"

#include <cstring>
#include <memory>
#include <utility>
#include <vector>

namespace render {
namespace rhi {
namespace detail {

#ifdef HAS_FLYCUBE

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
    const uint32_t w = gpu->width();
    const uint32_t h = gpu->height();
    if (!data || w == 0 || h == 0) {
      return false;
    }
    // Tight RGBA8 source pitch. Do not ask GetFormatInfo to pre-align — that
    // would make src_row_pitch larger than the caller's tightly packed buffer
    // and shear every row into horizontal streaks on sample.
    const size_t src_row = static_cast<size_t>(w) * 4u;
    const size_t src_bytes = src_row * static_cast<size_t>(h);
    if (byte_size < src_bytes) {
      return false;
    }
    const uint64_t aligned_row =
        Align(src_row, fc_device_->GetTextureDataPitchAlignment());
    const uint64_t buffer_size = aligned_row * static_cast<uint64_t>(h);
    ::BufferDesc upload_desc;
    upload_desc.size = buffer_size;
    upload_desc.usage = BindFlag::kCopySource;
    auto upload = fc_device_->CreateBuffer(MemoryType::kUpload, upload_desc);
    if (!upload) {
      return false;
    }
    BufferTextureCopyRegion region = {};
    region.buffer_offset = 0;
    region.buffer_row_pitch = static_cast<uint32_t>(aligned_row);
    region.texture_mip_level = 0;
    region.texture_array_layer = 0;
    region.texture_offset = {};
    region.texture_extent = {.width = w, .height = h, .depth = 1};
    // Manually pad rows into the upload heap. A mismatched src_row_pitch
    // shears RGBA8 hillshade into horizontal gray bands on sample.
    std::vector<uint8_t> staged(static_cast<size_t>(buffer_size), 0);
    const auto* src = static_cast<const uint8_t*>(data);
    for (uint32_t y = 0; y < h; ++y) {
      std::memcpy(staged.data() + static_cast<size_t>(y) * aligned_row,
                  src + static_cast<size_t>(y) * src_row, src_row);
    }
    upload->UpdateUploadBuffer(0, staged.data(), buffer_size);
    static int s_pad_logs = 0;
    if (s_pad_logs < 3) {
      LOGGING(LOG_INFO,
              "rhi.flycube upload_gpu_texture padded %ux%u src_row=%zu "
              "aligned_row=%llu px0=%02x%02x%02x%02x",
              w, h, src_row, static_cast<unsigned long long>(aligned_row),
              src[0], src[1], src[2], src[3]);
      ++s_pad_logs;
    }
    wait_for_idle();
    auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
    if (!fc_list) {
      return false;
    }
    fc_list->Reset();
    // Re-uploads (china LOD remesh / sat_cloud refresh) leave the texture in
    // AllShaderResource — never assume COMMON or D3D12 device-removes.
    gpu->barrier_to(fc_list.get(), ResourceState::kCopyDest);
    fc_list->CopyBufferToTexture(upload, gpu->shared(), {region});
    gpu->barrier_to(fc_list.get(), ResourceState::kAllShaderResource);
    fc_list->Close();
    command_queue_->ExecuteCommandLists({fc_list});
    wait_for_idle();
    return true;
  }

bool FlycubeDevice::ensure_depth_buffer(uint32_t w, uint32_t h) {
    if (!fc_device_ || w == 0 || h == 0) {
      return false;
    }
    if (depth_texture_ && depth_view_ && depth_sample_facade_ &&
        depth_w_ == w && depth_h_ == h) {
      return true;
    }
    clear_depth_sample_facade();
    depth_view_.reset();
    depth_srv_.reset();
    depth_texture_.reset();
    depth_state_ = ResourceState::kCommon;
    ::TextureDesc td = {
        .type = TextureType::k2D,
        .format = gli::format::FORMAT_D32_SFLOAT_PACK32,
        .width = w,
        .height = h,
        .depth_or_array_layers = 1,
        .mip_levels = 1,
        .sample_count = 1,
        // DSV for opaque passes + SRV for post fog / soft particles.
        .usage = BindFlag::kDepthStencil | BindFlag::kShaderResource,
    };
    depth_texture_ = fc_device_->CreateTexture(MemoryType::kDefault, td);
    if (!depth_texture_) {
      // Some adapters reject DSV|SRV on D32 at large sizes. Opaque Scene3d
      // only needs a DSV; fog SRV is optional.
      td.usage = BindFlag::kDepthStencil;
      depth_texture_ = fc_device_->CreateTexture(MemoryType::kDefault, td);
    }
    if (!depth_texture_) {
      LOGGING(LOG_ERROR, "rhi.flycube ensure_depth_buffer CreateTexture fail "
                         "%ux%u",
              w, h);
      return false;
    }
    ViewDesc vd = {
        .view_type = ViewType::kDepthStencil,
        .dimension = ViewDimension::kTexture2D,
    };
    depth_view_ = fc_device_->CreateView(depth_texture_, vd);
    ViewDesc srv_desc = {
        .view_type = ViewType::kTexture,
        .dimension = ViewDimension::kTexture2D,
    };
    depth_srv_ = fc_device_->CreateView(depth_texture_, srv_desc);
    if (!depth_view_) {
      LOGGING(LOG_ERROR, "rhi.flycube ensure_depth_buffer CreateView DSV fail "
                         "%ux%u",
              w, h);
      clear_depth_sample_facade();
      depth_view_.reset();
      depth_srv_.reset();
      depth_texture_.reset();
      return false;
    }
    // SRV may be null when usage is DSV-only — fog sampling falls back.
    if (!depth_srv_) {
      LOGGING(LOG_WARNING,
              "rhi.flycube ensure_depth_buffer DSV-only (no SRV) %ux%u", w, h);
    }
    const uint32_t bytes = w * h * 4u;
    depth_sample_facade_ =
        new FlycubeTexture(depth_texture_, depth_srv_, std::shared_ptr<View>{},
                           w, h, bytes, TextureFormat::kD32Float);
    depth_w_ = w;
    depth_h_ = h;
    return true;
  }

#endif  // HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
