// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_gdi_raster.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

#include "vista/component/map/shade/multiply.h"

#pragma comment(lib, "Msimg32.lib")

namespace content {
namespace detail {
namespace {

// Bilinear sample tightly packed RGBA8 → BGRA8 with continuous alpha.
// Avoids GDI HALFTONE (destroys A) and COLORONCOLOR nearest staircases.
void sample_rgba_to_bgra_bilinear(const uint8_t* rgba, int tw, int th, double u,
                                  double v, uint8_t out_bgra[4]) {
  if (!rgba || tw < 1 || th < 1 || !out_bgra) {
    out_bgra[0] = out_bgra[1] = out_bgra[2] = out_bgra[3] = 0;
    return;
  }
  u = (std::max)(0.0, (std::min)(1.0, u));
  v = (std::max)(0.0, (std::min)(1.0, v));
  const double fx = u * static_cast<double>(tw - 1);
  const double fy = v * static_cast<double>(th - 1);
  const int x0 = static_cast<int>(std::floor(fx));
  const int y0 = static_cast<int>(std::floor(fy));
  const int x1 = (std::min)(x0 + 1, tw - 1);
  const int y1 = (std::min)(y0 + 1, th - 1);
  const float tx = static_cast<float>(fx - x0);
  const float ty = static_cast<float>(fy - y0);
  const auto px = [&](int x, int y) -> const uint8_t* {
    return rgba +
           (static_cast<size_t>(y) * static_cast<size_t>(tw) +
            static_cast<size_t>(x)) *
               4u;
  };
  const uint8_t* p00 = px(x0, y0);
  const uint8_t* p10 = px(x1, y0);
  const uint8_t* p01 = px(x0, y1);
  const uint8_t* p11 = px(x1, y1);
  float c[4];
  for (int k = 0; k < 4; ++k) {
    const float a = static_cast<float>(p00[k]) * (1.f - tx) +
                    static_cast<float>(p10[k]) * tx;
    const float b = static_cast<float>(p01[k]) * (1.f - tx) +
                    static_cast<float>(p11[k]) * tx;
    c[k] = a * (1.f - ty) + b * ty;
  }
  // RGBA → BGRA; keep soft alpha (jet ocean / isoline edges).
  out_bgra[0] =
      static_cast<uint8_t>((std::min)(255.f, c[2] + 0.5f));
  out_bgra[1] =
      static_cast<uint8_t>((std::min)(255.f, c[1] + 0.5f));
  out_bgra[2] =
      static_cast<uint8_t>((std::min)(255.f, c[0] + 0.5f));
  out_bgra[3] =
      static_cast<uint8_t>((std::min)(255.f, c[3] + 0.5f));
}

}  // namespace

bool try_bind_dib(HDC hdc, DibSurface* out) {
  if (!hdc || !out) {
    return false;
  }
  const HBITMAP bmp = static_cast<HBITMAP>(GetCurrentObject(hdc, OBJ_BITMAP));
  if (!bmp) {
    return false;
  }
  DIBSECTION ds{};
  if (GetObjectW(bmp, sizeof(ds), &ds) <
      static_cast<int>(sizeof(DIBSECTION))) {
    return false;
  }
  if (!ds.dsBm.bmBits || ds.dsBm.bmBitsPixel != 32 || ds.dsBm.bmWidth <= 0 ||
      ds.dsBm.bmHeight == 0 || ds.dsBm.bmWidthBytes < 4) {
    return false;
  }
  out->pixels = static_cast<uint32_t*>(ds.dsBm.bmBits);
  out->width = ds.dsBm.bmWidth;
  out->height = std::abs(ds.dsBm.bmHeight);
  out->stride_px = ds.dsBm.bmWidthBytes / 4;
  // CreateDIBSection(..., biHeight=-H) stores top-down bits. GetObjectW on
  // some stacks reports dsBmih.biHeight=+H while the buffer stays top-down —
  // trusting that positive sign inverted DibSurface::row (map2d_frame_gdi_test
  // red quad landed at y=19..59 instead of 300..340). Map2d paint/export HDCs
  // are always created top-down (see Map2dSoftwarePainter::export_bmp /
  // ensure_present_cache_dib); do not bind bottom-up DIBs here.
  out->top_down = true;
  return out->valid();
}

void fill_dib_solid(DibSurface* dib, uint32_t bgra) {
  if (!dib || !dib->valid()) {
    return;
  }
  const size_t width = static_cast<size_t>(dib->width);
  for (int y = 0; y < dib->height; ++y) {
    uint32_t* row = dib->row(y);
    std::fill(row, row + width, bgra);
  }
}

bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity, vista::DrawBlend blend) {
  if (!hdc || pts.size() < 4 || tw <= 0 || th <= 0 ||
      rgba.size() < static_cast<size_t>(tw) * static_cast<size_t>(th) * 4u) {
    return false;
  }
  LONG min_x = pts[0].x;
  LONG max_x = pts[0].x;
  LONG min_y = pts[0].y;
  LONG max_y = pts[0].y;
  for (const POINT& p : pts) {
    min_x = (std::min)(min_x, p.x);
    max_x = (std::max)(max_x, p.x);
    min_y = (std::min)(min_y, p.y);
    max_y = (std::max)(max_y, p.y);
  }
  const int full_w = static_cast<int>(max_x - min_x);
  const int full_h = static_cast<int>(max_y - min_y);
  if (full_w <= 0 || full_h <= 0) {
    return false;
  }

  // Clip the DEM AABB to the HDC bitmap. Zoomed views project china_dem to a
  // multi-viewport AABB; allocating that full DIB fails and hillshade vanishes.
  int clip_l = 0;
  int clip_t = 0;
  int clip_r = full_w;
  int clip_b = full_h;
  DibSurface host{};
  if (try_bind_dib(hdc, &host)) {
    clip_l = (std::max)(0, static_cast<int>(-min_x));
    clip_t = (std::max)(0, static_cast<int>(-min_y));
    clip_r = (std::min)(full_w, static_cast<int>(host.width - min_x));
    clip_b = (std::min)(full_h, static_cast<int>(host.height - min_y));
  } else {
    RECT cr{};
    if (GetClipBox(hdc, &cr) != ERROR && cr.right > cr.left &&
        cr.bottom > cr.top) {
      clip_l = (std::max)(0, static_cast<int>(cr.left - min_x));
      clip_t = (std::max)(0, static_cast<int>(cr.top - min_y));
      clip_r = (std::min)(full_w, static_cast<int>(cr.right - min_x));
      clip_b = (std::min)(full_h, static_cast<int>(cr.bottom - min_y));
    }
  }
  const int dst_w = clip_r - clip_l;
  const int dst_h = clip_b - clip_t;
  if (dst_w <= 0 || dst_h <= 0) {
    return false;
  }
  const int dest_x = static_cast<int>(min_x) + clip_l;
  const int dest_y = static_cast<int>(min_y) + clip_t;

  // UV window for the clipped dest (full DEM AABB → [0,1]²).
  const double u0 = static_cast<double>(clip_l) / static_cast<double>(full_w);
  const double v0 = static_cast<double>(clip_t) / static_cast<double>(full_h);
  const double u1 = static_cast<double>(clip_r) / static_cast<double>(full_w);
  const double v1 = static_cast<double>(clip_b) / static_cast<double>(full_h);

  const float k = (std::max)(0.f, (std::min)(1.f, opacity));

  HDC mem = CreateCompatibleDC(hdc);
  if (!mem) {
    return false;
  }
  BITMAPINFO dbmi{};
  dbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  dbmi.bmiHeader.biWidth = dst_w;
  dbmi.bmiHeader.biHeight = -dst_h;  // top-down
  dbmi.bmiHeader.biPlanes = 1;
  dbmi.bmiHeader.biBitCount = 32;
  dbmi.bmiHeader.biCompression = BI_RGB;
  void* dest_bits = nullptr;
  HBITMAP dest_dib =
      CreateDIBSection(mem, &dbmi, DIB_RGB_COLORS, &dest_bits, nullptr, 0);
  if (!dest_dib || !dest_bits) {
    if (dest_dib) {
      DeleteObject(dest_dib);
    }
    DeleteDC(mem);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dest_dib);
  // Snapshot the land/water already painted under this quad.
  if (!BitBlt(mem, 0, 0, dst_w, dst_h, hdc, dest_x, dest_y, SRCCOPY)) {
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }

  // Viewport-sized bilinear resample (keeps alpha). GDI HALFTONE zeros A on
  // 32bpp; COLORONCOLOR nearest makes jet bands / isolines staircase. Full
  // unclipped 2k×2k per-pixel was ~2s/frame — clip keeps this ~viewport cost.
  auto* dest = static_cast<uint8_t*>(dest_bits);
  BITMAPINFO cbmi = dbmi;
  void* cov_bits = nullptr;
  HBITMAP cov_dib =
      CreateDIBSection(mem, &cbmi, DIB_RGB_COLORS, &cov_bits, nullptr, 0);
  if (!cov_dib || !cov_bits) {
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }
  auto* coverage = static_cast<uint8_t*>(cov_bits);
  const double du = (u1 - u0) / static_cast<double>(dst_w);
  const double dv = (v1 - v0) / static_cast<double>(dst_h);
  for (int dy = 0; dy < dst_h; ++dy) {
    const double v = v0 + (static_cast<double>(dy) + 0.5) * dv;
    uint8_t* row =
        coverage + static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      const double u = u0 + (static_cast<double>(dx) + 0.5) * du;
      sample_rgba_to_bgra_bilinear(rgba.data(), tw, th, u, v, row + dx * 4);
    }
  }

