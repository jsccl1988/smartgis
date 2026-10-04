// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi3d/impl/d3d/host/render_device.h"

#include <cstring>
#include <vector>

namespace scenic {
namespace detail {
namespace {

DXGI_FORMAT dxgi_from_texture_format(TextureFormat fmt) {
  switch (fmt) {
    case RGB8:
    case RGBA8:
      return DXGI_FORMAT_R8G8B8A8_UNORM;
    default:
      return DXGI_FORMAT_UNKNOWN;
  }
}

}  // namespace

Texture* D3dRenderDevice::CreateTexture(const char* szName) {
  if (!szName || !device_) {
    return nullptr;
  }
  const uint handle = alloc_texture_handle();
  gpu_textures_[handle] = D3dGpuTexture{};
  Texture* pTex = new Texture(this, handle, szName);
  if (kErrNone == m_textureMgr.AddTexture(pTex)) {
    return pTex;
  }
  gpu_textures_.erase(handle);
  delete pTex;
  return nullptr;
}

long D3dRenderDevice::DestroyTexture(const char* szName) {
  Texture* pTexture = m_textureMgr.GetTexture(szName);
  if (!pTexture) {
    return kErrInvalidParam;
  }
  const uint handle = pTexture->GetHandle();
  if (bound_texture_ == pTexture) {
    bound_texture_ = nullptr;
  }
  auto it = gpu_textures_.find(handle);
  if (it != gpu_textures_.end()) {
    release_gpu_texture(it->second);
    gpu_textures_.erase(it);
  }
  m_textureMgr.DestroyTexture(pTexture->GetTextureName());
  return kErrNone;
}

Texture* D3dRenderDevice::GetTexture(const char* szName) {
  return m_textureMgr.GetTexture(szName);
}

long D3dRenderDevice::GenerateMipmap(Texture* /*pTexture*/) {
  // v1: single-level textures only; succeed as no-op so callers proceed.
  return kErrNone;
}

long D3dRenderDevice::BindTexture(Texture* pTexture) {
  if (!pTexture) {
    return kErrInvalidParam;
  }
  bound_texture_ = pTexture;
  if (context_) {
    ID3D11ShaderResourceView* srv = texture_srv(pTexture->GetHandle());
    context_->PSSetShaderResources(0, 1, &srv);
    ID3D11SamplerState* samp = linear_sampler();
    if (samp) {
      context_->PSSetSamplers(0, 1, &samp);
    }
  }
  return kErrNone;
}

long D3dRenderDevice::BuildTexture(Texture* pTexture) {
  if (!pTexture || !device_) {
    return kErrInvalidParam;
  }
  void* pDataBuf = pTexture->GetData();
  if (!pDataBuf) {
    return kErrInvalidParam;
  }
  const TextureDesc texDesc = pTexture->GetDesc();
  const DXGI_FORMAT dxgi = dxgi_from_texture_format(texDesc.format);
  if (dxgi == DXGI_FORMAT_UNKNOWN || texDesc.width <= 0 ||
      texDesc.height <= 0) {
    return kErrFailure;
  }

  const int comps = pTexture->GetComponents(texDesc.format);
  if (comps != 3 && comps != 4) {
    return kErrFailure;
  }

  const UINT w = static_cast<UINT>(texDesc.width);
  const UINT h = static_cast<UINT>(texDesc.height);
  std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4u);
  const auto* src = static_cast<const unsigned char*>(pDataBuf);
  for (UINT y = 0; y < h; ++y) {
    for (UINT x = 0; x < w; ++x) {
      const size_t di = (static_cast<size_t>(y) * w + x) * 4u;
      const size_t si =
          (static_cast<size_t>(y) * w + x) * static_cast<size_t>(comps);
      rgba[di + 0] = src[si + 0];
      rgba[di + 1] = src[si + 1];
      rgba[di + 2] = src[si + 2];
      rgba[di + 3] = (comps == 4) ? src[si + 3] : 255;
    }
  }

  auto it = gpu_textures_.find(pTexture->GetHandle());
  if (it == gpu_textures_.end()) {
    return kErrFailure;
  }
  release_gpu_texture(it->second);

  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = w;
  desc.Height = h;
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = dxgi;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;

  D3D11_SUBRESOURCE_DATA init = {};
  init.pSysMem = rgba.data();
  init.SysMemPitch = w * 4u;

  if (FAILED(device_->CreateTexture2D(&desc, &init, &it->second.tex)) ||
      !it->second.tex) {
    return kErrFailure;
  }
  if (FAILED(device_->CreateShaderResourceView(it->second.tex, nullptr,
                                               &it->second.srv))) {
    release_gpu_texture(it->second);
    return kErrFailure;
  }
  it->second.format = dxgi;
  it->second.width = w;
  it->second.height = h;

  TextureSampler sampler;
  TextureEnvMode env;
  sampler.magFilter = LINEAR;
  sampler.minFilter = LINEAR;
  env.envMode = MODULATE;
  pTexture->SetSampler(sampler);
  pTexture->SetEnvMode(env);
  return kErrNone;
}

long D3dRenderDevice::BindRectTexture(Texture* pTexture) {
  return BindTexture(pTexture);
}

long D3dRenderDevice::UnbindTexture() {
  bound_texture_ = nullptr;
  if (context_) {
    ID3D11ShaderResourceView* null_srv = nullptr;
    context_->PSSetShaderResources(0, 1, &null_srv);
  }
  return kErrNone;
}

long D3dRenderDevice::UnbindRectTexture(void) { return UnbindTexture(); }

}  // namespace detail
}  // namespace scenic
