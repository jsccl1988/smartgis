// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Open a DXGI NT shared texture (from OutputSurface D3D11) on the FlyCube DX12
// device and present into it — either by GPU compose (execute_to_imported) or
// by CopyBufferToTexture of a BGRA staging upload.

#include "render/rhi/flycube/device.h"

#ifdef SMT_HAS_FLYCUBE

#include "Device/DXDevice.h"
#include "Resource/DXTexture.h"
#include "Utilities/FormatHelper.h"

#include <cstring>
#include <vector>
#include <directx/d3d12.h>

#endif

namespace render {
namespace rhi {
namespace detail {

#ifdef SMT_HAS_FLYCUBE

void FlycubeDevice::clear_imported_shared() {
  imported_rtv_.reset();
  imported_shared_.reset();
  imported_nt_handle_ = nullptr;
  composed_into_imported_ = false;
}

bool FlycubeDevice::has_imported_shared() const {
  return imported_shared_ != nullptr && imported_rtv_ != nullptr;
}

bool FlycubeDevice::composed_into_imported_shared() const {
  return composed_into_imported_;
}

bool FlycubeDevice::import_shared_nt_handle(void* nt_handle, uint32_t width_px,
                                            uint32_t height_px) {
  if (!nt_handle || width_px == 0 || height_px == 0 || !fc_device_ ||
      backend_ != Backend::kDx12) {
    clear_imported_shared();
    return false;
  }
  if (imported_nt_handle_ == nt_handle && width_ == width_px &&
      height_ == height_px && has_imported_shared()) {
    return true;
  }
  clear_imported_shared();

  auto* dx = dynamic_cast<DXDevice*>(fc_device_.get());
  if (!dx || !dx->GetDevice()) {
    return false;
  }

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  const HRESULT hr = dx->GetDevice()->OpenSharedHandle(
      static_cast<HANDLE>(nt_handle), IID_PPV_ARGS(&resource));
  if (FAILED(hr) || !resource) {
    return false;
  }

  // OutputSurface DXGI path is B8G8R8A8_UNORM. Wrap like a swapchain buffer
  // then force Common initial state (cross-API shared is not DXGI Present).
  auto tex = DXTexture::WrapSwapchainBackBuffer(
      *dx, resource, gli::FORMAT_BGRA8_UNORM_PACK8);
  if (!tex) {
    return false;
  }
  tex->SetInitialState(ResourceState::kCommon);

  ViewDesc view_desc = {
      .view_type = ViewType::kRenderTarget,
      .dimension = ViewDimension::kTexture2D,
  };
  auto rtv = fc_device_->CreateView(tex, view_desc);
  if (!rtv) {
    return false;
  }

  imported_shared_ = tex;
  imported_rtv_ = std::move(rtv);
  imported_nt_handle_ = nt_handle;
  width_ = width_px;
  height_ = height_px;
  return true;
}

bool FlycubeDevice::copy_bgra_to_imported_shared(const uint8_t* bgra,
                                                 uint32_t stride_bytes,
                                                 uint32_t width_px,
                                                 uint32_t height_px) {
  if (!has_imported_shared() || !bgra || width_px == 0 || height_px == 0 ||
      !fc_device_ || !command_queue_ || !fence_) {
    return false;
  }
  if (width_px != width_ || height_px != height_) {
    return false;
  }
  if (stride_bytes < width_px * 4u) {
    return false;
  }

  size_t num_bytes = 0;
  size_t row_bytes = 0;
  size_t num_rows = 0;
  GetFormatInfo(width_px, height_px, gli::FORMAT_BGRA8_UNORM_PACK8, num_bytes,
                row_bytes, num_rows);
  const uint64_t aligned_row =
      Align(row_bytes, fc_device_->GetTextureDataPitchAlignment());
  const uint64_t buffer_size = aligned_row * num_rows;

  // Pack tightly or with source stride into a contiguous staging image.
  std::vector<uint8_t> packed(static_cast<size_t>(row_bytes) * num_rows);
  for (uint32_t y = 0; y < height_px; ++y) {
    const uint8_t* src = bgra + static_cast<size_t>(y) * stride_bytes;
    uint8_t* dst = packed.data() + static_cast<size_t>(y) * row_bytes;
    std::memcpy(dst, src, row_bytes);
  }

  ::BufferDesc upload_desc;
  upload_desc.size = buffer_size;
  upload_desc.usage = BindFlag::kCopySource;
  auto upload = fc_device_->CreateBuffer(MemoryType::kUpload, upload_desc);
  if (!upload) {
    return false;
  }
  BufferTextureCopyRegion copy_region = {
      .buffer_offset = 0,
      .buffer_row_pitch = static_cast<uint32_t>(aligned_row),
      .texture_mip_level = 0,
      .texture_array_layer = 0,
      .texture_extent = {.width = width_px, .height = height_px, .depth = 1},
  };
  upload->UpdateUploadBufferWithTextureData(
      0, aligned_row, buffer_size, packed.data(), row_bytes,
      row_bytes * num_rows, row_bytes, static_cast<uint32_t>(num_rows), 1);

  wait_for_idle();
  auto fc_list = fc_device_->CreateCommandList(::CommandListType::kGraphics);
  if (!fc_list) {
    return false;
  }
  fc_list->Reset();
  fc_list->ResourceBarrier({{imported_shared_, ResourceState::kCommon,
                             ResourceState::kCopyDest}});
  fc_list->CopyBufferToTexture(upload, imported_shared_, {copy_region});
  fc_list->ResourceBarrier({{imported_shared_, ResourceState::kCopyDest,
                             ResourceState::kCommon}});
  fc_list->Close();
  command_queue_->ExecuteCommandLists({fc_list});
  wait_for_idle();
  return true;
}

#endif  // SMT_HAS_FLYCUBE

}  // namespace detail
}  // namespace rhi
}  // namespace render
