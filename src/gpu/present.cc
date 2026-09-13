// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/present.h"

#include <cstring>

#include <d3d11.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>

namespace gpu {
namespace detail {
namespace {

constexpr uint8_t kClearB = 0x40;
constexpr uint8_t kClearG = 0x80;
constexpr uint8_t kClearR = 0xC0;
constexpr uint8_t kClearA = 0xFF;

HANDLE dup_into(HANDLE local, HANDLE ui_process) {
  HANDLE remote = nullptr;
  if (!DuplicateHandle(GetCurrentProcess(), local, ui_process, &remote, 0,
                       FALSE, DUPLICATE_SAME_ACCESS)) {
    return nullptr;
  }
  return remote;
}

}  // namespace

PresentTarget::~PresentTarget() {
  release();
}

void PresentTarget::release() {
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

bool PresentTarget::create_dxgi(uint32_t w, uint32_t h, HANDLE ui_process) {
  ID3D11Device* dev = nullptr;
  ID3D11DeviceContext* ctx = nullptr;
  const HRESULT hr_dev = D3D11CreateDevice(
      nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
      D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION, &dev,
      nullptr, &ctx);
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

  IDXGIResource1* res = nullptr;
  HANDLE nt = nullptr;
  if (FAILED(tex->QueryInterface(__uuidof(IDXGIResource1),
                                 reinterpret_cast<void**>(&res))) ||
      !res ||
      FAILED(res->CreateSharedHandle(nullptr, DXGI_SHARED_RESOURCE_READ, nullptr,
                                     &nt)) ||
      !nt) {
    if (res) {
      res->Release();
    }
    tex->Release();
    dev->Release();
    ctx->Release();
    return false;
  }
  res->Release();

  HANDLE remote = dup_into(nt, ui_process);
  CloseHandle(nt);
  if (!remote) {
    tex->Release();
    dev->Release();
    ctx->Release();
    return false;
  }

  d3d_device_ = dev;
  d3d_context_ = ctx;
  d3d_texture_ = tex;
  local_handle_ = nullptr;
  mode_ = content::PresentMode::kSharedTexture;
  wire_.width_px = w;
  wire_.height_px = h;
  wire_.format = content::kDxgiBgraUnorm;
  wire_.nt_handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remote));
  wire_.present_mode = static_cast<uint32_t>(mode_);
  return true;
}

bool PresentTarget::create_dib(uint32_t w, uint32_t h, HANDLE ui_process) {
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
  if (!remote) {
    UnmapViewOfFile(bits);
    CloseHandle(mapping);
    return false;
  }
  local_handle_ = mapping;
  bits_ = bits;
  mode_ = content::PresentMode::kSoftwareDib;
  wire_.width_px = w;
  wire_.height_px = h;
  wire_.format = content::kDxgiBgraUnorm;
  wire_.nt_handle = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remote));
  wire_.present_mode = static_cast<uint32_t>(mode_);
  std::memset(bits_, 0, bytes);
  (void)kClearB;
  (void)kClearG;
  (void)kClearR;
  (void)kClearA;
  return true;
}

bool PresentTarget::resize(uint32_t width_px,
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

void PresentTarget::paint_clear(uint8_t b, uint8_t g, uint8_t r, uint8_t a) {
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

void PresentTarget::copy_from_hwnd(HWND hwnd) {
  if (!hwnd || !bits_) {
    return;
  }
  HDC src = GetDC(hwnd);
  if (!src) {
    return;
  }
  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = static_cast<LONG>(wire_.width_px);
  bi.bmiHeader.biHeight = -static_cast<LONG>(wire_.height_px);
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;
  GetDIBits(src, static_cast<HBITMAP>(GetCurrentObject(src, OBJ_BITMAP)), 0,
            wire_.height_px, nullptr, &bi, DIB_RGB_COLORS);
  // Window DC is not a memory DC; blit via compatible bitmap.
  HDC mem = CreateCompatibleDC(src);
  HBITMAP bmp =
      CreateCompatibleBitmap(src, static_cast<int>(wire_.width_px),
                             static_cast<int>(wire_.height_px));
  HGDIOBJ old = SelectObject(mem, bmp);
  BitBlt(mem, 0, 0, static_cast<int>(wire_.width_px),
         static_cast<int>(wire_.height_px), src, 0, 0, SRCCOPY);
  GetDIBits(mem, bmp, 0, wire_.height_px, bits_, &bi, DIB_RGB_COLORS);
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(hwnd, src);

  if (d3d_texture_ && d3d_context_ && bits_) {
    auto* tex = static_cast<ID3D11Texture2D*>(d3d_texture_);
    auto* ctx = static_cast<ID3D11DeviceContext*>(d3d_context_);
    ctx->UpdateSubresource(tex, 0, nullptr, bits_, wire_.width_px * 4, 0);
    ctx->Flush();
  }
}

}  // namespace detail
}  // namespace gpu
