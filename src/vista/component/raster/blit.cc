// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/raster/blit.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "vista/component/map/shade/multiply.h"

#pragma comment(lib, "Msimg32.lib")

namespace vista {
namespace raster {
namespace {

#if defined(_MSC_VER)
#define VISTA_RASTER_BLIT_INLINE __forceinline
#else
#define VISTA_RASTER_BLIT_INLINE inline
#endif

constexpr int kParallelBlitMinRows = 8;
constexpr int kParallelBlitMinPixels = 4096;

// Bilinear sample tightly packed RGBA8 → BGRA8 with continuous alpha.
// Avoids GDI HALFTONE (destroys A) and COLORONCOLOR nearest staircases.
VISTA_RASTER_BLIT_INLINE void sample_rgba_to_bgra_bilinear(
    const uint8_t* rgba, int tw, int th, double u, double v,
    uint8_t out_bgra[4]) {
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

// In-place kMultiply into a bound 32bpp DIB — skips two CreateDIBSection +
// BitBlt round-trips. Workers write a private coverage buffer only; the host
// DIB is updated on the calling thread (GDI bitmaps are not worker-safe).
bool blit_multiply_into_host(DibSurface* host, int dest_x, int dest_y,
                             int dst_w, int dst_h, const uint8_t* rgba, int tw,
                             int th, double u0, double v0, double u1,
                             double v1, float opacity) {
  if (!host || !host->valid() || !rgba || dst_w <= 0 || dst_h <= 0) {
    return false;
  }
  if (dest_x < 0 || dest_y < 0 || dest_x + dst_w > host->width ||
      dest_y + dst_h > host->height) {
    return false;
  }
  const double du = (u1 - u0) / static_cast<double>(dst_w);
  const double dv = (v1 - v0) / static_cast<double>(dst_h);
  uint32_t* pixels = host->pixels;
  const int stride = host->stride_px;
  GdiFlush();
  const int twm = tw - 1;
  const int thm = th - 1;
  const size_t cov_bytes =
      static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u;
  std::vector<uint8_t> coverage(cov_bytes);

  // Resample DEM RGBA into a private buffer (RGBA layout for
  // apply_multiply_coverage AVX2). Parallel by row when the clip is large.
  auto resample_row = [&](int dy) {
    double v = v0 + (static_cast<double>(dy) + 0.5) * dv;
    v = (std::max)(0.0, (std::min)(1.0, v));
    const double fy = v * static_cast<double>(thm);
    const int y0 = static_cast<int>(fy);
    const int y1 = (std::min)(y0 + 1, thm);
    const float ty = static_cast<float>(fy - y0);
    const float ity = 1.f - ty;
    const uint8_t* row0 =
        rgba + static_cast<size_t>(y0) * static_cast<size_t>(tw) * 4u;
    const uint8_t* row1 =
        rgba + static_cast<size_t>(y1) * static_cast<size_t>(tw) * 4u;
    uint8_t* out =
        coverage.data() +
        static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      double u = u0 + (static_cast<double>(dx) + 0.5) * du;
      u = (std::max)(0.0, (std::min)(1.0, u));
      const double fx = u * static_cast<double>(twm);
      const int x0 = static_cast<int>(fx);
      const int x1 = (std::min)(x0 + 1, twm);
      const float tx = static_cast<float>(fx - x0);
      const float itx = 1.f - tx;
      const uint8_t* p00 = row0 + x0 * 4;
      const uint8_t* p10 = row0 + x1 * 4;
      const uint8_t* p01 = row1 + x0 * 4;
      const uint8_t* p11 = row1 + x1 * 4;
      float c[4];
      for (int k = 0; k < 4; ++k) {
        const float a = static_cast<float>(p00[k]) * itx +
                        static_cast<float>(p10[k]) * tx;
        const float b = static_cast<float>(p01[k]) * itx +
                        static_cast<float>(p11[k]) * tx;
        c[k] = a * ity + b * ty;
      }
      // Keep RGBA order for apply_multiply_coverage (not BGRA).
      out[dx * 4 + 0] =
          static_cast<uint8_t>((std::min)(255.f, c[0] + 0.5f));
      out[dx * 4 + 1] =
          static_cast<uint8_t>((std::min)(255.f, c[1] + 0.5f));
      out[dx * 4 + 2] =
          static_cast<uint8_t>((std::min)(255.f, c[2] + 0.5f));
      out[dx * 4 + 3] =
          static_cast<uint8_t>((std::min)(255.f, c[3] + 0.5f));
    }
  };

  const int pixels_n = dst_w * dst_h;
  if (dst_h >= kParallelBlitMinRows &&
      pixels_n >= kParallelBlitMinPixels) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, dst_h, resample_row);
  } else {
    for (int dy = 0; dy < dst_h; ++dy) {
      resample_row(dy);
    }
  }

