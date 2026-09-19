// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/present.h"

#include "gis/world/dem_raster.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

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

  // Keep the local NT share handle for Channel attachment fallback. Prefer
  // pickle nt_handle when DuplicateHandle into the UI process succeeds.
  HANDLE remote = dup_into(nt, ui_process);
  d3d_device_ = dev;
  d3d_context_ = ctx;
  d3d_texture_ = tex;
  local_handle_ = nt;
  mode_ = content::PresentMode::kSharedTexture;
  wire_.width_px = w;
  wire_.height_px = h;
  wire_.format = content::kDxgiBgraUnorm;
  wire_.nt_handle =
      remote ? static_cast<uint64_t>(reinterpret_cast<uintptr_t>(remote)) : 0;
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

bool PresentTarget::paint_bgra(const uint8_t* bgra, uint32_t stride_bytes) {
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

bool PresentTarget::copy_bgra(uint8_t* dst, size_t dst_bytes) const {
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

void PresentTarget::paint_demo_frame(content::ViewKind kind) {
  if (!bits_ || wire_.width_px < 8 || wire_.height_px < 8) {
    return;
  }
  const int w = static_cast<int>(wire_.width_px);
  const int h = static_cast<int>(wire_.height_px);

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  HDC screen = GetDC(nullptr);
  if (!screen) {
    return;
  }
  HDC mem = CreateCompatibleDC(screen);
  void* dib_bits = nullptr;
  HBITMAP bmp =
      CreateDIBSection(mem, &bi, DIB_RGB_COLORS, &dib_bits, nullptr, 0);
  if (!mem || !bmp || !dib_bits) {
    if (bmp) {
      DeleteObject(bmp);
    }
    if (mem) {
      DeleteDC(mem);
    }
    ReleaseDC(nullptr, screen);
    return;
  }

  const size_t bytes =
      static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  std::memcpy(dib_bits, bits_, bytes);
  HGDIOBJ old = SelectObject(mem, bmp);

  const COLORREF grid =
      (kind == content::ViewKind::kScene3d)
          ? RGB(180, 120, 80)
          : (kind == content::ViewKind::kMapData) ? RGB(90, 180, 120)
                                                 : RGB(120, 180, 220);
  const COLORREF feature = RGB(255, 200, 120);
  const COLORREF ink = RGB(235, 245, 255);

  HPEN grid_pen = CreatePen(PS_SOLID, 1, grid);
  HGDIOBJ old_pen = SelectObject(mem, grid_pen);
  const int step = (w < 200 || h < 200) ? 24 : 48;
  for (int x = step; x < w; x += step) {
    MoveToEx(mem, x, 0, nullptr);
    LineTo(mem, x, h);
  }
  for (int y = step; y < h; y += step) {
    MoveToEx(mem, 0, y, nullptr);
    LineTo(mem, w, y);
  }
  SelectObject(mem, old_pen);
  DeleteObject(grid_pen);

  // 2D panes: light grid only (chrome overlays MapScene vectors).
  // Scene3d: synthetic DEM wireframe (no GDAL in the GPU process — sample
  // raster load here raced chrome and faulted Views --self-test).
  if (kind == content::ViewKind::kScene3d) {
    gis::DemRaster dem;
    dem.fill_synthetic_china();
    std::vector<float> xyz;
    std::vector<uint32_t> indices;
    if (dem.build_mesh(32, &xyz, &indices) && xyz.size() >= 9 &&
        indices.size() >= 3) {
      float minx = xyz[0], maxx = xyz[0];
      float miny = xyz[1], maxy = xyz[1];
      float minz = xyz[2], maxz = xyz[2];
      for (size_t i = 0; i + 2 < xyz.size(); i += 3) {
        minx = (std::min)(minx, xyz[i]);
        maxx = (std::max)(maxx, xyz[i]);
        miny = (std::min)(miny, xyz[i + 1]);
        maxy = (std::max)(maxy, xyz[i + 1]);
        minz = (std::min)(minz, xyz[i + 2]);
        maxz = (std::max)(maxz, xyz[i + 2]);
      }
      const float cx = 0.5f * (minx + maxx);
      const float cy = 0.5f * (miny + maxy);
      const float cz = 0.5f * (minz + maxz);
      const float span =
          (std::max)(maxx - minx, (std::max)(maxz - minz, 1.f));
      const float s = 3.2f / span;
      constexpr float kYaw = 0.55f;
      constexpr float kPitch = 0.4f;
      constexpr float kDist = 3.2f;
      const float cyaw = std::cos(kYaw);
      const float syaw = std::sin(kYaw);
      const float cp = std::cos(kPitch);
      const float sp = std::sin(kPitch);
      auto project = [&](float x, float y, float z, int* sx, int* sy) {
        x = (x - cx) * s;
        y = (y - cy) * s;
        z = (z - cz) * s;
        const float x1 = x * cyaw - z * syaw;
        const float z1 = x * syaw + z * cyaw;
        const float y2 = y * cp - z1 * sp;
        const float z2 = y * sp + z1 * cp;
        const float depth = z2 + kDist;
        const float inv = depth > 0.15f ? (1.f / depth) : (1.f / 0.15f);
        const float f = 280.f * inv;
        if (sx) {
          *sx = w / 2 + static_cast<int>(std::lround(x1 * f));
        }
        if (sy) {
          *sy = h / 2 - static_cast<int>(std::lround(y2 * f));
        }
      };
      HPEN feat_pen = CreatePen(PS_SOLID, 1, RGB(140, 190, 120));
      old_pen = SelectObject(mem, feat_pen);
      const size_t ntri =
          (std::min)(indices.size() / 3, static_cast<size_t>(400));
      for (size_t t = 0; t < ntri; ++t) {
        const uint32_t i0 = indices[t * 3];
        const uint32_t i1 = indices[t * 3 + 1];
        const uint32_t i2 = indices[t * 3 + 2];
        if ((i0 + 1) * 3 > xyz.size() || (i1 + 1) * 3 > xyz.size() ||
            (i2 + 1) * 3 > xyz.size()) {
          continue;
        }
        int p0[2] = {};
        int p1[2] = {};
        int p2[2] = {};
        project(xyz[i0 * 3], xyz[i0 * 3 + 1], xyz[i0 * 3 + 2], &p0[0],
                &p0[1]);
        project(xyz[i1 * 3], xyz[i1 * 3 + 1], xyz[i1 * 3 + 2], &p1[0],
                &p1[1]);
        project(xyz[i2 * 3], xyz[i2 * 3 + 1], xyz[i2 * 3 + 2], &p2[0],
                &p2[1]);
        MoveToEx(mem, p0[0], p0[1], nullptr);
        LineTo(mem, p1[0], p1[1]);
        LineTo(mem, p2[0], p2[1]);
        LineTo(mem, p0[0], p0[1]);
      }
      SelectObject(mem, old_pen);
      DeleteObject(feat_pen);
    } else {
      HPEN feat_pen = CreatePen(PS_SOLID, 2, feature);
      old_pen = SelectObject(mem, feat_pen);
      const int midx = w / 2;
      const int midy = h / 2;
      const int s = (w < h ? w : h) / 5;
      Rectangle(mem, midx - s, midy - s, midx + s, midy + s);
      SelectObject(mem, old_pen);
      DeleteObject(feat_pen);
    }
  }

  SetBkMode(mem, TRANSPARENT);
  SetTextColor(mem, ink);
  const wchar_t* title =
      (kind == content::ViewKind::kScene3d)
          ? L"3D DEM"
          : (kind == content::ViewKind::kMapData) ? L"Datasource"
                                                 : L"Map";
  TextOutW(mem, 12, 12, title, lstrlenW(title));

  std::memcpy(bits_, dib_bits, bytes);
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);

  if (d3d_texture_ && d3d_context_ && bits_) {
    auto* tex = static_cast<ID3D11Texture2D*>(d3d_texture_);
    auto* ctx = static_cast<ID3D11DeviceContext*>(d3d_context_);
    ctx->UpdateSubresource(tex, 0, nullptr, bits_, wire_.width_px * 4, 0);
    ctx->Flush();
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
