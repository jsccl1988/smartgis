// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_gdi_raster.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <span>
#include <vector>

#include "vista/component/map/multiply.h"

#pragma comment(lib, "Msimg32.lib")

namespace content {
namespace detail {

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
  out->top_down = ds.dsBmih.biHeight < 0;
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
  const int dst_w = static_cast<int>(max_x - min_x);
  const int dst_h = static_cast<int>(max_y - min_y);
  if (dst_w <= 0 || dst_h <= 0) {
    return false;
  }

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
  if (!BitBlt(mem, 0, 0, dst_w, dst_h, hdc, static_cast<int>(min_x),
              static_cast<int>(min_y), SRCCOPY)) {
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }

  // GDI StretchDIBits for coverage (nearest). Per-pixel sample of a 2k
  // viewport was ~2s/frame (gdi other_us) and froze 2D gestures.
  auto* dest = static_cast<uint8_t*>(dest_bits);
  BITMAPINFO sbmi{};
  sbmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  sbmi.bmiHeader.biWidth = tw;
  sbmi.bmiHeader.biHeight = -th;
  sbmi.bmiHeader.biPlanes = 1;
  sbmi.bmiHeader.biBitCount = 32;
  sbmi.bmiHeader.biCompression = BI_RGB;
  void* src_bits = nullptr;
  HBITMAP src_dib =
      CreateDIBSection(mem, &sbmi, DIB_RGB_COLORS, &src_bits, nullptr, 0);
  if (!src_dib || !src_bits) {
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }
  auto* src = static_cast<uint8_t*>(src_bits);
  for (int y = 0; y < th; ++y) {
    for (int x = 0; x < tw; ++x) {
      const size_t so = (static_cast<size_t>(y) * static_cast<size_t>(tw) +
                         static_cast<size_t>(x)) *
                        4u;
      src[so + 0] = rgba[so + 2];
      src[so + 1] = rgba[so + 1];
      src[so + 2] = rgba[so + 0];
      src[so + 3] = rgba[so + 3] < 160u ? 0 : 255;
    }
  }
  BITMAPINFO cbmi = dbmi;
  void* cov_bits = nullptr;
  HBITMAP cov_dib =
      CreateDIBSection(mem, &cbmi, DIB_RGB_COLORS, &cov_bits, nullptr, 0);
  if (!cov_dib || !cov_bits) {
    DeleteObject(src_dib);
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }
  HDC cov_dc = CreateCompatibleDC(mem);
  HDC src_dc = CreateCompatibleDC(mem);
  if (!cov_dc || !src_dc) {
    if (cov_dc) {
      DeleteDC(cov_dc);
    }
    if (src_dc) {
      DeleteDC(src_dc);
    }
    DeleteObject(cov_dib);
    DeleteObject(src_dib);
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }
  HGDIOBJ old_cov = SelectObject(cov_dc, cov_dib);
  HGDIOBJ old_src = SelectObject(src_dc, src_dib);
  SetStretchBltMode(cov_dc, COLORONCOLOR);
  StretchBlt(cov_dc, 0, 0, dst_w, dst_h, src_dc, 0, 0, tw, th, SRCCOPY);
  SelectObject(src_dc, old_src);
  SelectObject(cov_dc, old_cov);
  DeleteDC(src_dc);
  DeleteDC(cov_dc);
  DeleteObject(src_dib);
  auto* coverage = static_cast<uint8_t*>(cov_bits);

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
    ok = BitBlt(hdc, static_cast<int>(min_x), static_cast<int>(min_y), dst_w,
                dst_h, mem, 0, 0, SRCCOPY);
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
    ok = AlphaBlend(hdc, static_cast<int>(min_x), static_cast<int>(min_y),
                    dst_w, dst_h, mem, 0, 0, dst_w, dst_h, bf);
  }
  SelectObject(mem, old);
  DeleteObject(cov_dib);
  DeleteObject(dest_dib);
  DeleteDC(mem);
  return ok != FALSE;
}

}  // namespace detail
}  // namespace content