  vista::apply_multiply_coverage(
      std::span<uint8_t>(coverage.data(), cov_bytes), opacity);

  // Serial host write: factor is in every RGB channel after multiply bake.
  for (int dy = 0; dy < dst_h; ++dy) {
    uint8_t* dest_row = reinterpret_cast<uint8_t*>(
        pixels + static_cast<size_t>(dest_y + dy) * static_cast<size_t>(stride));
    uint8_t* px = dest_row + static_cast<size_t>(dest_x) * 4u;
    const uint8_t* cov =
        coverage.data() +
        static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      const size_t o = static_cast<size_t>(dx) * 4u;
      if (cov[o + 3] == 0) {
        const unsigned b = px[o + 0];
        const unsigned g = px[o + 1];
        const unsigned r = px[o + 2];
        const bool oceanish = (b > r + 15u && g > r);
        const bool land_cream =
            (r > 200u && g > 190u && b > 170u && !oceanish);
        if (land_cream) {
          constexpr float kAmbient = 0.58f;
          px[o + 0] =
              static_cast<uint8_t>(static_cast<float>(b) * kAmbient + 0.5f);
          px[o + 1] =
              static_cast<uint8_t>(static_cast<float>(g) * kAmbient + 0.5f);
          px[o + 2] =
              static_cast<uint8_t>(static_cast<float>(r) * kAmbient + 0.5f);
        }
        continue;
      }
      const float m = static_cast<float>(cov[o + 1]) / 255.f;
      px[o + 0] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(px[o + 0]) * m + 0.5f));
      px[o + 1] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(px[o + 1]) * m + 0.5f));
      px[o + 2] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(px[o + 2]) * m + 0.5f));
    }
  }
  return true;
}

