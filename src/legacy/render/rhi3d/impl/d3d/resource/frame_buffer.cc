// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/rhi3d/impl/d3d/host/render_device.h"

namespace render {

SmtFrameBuffer* SmtD3DRenderDevice::CreateFrameBuffer() {
  if (!device_) {
    return nullptr;
  }
  const uint handle = next_fbo_handle_++;
  gpu_fbos_[handle] = D3dGpuFbo{};
  return new SmtFrameBuffer(this, handle);
}

long SmtD3DRenderDevice::DestroyFrameBuffer(SmtFrameBuffer* frameBuffer) {
  if (!frameBuffer) {
    return SMT_ERR_INVALID_PARAM;
  }
  const uint handle = frameBuffer->GetHandle();
  if (bound_fbo_handle_ == handle) {
    UnbindFrameBuffer();
  }
  gpu_fbos_.erase(handle);
  delete frameBuffer;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::BindFrameBuffer(SmtFrameBuffer* frameBuffer) {
  if (!frameBuffer || !context_) {
    return SMT_ERR_INVALID_PARAM;
  }
  const uint handle = frameBuffer->GetHandle();
  auto it = gpu_fbos_.find(handle);
  if (it == gpu_fbos_.end()) {
    return SMT_ERR_FAILURE;
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
    return SMT_ERR_FAILURE;
  }
  context_->OMSetRenderTargets(1, &rtv, dsv);
  bound_fbo_handle_ = handle;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::UnbindFrameBuffer() {
  bound_fbo_handle_ = 0;
  if (context_ && rtv_) {
    context_->OMSetRenderTargets(1, &rtv_, dsv_);
  }
  return SMT_ERR_NONE;
}

SmtRenderBuffer* SmtD3DRenderDevice::CreateRenderBuffer(TextureFormat format,
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
  SmtRenderBuffer* rb =
      new SmtRenderBuffer(this, /*handle=*/0, format, width, height);
  gpu_renderbuffers_[rb] = gpu;
  return rb;
}

long SmtD3DRenderDevice::DestroyRenderBuffer(SmtRenderBuffer* renderBuffer) {
  if (!renderBuffer) {
    return SMT_ERR_INVALID_PARAM;
  }
  auto it = gpu_renderbuffers_.find(renderBuffer);
  if (it != gpu_renderbuffers_.end()) {
    release_gpu_texture(it->second);
    gpu_renderbuffers_.erase(it);
  }
  delete renderBuffer;
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::AttachRenderBuffer(SmtFrameBuffer* frameBuffer,
                                            SmtRenderBuffer* renderBuffer,
                                            RenderBufferSlot slot) {
  if (!frameBuffer || !renderBuffer) {
    return SMT_ERR_INVALID_PARAM;
  }
  auto it = gpu_fbos_.find(frameBuffer->GetHandle());
  if (it == gpu_fbos_.end()) {
    return SMT_ERR_FAILURE;
  }
  if (slot == DEPTH_ATTACHMENT || slot == STENCIL_ATTACHMENT) {
    it->second.depth_rb = renderBuffer;
    it->second.depth_tex_handle = 0;
  }
  return SMT_ERR_NONE;
}

long SmtD3DRenderDevice::AttachTexture(SmtFrameBuffer* frameBuffer,
                                       SmtTexture* texture2D,
                                       RenderBufferSlot slot) {
  if (!frameBuffer || !texture2D) {
    return SMT_ERR_INVALID_PARAM;
  }
  auto it = gpu_fbos_.find(frameBuffer->GetHandle());
  if (it == gpu_fbos_.end()) {
    return SMT_ERR_FAILURE;
  }
  const uint th = texture2D->GetHandle();
  if (slot == DEPTH_ATTACHMENT || slot == STENCIL_ATTACHMENT) {
    it->second.depth_tex_handle = th;
    it->second.depth_rb = nullptr;
  } else {
    it->second.color_tex_handle = th;
  }
  return SMT_ERR_NONE;
}

FrameBufferStatus SmtD3DRenderDevice::CheckFrameBufferStatus() {
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

}  // namespace render
