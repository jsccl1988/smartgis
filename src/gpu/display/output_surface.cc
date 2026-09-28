// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/display/output_surface.h"

#include <cstring>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

#include "gpu/device/gpu_device_hub.h"

namespace gpu {
namespace detail {
namespace {

constexpr uint8_t kClearB = 0x40;
constexpr uint8_t kClearG = 0x80;
constexpr uint8_t kClearR = 0xC0;
constexpr uint8_t kClearA = 0xFF;

HANDLE dup_into(HANDLE local, HANDLE ui_process) {
  HANDLE remote = nullptr;
  if (!ui_process || !local || local == INVALID_HANDLE_VALUE) {
    return nullptr;
  }
  if (!DuplicateHandle(GetCurrentProcess(), local, ui_process, &remote, 0,
                       FALSE, DUPLICATE_SAME_ACCESS)) {
    return nullptr;
  }
  return remote;
}

}  // namespace

OutputSurface::~OutputSurface() {
  device_hub().unbind_surface(this);
  release();
}

void OutputSurface::bump_generation() {
  wire_.generation += 1;
  if (wire_.generation == 0) {
    wire_.generation = 1;
  }
}

void OutputSurface::release() {
  if (bits_ && local_handle_) {
    UnmapViewOfFile(bits_);
    bits_ = nullptr;
  }
  if (d3d_texture_) {
    static_cast<ID3D11Texture2D*>(d3d_texture_)->Release();
    d3d_texture_ = nullptr;
  }
  if (d3d_context_) {
    static_cast<ID3D11DeviceContext*>(d3d_context_)->Release();
    d3d_context_ = nullptr;
  }
  if (d3d_device_) {
    static_cast<ID3D11Device*>(d3d_device_)->Release();
    d3d_device_ = nullptr;
  }
  if (local_handle_) {
    CloseHandle(local_handle_);
    local_handle_ = nullptr;
  }
  wire_ = {};
}

bool OutputSurface::create_dxgi(uint32_t w, uint32_t h, HANDLE ui_process) {
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;

  // Prefer the pinned AdapterId (multi-GPU). Fall back to default hardware.
  IDXGIFactory1* factory = nullptr;
  IDXGIAdapter* chosen = nullptr;
  if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1),
                                   reinterpret_cast<void**>(&factory))) &&
      factory) {
    const AdapterId want =
        (adapter_id_ == kAdapterInvalid) ? kAdapterPrimary : adapter_id_;
    IDXGIAdapter1* adapter1 = nullptr;
    if (SUCCEEDED(factory->EnumAdapters1(static_cast<UINT>(want), &adapter1)) &&
        adapter1) {
      chosen = adapter1;
    }
  }

  HRESULT hr_dev = E_FAIL;
  if (chosen) {
    hr_dev = D3D11CreateDevice(
        chosen, D3D_DRIVER_TYPE_UNKNOWN, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &dev,
        nullptr, &ctx);
  }
  if (FAILED(hr_dev) || !dev) {
    if (dev) {
      dev->Release();
      dev = nullptr;
    }
    if (ctx) {
      ctx->Release();
      ctx = nullptr;
    }
    hr_dev = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &dev,
        nullptr, &ctx);
  }
  if (chosen) {
    chosen->Release();
  }
  if (factory) {
    factory->Release();
  }
  if (FAILED(hr_dev) || !dev) {
    if (dev) {
      dev->Release();
    }
    if (ctx) {
      ctx->Release();
    }
    return false;
  }

  D3D11_TEXTURE2D_DESC td = {};
  td.Width = w;
  td.Height = h;
  td.MipLevels = 1;
  td.ArraySize = 1;
  td.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
  td.SampleDesc.Count = 1;
  td.Usage = D3D11_USAGE_DEFAULT;
  td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
  td.MiscFlags =
      D3D11_RESOURCE_MISC_SHARED | D3D11_RESOURCE_MISC_SHARED_NTHANDLE;

  ID3D11Texture2D* tex = nullptr;
  if (FAILED(dev->CreateTexture2D(&td, nullptr, &tex)) || !tex) {
    dev->Release();
    ctx->Release();
    return false;
  }

  HANDLE shared = nullptr;
  IDXGIResource1* res1 = nullptr;
  if (SUCCEEDED(tex->QueryInterface(__uuidof(IDXGIResource1),
                                    reinterpret_cast<void**>(&res1))) &&
      res1) {
    if (FAILED(res1->CreateSharedHandle(
            nullptr,
            DXGI_SHARED_RESOURCE_READ | DXGI_SHARED_RESOURCE_WRITE, nullptr,
            &shared)) ||
        !shared) {
      shared = nullptr;
    }
    res1->Release();
  }
  // Older DXGI path (non-NT): still yields a share handle for smoke / DIB peers.
  if (!shared) {
    IDXGIResource* res0 = nullptr;
    if (SUCCEEDED(tex->QueryInterface(__uuidof(IDXGIResource),
                                      reinterpret_cast<void**>(&res0))) &&
        res0) {
      if (FAILED(res0->GetSharedHandle(&shared)) || !shared) {
        shared = nullptr;
      }
      res0->Release();
    }
  }
  if (!shared) {
    tex->Release();
    dev->Release();
    ctx->Release();
    return false;
  }

  // Keep the local share handle for Channel attachment fallback. Prefer
  // pickle nt_handle when DuplicateHandle into the UI process succeeds.
  HANDLE remote = dup_into(shared, ui_process);
  d3d_device_ = dev;
  d3d_context_ = ctx;
  d3d_texture_ = tex;
  local_handle_ = shared;
  mode_ = content::PresentMode::kSharedTexture;
  wire_.width_px = w;
  wire_.height_px = h;
  wire_.format = content::kDxgiBgraUnorm;
  wire_.nt_handle =
      remote ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remote)) : 0;
  wire_.present_mode = static_cast<uint32_t>(mode_);
  return true;
}