  BOOL ok = FALSE;
  if (blend == vista::DrawBlend::kMultiply) {
    const size_t cov_bytes =
        static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u;
    vista::apply_multiply_coverage(std::span<uint8_t>(coverage, cov_bytes), k);
    for (int dy = 0; dy < dst_h; ++dy) {
      for (int dx = 0; dx < dst_w; ++dx) {
        const size_t o = (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
                          static_cast<size_t>(dx)) *
                         4u;
        if (coverage[o + 3] == 0) {
          // Outside DEM footprint: bare cream land (#f5f3e9) reads as a
          // white "missing tile" next to shaded terrain (north plateau /
          // coastal fringe). Soft ambient multiply matches mid hillshade.
          const unsigned b = dest[o + 0];
          const unsigned g = dest[o + 1];
          const unsigned r = dest[o + 2];
          const bool oceanish = (b > r + 15u && g > r);
          const bool land_cream =
              (r > 200u && g > 190u && b > 170u && !oceanish);
          if (land_cream) {
            // Match mid hillshade luma (~0.55–0.65 of cream) so DEM nodata
            // / Korea-fringe land does not read as a missing-tile slab.
            constexpr float kAmbient = 0.58f;
            dest[o + 0] =
                static_cast<uint8_t>(static_cast<float>(b) * kAmbient + 0.5f);
            dest[o + 1] =
                static_cast<uint8_t>(static_cast<float>(g) * kAmbient + 0.5f);
            dest[o + 2] =
                static_cast<uint8_t>(static_cast<float>(r) * kAmbient + 0.5f);
          }
          continue;
        }
        const float m = static_cast<float>(coverage[o + 1]) / 255.f;
        // Dest DIB is BGRA. Coverage RGB is the multiply factor.
        dest[o + 0] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(dest[o + 0]) * m + 0.5f));
        dest[o + 1] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(dest[o + 1]) * m + 0.5f));
        dest[o + 2] = static_cast<uint8_t>(
            (std::min)(255.f, static_cast<float>(dest[o + 2]) * m + 0.5f));
      }
    }
    ok = BitBlt(hdc, dest_x, dest_y, dst_w, dst_h, mem, 0, 0, SRCCOPY);
  } else {
    for (int dy = 0; dy < dst_h; ++dy) {
      for (int dx = 0; dx < dst_w; ++dx) {
        const size_t o = (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
                          static_cast<size_t>(dx)) *
                         4u;
        const float af = (static_cast<float>(coverage[o + 3]) / 255.f) * k;
        const auto au =
            static_cast<uint8_t>((std::min)(255.f, af * 255.f + 0.5f));
        dest[o + 0] = static_cast<uint8_t>(
            (static_cast<unsigned>(coverage[o + 0]) * au) / 255u);
        dest[o + 1] = static_cast<uint8_t>(
            (static_cast<unsigned>(coverage[o + 1]) * au) / 255u);
        dest[o + 2] = static_cast<uint8_t>(
            (static_cast<unsigned>(coverage[o + 2]) * au) / 255u);
        dest[o + 3] = au;
      }
    }
    BLENDFUNCTION bf{};
    bf.BlendOp = AC_SRC_OVER;
    bf.BlendFlags = 0;
    bf.SourceConstantAlpha = 255;
    bf.AlphaFormat = AC_SRC_ALPHA;
    ok = AlphaBlend(hdc, dest_x, dest_y, dst_w, dst_h, mem, 0, 0, dst_w, dst_h,
                    bf);
  }
  SelectObject(mem, old);
  DeleteObject(cov_dib);
  DeleteObject(dest_dib);
  DeleteDC(mem);
  return ok != FALSE;
}

}  // namespace detail
}  // namespace content
