// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"

namespace scenic {
namespace detail {

FrameBuffer* D3dRenderDevice::CreateFrameBuffer() {
  if (!device_) {
    return nullptr;
  }
  const uint handle = next_fbo_handle_++;
  gpu_fbos_[handle] = D3dGpuFbo{};
  return new FrameBuffer(this, handle);
}

long D3dRenderDevice::DestroyFrameBuffer(FrameBuffer* frameBuffer) {
  if (!frameBuffer) {
    return kErrInvalidParam;
  }
  const uint handle = frameBuffer->GetHandle();
  if (bound_fbo_handle_ == handle) {
    UnbindFrameBuffer();
  }
  gpu_fbos_.erase(handle);
  delete frameBuffer;
  return kErrNone;
}

long D3dRenderDevice::BindFrameBuffer(FrameBuffer* frameBuffer) {
  if (!frameBuffer || !context_) {
    return kErrInvalidParam;
  }
  const uint handle = frameBuffer->GetHandle();
  auto it = gpu_fbos_.find(handle);
  if (it == gpu_fbos_.end()) {
    return kErrFailure;
  }
  ID3D11RenderTargetView* rtv = nullptr;
  ID3D11DepthStencilView* dsv = nullptr;
  if (it->second.color_tex_handle) {
    auto tex = gpu_textures_.find(it->second.color_tex_handle);
    if (tex != gpu_textures_.end()) {
      if (!tex->second.rtv && tex->second.tex && device_) {
        device_->CreateRenderTargetView(tex->second.tex, nullptr,
                                        &tex->second.rtv);
      }
      rtv = tex->second.rtv;
    }
  }
  if (it->second.depth_tex_handle) {
    auto tex = gpu_textures_.find(it->second.depth_tex_handle);
    if (tex != gpu_textures_.end()) {
      dsv = tex->second.dsv;
    }
  } else if (it->second.depth_rb) {
    auto rb = gpu_renderbuffers_.find(it->second.depth_rb);
    if (rb != gpu_renderbuffers_.end()) {
      dsv = rb->second.dsv;
    }
  }
  if (!rtv) {
    return kErrFailure;
  }
  context_->OMSetRenderTargets(1, &rtv, dsv);
  bound_fbo_handle_ = handle;
  return kErrNone;
}

long D3dRenderDevice::UnbindFrameBuffer() {
  bound_fbo_handle_ = 0;
  if (context_ && rtv_) {
    context_->OMSetRenderTargets(1, &rtv_, dsv_);
  }
  return kErrNone;
}

RenderBuffer* D3dRenderDevice::CreateRenderBuffer(TextureFormat format,
                                                        uint width,
                                                        uint height) {
  if (!device_ || width == 0 || height == 0) {
    return nullptr;
  }
  D3dGpuTexture gpu;
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = width;
  desc.Height = height;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  if (format == RGB8 || format == RGBA8) {
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  } else {
    // Depth / unknown → D24S8 depth surface.
    desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
  }
  if (FAILED(device_->CreateTexture2D(&desc, nullptr, &gpu.tex)) || !gpu.tex) {
    return nullptr;
  }
  if (desc.BindFlags & D3D11_BIND_DEPTH_STENCIL) {
    if (FAILED(device_->CreateDepthStencilView(gpu.tex, nullptr, &gpu.dsv))) {
      release_gpu_texture(gpu);
      return nullptr;
    }
  } else if (FAILED(
                 device_->CreateRenderTargetView(gpu.tex, nullptr, &gpu.rtv))) {
    release_gpu_texture(gpu);
    return nullptr;
  }
  gpu.format = desc.Format;
  gpu.width = width;
  gpu.height = height;
  // Constructor historically stores height in m_unHandle; track by pointer.
  RenderBuffer* rb =
      new RenderBuffer(this, /*handle=*/0, format, width, height);
  gpu_renderbuffers_[rb] = gpu;
  return rb;
}

long D3dRenderDevice::DestroyRenderBuffer(RenderBuffer* renderBuffer) {
  if (!renderBuffer) {
    return kErrInvalidParam;
  }
  auto it = gpu_renderbuffers_.find(renderBuffer);
  if (it != gpu_renderbuffers_.end()) {
    release_gpu_texture(it->second);
    gpu_renderbuffers_.erase(it);
  }
  delete renderBuffer;
  return kErrNone;
}

long D3dRenderDevice::AttachRenderBuffer(FrameBuffer* frameBuffer,
                                            RenderBuffer* renderBuffer,
                                            RenderBufferSlot slot) {
  if (!frameBuffer || !renderBuffer) {
    return kErrInvalidParam;
  }
  auto it = gpu_fbos_.find(frameBuffer->GetHandle());
  if (it == gpu_fbos_.end()) {
    return kErrFailure;
  }
  if (slot == DEPTH_ATTACHMENT || slot == STENCIL_ATTACHMENT) {
    it->second.depth_rb = renderBuffer;
    it->second.depth_tex_handle = 0;
  }
  return kErrNone;
}

long D3dRenderDevice::AttachTexture(FrameBuffer* frameBuffer,
                                       Texture* texture2D,
                                       RenderBufferSlot slot) {
  if (!frameBuffer || !texture2D) {
    return kErrInvalidParam;
  }
  auto it = gpu_fbos_.find(frameBuffer->GetHandle());
  if (it == gpu_fbos_.end()) {
    return kErrFailure;
  }
  const uint th = texture2D->GetHandle();
  if (slot == DEPTH_ATTACHMENT || slot == STENCIL_ATTACHMENT) {
    it->second.depth_tex_handle = th;
    it->second.depth_rb = nullptr;
  } else {
    it->second.color_tex_handle = th;
  }
  return kErrNone;
}

FrameBufferStatus D3dRenderDevice::CheckFrameBufferStatus() {
  if (bound_fbo_handle_ == 0) {
    return FRAMEBUFFER_COMPLETE;
  }
  auto it = gpu_fbos_.find(bound_fbo_handle_);
  if (it == gpu_fbos_.end() || it->second.color_tex_handle == 0) {
    return FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT;
  }
  auto tex = gpu_textures_.find(it->second.color_tex_handle);
  if (tex == gpu_textures_.end() || !tex->second.tex) {
    return FRAMEBUFFER_INCOMPLETE_ATTACHMENT;
  }
  return FRAMEBUFFER_COMPLETE;
}

}  // namespace detail
}  // namespace scenic