// In-place SRC_OVER into a bound 32bpp DIB (jet hillshade / kOver). Same
// private-buffer parallel resample as multiply; host write stays serial.
bool blit_over_into_host(DibSurface* host, int dest_x, int dest_y, int dst_w,
                         int dst_h, const uint8_t* rgba, int tw, int th,
                         double u0, double v0, double u1, double v1,
                         float opacity) {
  if (!host || !host->valid() || !rgba || dst_w <= 0 || dst_h <= 0) {
    return false;
  }
  if (dest_x < 0 || dest_y < 0 || dest_x + dst_w > host->width ||
      dest_y + dst_h > host->height) {
    return false;
  }
  const double du = (u1 - u0) / static_cast<double>(dst_w);
  const double dv = (v1 - v0) / static_cast<double>(dst_h);
  uint32_t* pixels = host->pixels;
  const int stride = host->stride_px;
  GdiFlush();
  const size_t cov_bytes =
      static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u;
  std::vector<uint8_t> coverage(cov_bytes);

  auto resample_row = [&](int dy) {
    const double v = v0 + (static_cast<double>(dy) + 0.5) * dv;
    uint8_t* row =
        coverage.data() +
        static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      const double u = u0 + (static_cast<double>(dx) + 0.5) * du;
      sample_rgba_to_bgra_bilinear(rgba, tw, th, u, v, row + dx * 4);
    }
  };

  if (dst_h >= kParallelBlitMinRows &&
      dst_w * dst_h >= kParallelBlitMinPixels) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, dst_h, resample_row);
  } else {
    for (int dy = 0; dy < dst_h; ++dy) {
      resample_row(dy);
    }
  }

  for (int dy = 0; dy < dst_h; ++dy) {
    uint8_t* dest_row = reinterpret_cast<uint8_t*>(
        pixels + static_cast<size_t>(dest_y + dy) * static_cast<size_t>(stride));
    uint8_t* px = dest_row + static_cast<size_t>(dest_x) * 4u;
    const uint8_t* cov =
        coverage.data() +
        static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    for (int dx = 0; dx < dst_w; ++dx) {
      const size_t o = static_cast<size_t>(dx) * 4u;
      const float af = (static_cast<float>(cov[o + 3]) / 255.f) * opacity;
      if (af <= 0.f) {
        continue;
      }
      if (af >= 1.f) {
        px[o + 0] = cov[o + 0];
        px[o + 1] = cov[o + 1];
        px[o + 2] = cov[o + 2];
        continue;
      }
      const int a = static_cast<int>(af * 255.f + 0.5f);
      const int ia = 255 - a;
      px[o + 0] = static_cast<uint8_t>(
          (static_cast<int>(cov[o + 0]) * a + static_cast<int>(px[o + 0]) * ia) /
          255);
      px[o + 1] = static_cast<uint8_t>(
          (static_cast<int>(cov[o + 1]) * a + static_cast<int>(px[o + 1]) * ia) /
          255);
      px[o + 2] = static_cast<uint8_t>(
          (static_cast<int>(cov[o + 2]) * a + static_cast<int>(px[o + 2]) * ia) /
          255);
    }
  }
  return true;
}

}  // namespace

bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity, DrawBlend blend) {
  if (tw <= 0 || th <= 0 ||
      rgba.size() < static_cast<size_t>(tw) * static_cast<size_t>(th) * 4u) {
    return false;
  }
  return blit_rgba_quad(hdc, pts, rgba.data(), tw, th, opacity, blend);
}

bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts, const uint8_t* rgba,
                    int tw, int th, float opacity, DrawBlend blend) {
  if (!hdc || !rgba || pts.size() < 4 || tw <= 0 || th <= 0) {
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

  if (host.valid() && blend == DrawBlend::kMultiply &&
      blit_multiply_into_host(&host, dest_x, dest_y, dst_w, dst_h, rgba, tw,
                              th, u0, v0, u1, v1, k)) {
    return true;
  }
  if (host.valid() && blend == DrawBlend::kOver &&
      blit_over_into_host(&host, dest_x, dest_y, dst_w, dst_h, rgba, tw, th,
                          u0, v0, u1, v1, k)) {
    return true;
  }

  // Fallback when the HDC is not a bound 32bpp DIB:
  // private coverage buffer + parallel resample, then BitBlt / AlphaBlend.
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
  if (!BitBlt(mem, 0, 0, dst_w, dst_h, hdc, dest_x, dest_y, SRCCOPY)) {
    SelectObject(mem, old);
    DeleteObject(dest_dib);
    DeleteDC(mem);
    return false;
  }

  auto* dest = static_cast<uint8_t*>(dest_bits);
  const size_t cov_bytes =
      static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u;
  std::vector<uint8_t> coverage(cov_bytes);
  const double du = (u1 - u0) / static_cast<double>(dst_w);
  const double dv = (v1 - v0) / static_cast<double>(dst_h);
  const int twm = tw - 1;
  const int thm = th - 1;

  auto resample_row = [&](int dy) {
    double v = v0 + (static_cast<double>(dy) + 0.5) * dv;
    v = (std::max)(0.0, (std::min)(1.0, v));
    uint8_t* row =
        coverage.data() +
        static_cast<size_t>(dy) * static_cast<size_t>(dst_w) * 4u;
    if (blend == DrawBlend::kMultiply) {
      // RGBA for apply_multiply_coverage (same as in-place host path).
      const double fy = v * static_cast<double>(thm);
      const int y0 = static_cast<int>(fy);
      const int y1 = (std::min)(y0 + 1, thm);
      const float ty = static_cast<float>(fy - y0);
      const float ity = 1.f - ty;
      const uint8_t* row0 =
          rgba + static_cast<size_t>(y0) * static_cast<size_t>(tw) * 4u;
      const uint8_t* row1 =
          rgba + static_cast<size_t>(y1) * static_cast<size_t>(tw) * 4u;
      for (int dx = 0; dx < dst_w; ++dx) {
        double u = u0 + (static_cast<double>(dx) + 0.5) * du;
        u = (std::max)(0.0, (std::min)(1.0, u));
        const double fx = u * static_cast<double>(twm);
        const int x0 = static_cast<int>(fx);
        const int x1 = (std::min)(x0 + 1, twm);
        const float tx = static_cast<float>(fx - x0);
        const float itx = 1.f - tx;
        const uint8_t* p00 = row0 + x0 * 4;
        const uint8_t* p10 = row0 + x1 * 4;
        const uint8_t* p01 = row1 + x0 * 4;
        const uint8_t* p11 = row1 + x1 * 4;
        float c[4];
        for (int ch = 0; ch < 4; ++ch) {
          const float a = static_cast<float>(p00[ch]) * itx +
                          static_cast<float>(p10[ch]) * tx;
          const float b = static_cast<float>(p01[ch]) * itx +
                          static_cast<float>(p11[ch]) * tx;
          c[ch] = a * ity + b * ty;
        }
        row[dx * 4 + 0] =
            static_cast<uint8_t>((std::min)(255.f, c[0] + 0.5f));
        row[dx * 4 + 1] =
            static_cast<uint8_t>((std::min)(255.f, c[1] + 0.5f));
        row[dx * 4 + 2] =
            static_cast<uint8_t>((std::min)(255.f, c[2] + 0.5f));
        row[dx * 4 + 3] =
            static_cast<uint8_t>((std::min)(255.f, c[3] + 0.5f));
      }
    } else {
      for (int dx = 0; dx < dst_w; ++dx) {
        const double u = u0 + (static_cast<double>(dx) + 0.5) * du;
        sample_rgba_to_bgra_bilinear(rgba, tw, th, u, v, row + dx * 4);
      }
    }
  };

  if (dst_h >= kParallelBlitMinRows &&
      dst_w * dst_h >= kParallelBlitMinPixels) {
    base::execution::GlobalNThreadPoolExecutor executor;
    base::execution::parallel_for(executor, 0, dst_h, resample_row);
  } else {
    for (int dy = 0; dy < dst_h; ++dy) {
      resample_row(dy);
    }
  }

  BOOL ok = FALSE;
  if (blend == DrawBlend::kMultiply) {
    vista::apply_multiply_coverage(
        std::span<uint8_t>(coverage.data(), cov_bytes), k);
    for (int dy = 0; dy < dst_h; ++dy) {
      for (int dx = 0; dx < dst_w; ++dx) {
        const size_t o = (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
                          static_cast<size_t>(dx)) *
                         4u;
        if (coverage[o + 3] == 0) {
          const unsigned b = dest[o + 0];
          const unsigned g = dest[o + 1];
          const unsigned r = dest[o + 2];
          const bool oceanish = (b > r + 15u && g > r);
          const bool land_cream =
              (r > 200u && g > 190u && b > 170u && !oceanish);
          if (land_cream) {
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
  DeleteObject(dest_dib);
  DeleteDC(mem);
  return ok != FALSE;
}

}  // namespace raster
}  // namespace vista