bool OutputSurface::create_dib(uint32_t w, uint32_t h, HANDLE ui_process) {
  const uint32_t bytes = w * h * 4;
  HANDLE mapping =
      CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0,
                         bytes, nullptr);
  if (!mapping) {
    return false;
  }
  void* bits = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, bytes);
  if (!bits) {
    CloseHandle(mapping);
    return false;
  }
  HANDLE remote = dup_into(mapping, ui_process);
  local_handle_ = mapping;
  bits_ = bits;
  mode_ = content::PresentMode::kSoftwareDib;
  wire_.width_px = w;
  wire_.height_px = h;
  wire_.format = content::kDxgiBgraUnorm;
  wire_.nt_handle =
      remote ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remote)) : 0;
  wire_.present_mode = static_cast<uint32_t>(mode_);
  // Seed with the map-edit clear color — never publish a black flash before
  // announce_and_paint runs.
  auto* px = static_cast<uint8_t*>(bits_);
  const uint32_t n = w * h;
  for (uint32_t i = 0; i < n; ++i) {
    px[i * 4 + 0] = kClearB;
    px[i * 4 + 1] = kClearG;
    px[i * 4 + 2] = kClearR;
    px[i * 4 + 3] = kClearA;
  }
  return true;
}

bool OutputSurface::resize(uint32_t width_px,
                           uint32_t height_px,
                           content::PresentMode requested,
                           HANDLE ui_process) {
  if (width_px < 1) {
    width_px = 1;
  }
  if (height_px < 1) {
    height_px = 1;
  }
  release();
  bool ok = false;
  if (requested == content::PresentMode::kSharedTexture) {
    ok = create_dxgi(width_px, height_px, ui_process);
  }
  if (!ok) {
    ok = create_dib(width_px, height_px, ui_process);
  }
  if (ok) {
    wire_.generation += 1;
    if (wire_.generation == 0) {
      wire_.generation = 1;
    }
  }
  return ok;
}

bool OutputSurface::upload_bgra(const uint8_t* bgra, uint32_t stride_bytes) {
  if (!bgra || wire_.width_px == 0 || wire_.height_px == 0) {
    return false;
  }
  const uint32_t w = wire_.width_px;
  const uint32_t h = wire_.height_px;
  if (stride_bytes < w * 4) {
    return false;
  }
  if (bits_) {
    auto* dst = static_cast<uint8_t*>(bits_);
    if (stride_bytes == w * 4) {
      std::memcpy(dst, bgra, static_cast<size_t>(w) * h * 4u);
    } else {
      for (uint32_t y = 0; y < h; ++y) {
        std::memcpy(dst + static_cast<size_t>(y) * w * 4u,
                    bgra + static_cast<size_t>(y) * stride_bytes, w * 4u);
      }
    }
  }
  if (d3d_texture_ && d3d_context_) {
    auto* tex = static_cast<ID3D11Texture2D*>(d3d_texture_);
    auto* ctx = static_cast<ID3D11DeviceContext*>(d3d_context_);
    ctx->UpdateSubresource(tex, 0, nullptr, bgra, stride_bytes, 0);
    ctx->Flush();
  }
  return bits_ != nullptr || d3d_texture_ != nullptr;
}

bool OutputSurface::copy_bgra(uint8_t* dst, size_t dst_bytes) const {
  if (!dst || !bits_ || wire_.width_px == 0 || wire_.height_px == 0) {
    return false;
  }
  const size_t n =
      static_cast<size_t>(wire_.width_px) * wire_.height_px * 4u;
  if (dst_bytes < n) {
    return false;
  }
  std::memcpy(dst, bits_, n);
  return true;
}

void OutputSurface::clear(uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
  if (d3d_texture_ && d3d_device_ && d3d_context_) {
    auto* tex = static_cast<ID3D11Texture2D*>(d3d_texture_);
    auto* dev = static_cast<ID3D11Device*>(d3d_device_);
    auto* ctx = static_cast<ID3D11DeviceContext*>(d3d_context_);
    ID3D11RenderTargetView* rtv = nullptr;
    if (SUCCEEDED(dev->CreateRenderTargetView(tex, nullptr, &rtv)) && rtv) {
      const float c[4] = {r / 255.f, g / 255.f, b / 255.f, a / 255.f};
      ctx->ClearRenderTargetView(rtv, c);
      ctx->Flush();
      rtv->Release();
    }
  }
  if (bits_) {
    const uint32_t n = wire_.width_px * wire_.height_px;
    auto* px = static_cast<uint8_t*>(bits_);
    for (uint32_t i = 0; i < n; ++i) {
      px[i * 4 + 0] = b;
      px[i * 4 + 1] = g;
      px[i * 4 + 2] = r;
      px[i * 4 + 3] = a;
    }
  }
}

}  // namespace detail
}  // namespace gpu
