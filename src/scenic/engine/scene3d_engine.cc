// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/engine.h"

#include "scenic/engine/mem_frame.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace scenic {
namespace detail {
namespace {

uint64_t bits_u64(double v) {
  uint64_t u = 0;
  static_assert(sizeof(double) == sizeof(uint64_t), "double size");
  std::memcpy(&u, &v, sizeof(u));
  return u;
}

uint64_t bits_u32(float v) {
  uint32_t u = 0;
  static_assert(sizeof(float) == sizeof(uint32_t), "float size");
  std::memcpy(&u, &v, sizeof(u));
  return u;
}

POINT project_ll(const OrbitXform& o, float lon, float lat, int w, int h) {
  const double dx = (o.lon_max > o.lon_min) ? (o.lon_max - o.lon_min) : 1.0;
  const double dy = (o.lat_max > o.lat_min) ? (o.lat_max - o.lat_min) : 1.0;
  const double nx = (static_cast<double>(lon) - o.lon_min) / dx * 2.0 - 1.0;
  const double ny = (static_cast<double>(lat) - o.lat_min) / dy * 2.0 - 1.0;
  const double cy = std::cos(static_cast<double>(o.yaw));
  const double sy = std::sin(static_cast<double>(o.yaw));
  const double cp = std::max(0.25, std::cos(static_cast<double>(o.pitch)));
  const double dist = o.distance > 0.2f ? static_cast<double>(o.distance) : 3.2;
  const double scale = (0.38 * 3.2) / dist;
  const double px = nx * cy - ny * sy;
  const double py = (nx * sy + ny * cy) * cp;
  POINT p;
  p.x = static_cast<int>(std::lround(w * 0.5 + px * w * scale));
  p.y = static_cast<int>(std::lround(h * 0.52 - py * h * scale));
  return p;
}

uint64_t orbit_fingerprint(const OrbitXform& o, uint32_t w, uint32_t h,
                           const DrawItem* items, uint32_t count) {
  uint64_t fp = 0;
  fp = mix_u64(fp, w);
  fp = mix_u64(fp, h);
  fp = mix_u64(fp, count);
  fp = mix_u64(fp, bits_u32(o.yaw));
  fp = mix_u64(fp, bits_u32(o.pitch));
  fp = mix_u64(fp, bits_u32(o.distance));
  fp = mix_u64(fp, bits_u64(o.lon_min));
  fp = mix_u64(fp, bits_u64(o.lat_min));
  fp = mix_u64(fp, bits_u64(o.lon_max));
  fp = mix_u64(fp, bits_u64(o.lat_max));
  fp = mix_u64(fp, reinterpret_cast<uintptr_t>(items));
  if (items && count > 0) {
    fp = mix_u64(fp, items[0].vertex_count);
    fp = mix_u64(fp, items[count - 1].vertex_count);
  }
  return fp == 0 ? 1 : fp;
}

void paint_sky_gradient(HDC hdc, int w, int h) {
  // Soft vertical sky → horizon (no solid flat fill).
  constexpr int kBands = 24;
  for (int i = 0; i < kBands; ++i) {
    const int y0 = (h * i) / kBands;
    const int y1 = (h * (i + 1)) / kBands;
    const double t = static_cast<double>(i) / (kBands - 1);
    const int r = static_cast<int>(48 + t * 70);
    const int g = static_cast<int>(110 + t * 50);
    const int b = static_cast<int>(190 - t * 30);
    RECT band = {0, y0, w, y1};
    HBRUSH br = CreateSolidBrush(RGB(r, g, b));
    FillRect(hdc, &band, br);
    DeleteObject(br);
  }
}

void paint_cloud_bands(HDC hdc, int w, int h, const OrbitXform& orbit) {
  // Lightweight translucent cloud strips for atmosphere/world3d scenic rows.
  const double yaw = static_cast<double>(orbit.yaw);
  const int base_y = h / 5;
  for (int c = 0; c < 5; ++c) {
    const double phase = yaw * 0.35 + c * 1.1;
    const int cx = static_cast<int>(w * (0.15 + 0.18 * c + 0.04 * std::sin(phase)));
    const int cy = base_y + static_cast<int>(h * 0.04 * std::cos(phase * 0.7));
    const int rw = w / 9 + (c % 3) * (w / 40);
    const int rh = h / 28 + (c % 2) * (h / 60);
    HBRUSH cloud = CreateSolidBrush(RGB(230, 238, 245));
    HPEN pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
    HGDIOBJ old_pen = SelectObject(hdc, pen);
    HGDIOBJ old_brush = SelectObject(hdc, cloud);
    Ellipse(hdc, cx - rw, cy - rh, cx + rw, cy + rh);
    Ellipse(hdc, cx - rw / 2, cy - rh - rh / 3, cx + rw / 2, cy + rh / 2);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(cloud);
    DeleteObject(pen);
  }
}

COLORREF land_fill_for_lat(float lat) {
  // DEM-like green→tan by latitude band (software stand-in for lit DEM).
  const double t = std::clamp((static_cast<double>(lat) - 18.0) / 35.0, 0.0, 1.0);
  const int r = static_cast<int>(110 + t * 55);
  const int g = static_cast<int>(165 - t * 35);
  const int b = static_cast<int>(78 + t * 20);
  return RGB(r, g, b);
}

void paint_scene(HDC hdc, uint32_t width_px, uint32_t height_px,
                 const OrbitXform& orbit, const DrawItem* items,
                 uint32_t count) {
  if (!hdc || width_px == 0 || height_px == 0) {
    return;
  }
  const int w = static_cast<int>(width_px);
  const int h = static_cast<int>(height_px);
  paint_sky_gradient(hdc, w, h);

  const int cx = w / 2;
  const int cy = h / 2 + h / 18;
  const int rx = static_cast<int>(w * 0.42);
  const int ry = static_cast<int>(h * 0.36);
  HBRUSH ocean = CreateSolidBrush(RGB(28, 96, 158));
  HPEN ocean_pen = CreatePen(PS_SOLID, 2, RGB(16, 60, 100));
  HGDIOBJ old_pen = SelectObject(hdc, ocean_pen);
  HGDIOBJ old_brush = SelectObject(hdc, ocean);
  Ellipse(hdc, cx - rx, cy - ry, cx + rx, cy + ry);
  // Inner ocean rim for depth cue.
  HBRUSH ocean_inner = CreateSolidBrush(RGB(40, 120, 176));
  SelectObject(hdc, ocean_inner);
  Ellipse(hdc, cx - rx + rx / 10, cy - ry + ry / 10, cx + rx - rx / 10,
          cy + ry - ry / 10);
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(ocean_inner);
  DeleteObject(ocean);
  DeleteObject(ocean_pen);

  // Cap polygon work: china_city has tens of thousands of rings — full GDI
  // Polygon every WM_PAINT is unusable. Prefer large land rings; skip crumbs.
  constexpr uint32_t kMaxPolys = 600u;
  constexpr uint32_t kMaxVerts = 900u;
  constexpr uint32_t kMinVerts = 8u;
  std::vector<POINT> pts;
  uint32_t drawn = 0;
  if (items && count > 0) {
    pts.reserve(kMaxVerts);
    for (uint32_t i = 0; i < count && drawn < kMaxPolys; ++i) {
      const DrawItem& it = items[i];
      if (!it.xy || it.vertex_count < kMinVerts ||
          it.kind != GeomKind::kPolygon) {
        continue;
      }
      pts.clear();
      const uint32_t n = it.vertex_count;
      const uint32_t step =
          n > kMaxVerts ? (n + kMaxVerts - 1) / kMaxVerts : 1u;
      float lat_acc = 0.f;
      uint32_t lat_n = 0;
      for (uint32_t v = 0; v < n; v += step) {
        // Map Y is -lat in host bind; project_ll expects lon/lat.
        const float lon = it.xy[v].x;
        const float lat = -it.xy[v].y;
        lat_acc += lat;
        ++lat_n;
        pts.push_back(project_ll(orbit, lon, lat, w, h));
      }
      if (n >= 2 && step > 1 && (n - 1) % step != 0) {
        const float lon = it.xy[n - 1].x;
        const float lat = -it.xy[n - 1].y;
        lat_acc += lat;
        ++lat_n;
        pts.push_back(project_ll(orbit, lon, lat, w, h));
      }
      if (pts.size() < 3) {
        continue;
      }
      const float mean_lat = lat_n ? (lat_acc / static_cast<float>(lat_n)) : 30.f;
      const COLORREF fill = it.fill_colorref
                                ? static_cast<COLORREF>(it.fill_colorref)
                                : land_fill_for_lat(mean_lat);
      const COLORREF stroke = it.stroke_colorref
                                  ? static_cast<COLORREF>(it.stroke_colorref)
                                  : RGB(48, 96, 40);
      HPEN pen = CreatePen(PS_SOLID, 1, stroke);
      HBRUSH brush = CreateSolidBrush(fill);
      old_pen = SelectObject(hdc, pen);
      old_brush = SelectObject(hdc, brush);
      Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
      SelectObject(hdc, old_brush);
      SelectObject(hdc, old_pen);
      DeleteObject(brush);
      DeleteObject(pen);
      ++drawn;
    }
  }

  // World3d/atmosphere scenic often has no MapScene bind (OGR seed skipped).
  // Paint land for the *current orbit lon/lat window* (not a full-China ring
  // that explodes under a regional east-China extent and floods the frame).
  if (drawn == 0) {
    constexpr int kSamples = 56;
    constexpr double kPi = 3.141592653589793;
    pts.clear();
    pts.reserve(static_cast<size_t>(kSamples));
    float lat_acc = 0.f;
    for (int i = 0; i < kSamples; ++i) {
      const double t = 2.0 * kPi * static_cast<double>(i) / kSamples;
      // Soft coastal ellipse inside the orbit box + light shoreline noise.
      const double u =
          0.50 + 0.40 * std::cos(t) + 0.035 * std::sin(3.0 * t);
      const double v =
          0.50 + 0.36 * std::sin(t) + 0.025 * std::cos(5.0 * t);
      const float lon = static_cast<float>(
          orbit.lon_min + u * (orbit.lon_max - orbit.lon_min));
      const float lat = static_cast<float>(
          orbit.lat_min + v * (orbit.lat_max - orbit.lat_min));
      lat_acc += lat;
      pts.push_back(project_ll(orbit, lon, lat, w, h));
    }
    if (pts.size() >= 3) {
      const float mean_lat = lat_acc / static_cast<float>(pts.size());
      // Clip land to the ocean disk so it reads as terrain on a globe.
      HRGN ocean_rgn = CreateEllipticRgn(cx - rx, cy - ry, cx + rx, cy + ry);
      HRGN old_clip = CreateRectRgn(0, 0, 0, 0);
      const int got = GetClipRgn(hdc, old_clip);
      if (ocean_rgn) {
        ExtSelectClipRgn(hdc, ocean_rgn, RGN_AND);
      }
      HPEN pen = CreatePen(PS_SOLID, 1, RGB(48, 96, 40));
      HBRUSH brush = CreateSolidBrush(land_fill_for_lat(mean_lat));
      old_pen = SelectObject(hdc, pen);
      old_brush = SelectObject(hdc, brush);
      SetPolyFillMode(hdc, WINDING);
      Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
      SelectObject(hdc, old_brush);
      SelectObject(hdc, old_pen);
      DeleteObject(brush);
      DeleteObject(pen);
      // Hypsometric bands + a simple "ridge" polyline (DEM stand-in).
      for (int band = 0; band < 3; ++band) {
        std::vector<POINT> inner;
        inner.reserve(pts.size());
        const double shrink = 0.10 * (band + 1);
        double acu = 0.0;
        double acv = 0.0;
        for (int i = 0; i < kSamples; ++i) {
          const double t = 2.0 * kPi * static_cast<double>(i) / kSamples;
          const double u =
              0.50 + (0.40 - shrink) * std::cos(t) + 0.02 * std::sin(3.0 * t);
          const double v =
              0.50 + (0.36 - shrink) * std::sin(t) + 0.015 * std::cos(5.0 * t);
          acu += u;
          acv += v;
          const float lon = static_cast<float>(
              orbit.lon_min + u * (orbit.lon_max - orbit.lon_min));
          const float lat = static_cast<float>(
              orbit.lat_min + v * (orbit.lat_max - orbit.lat_min));
          inner.push_back(project_ll(orbit, lon, lat, w, h));
        }
        (void)acu;
        (void)acv;
        const int shade = 18 * band;
        HBRUSH ib =
            CreateSolidBrush(RGB(110 + shade, 155 - shade / 2, 72 + shade / 3));
        HPEN ip = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
        old_pen = SelectObject(hdc, ip);
        old_brush = SelectObject(hdc, ib);
        Polygon(hdc, inner.data(), static_cast<int>(inner.size()));
        SelectObject(hdc, old_brush);
        SelectObject(hdc, old_pen);
        DeleteObject(ib);
        DeleteObject(ip);
      }
      // Ridge line across the land pad.
      POINT ridge[5];
      for (int i = 0; i < 5; ++i) {
        const double u = 0.22 + 0.14 * i;
        const double v = 0.35 + 0.08 * std::sin(i * 1.2);
        const float lon = static_cast<float>(
            orbit.lon_min + u * (orbit.lon_max - orbit.lon_min));
        const float lat = static_cast<float>(
            orbit.lat_min + v * (orbit.lat_max - orbit.lat_min));
        ridge[i] = project_ll(orbit, lon, lat, w, h);
      }
      HPEN ridge_pen = CreatePen(PS_SOLID, 2, RGB(90, 120, 55));
      old_pen = SelectObject(hdc, ridge_pen);
      Polyline(hdc, ridge, 5);
      SelectObject(hdc, old_pen);
      DeleteObject(ridge_pen);
      if (got == 1) {
        SelectClipRgn(hdc, old_clip);
      } else {
        SelectClipRgn(hdc, nullptr);
      }
      if (ocean_rgn) {
        DeleteObject(ocean_rgn);
      }
      DeleteObject(old_clip);
    }
  }

  paint_cloud_bands(hdc, w, h, orbit);

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(245, 248, 252));
  TextOutA(hdc, 12, 10, "scenic", 6);
}

bool write_bmp_file(const char* path, int width_px, int height_px,
                    const void* bits, int stride_bytes) {
  if (!path || !bits || width_px <= 0 || height_px <= 0 || stride_bytes <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride_bytes) * static_cast<DWORD>(height_px);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width_px;
  bih.biHeight = -height_px;
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  const bool ok = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
                  std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
                  std::fwrite(bits, 1, image_bytes, f) == image_bytes;
  std::fclose(f);
  return ok;
}

}  // namespace

class Scene3dEngine final : public Engine {
 public:
  bool initialize(const SessionDesc& desc) override {
    width_ = desc.width_px;
    height_ = desc.height_px;
    last_ok_ = width_ > 0 && height_ > 0;
    return last_ok_;
  }
  void shutdown() override { frame_.destroy(); }
  // HWND-free software present: rasterize into MemFrame so content
  // present_gpu / export_bmp / paint_hdc can succeed without FlyCube.
  bool present() override {
    if (width_ == 0 || height_ == 0) {
      last_ok_ = false;
      return false;
    }
    const uint64_t fp =
        orbit_fingerprint(orbit_, width_, height_, items_, count_);
    if (frame_.matches(width_, height_, fp)) {
      last_ok_ = true;
      return true;
    }
    if (!frame_.ensure(width_, height_)) {
      last_ok_ = false;
      return false;
    }
    paint_scene(frame_.dc, width_, height_, orbit_, items_, count_);
    frame_.fingerprint = fp;
    last_ok_ = true;
    return true;
  }
  Kind kind() const override { return Kind::kScene3d; }

  void bind_orbit(const OrbitXform& orbit) override { orbit_ = orbit; }
  void bind_draw_items(const DrawItem* items, uint32_t count) override {
    items_ = items;
    count_ = count;
  }

  bool paint_hdc(void* hdc, uint32_t width_px, uint32_t height_px) override {
    HDC dc = static_cast<HDC>(hdc);
    if (!dc || width_px == 0 || height_px == 0) {
      last_ok_ = false;
      return false;
    }
    const uint64_t fp =
        orbit_fingerprint(orbit_, width_px, height_px, items_, count_);
    if (frame_.matches(width_px, height_px, fp)) {
      last_ok_ = frame_.blit_to(dc);
      return last_ok_;
    }
    if (!frame_.ensure(width_px, height_px)) {
      paint_scene(dc, width_px, height_px, orbit_, items_, count_);
      last_ok_ = true;
      return true;
    }
    paint_scene(frame_.dc, width_px, height_px, orbit_, items_, count_);
    frame_.fingerprint = fp;
    last_ok_ = frame_.blit_to(dc);
    return last_ok_;
  }

  bool export_bmp(const char* path, uint32_t width_px,
                  uint32_t height_px) override {
    if (!path || width_px == 0 || height_px == 0) {
      last_ok_ = false;
      return false;
    }
    const uint64_t fp =
        orbit_fingerprint(orbit_, width_px, height_px, items_, count_);
    if (!frame_.matches(width_px, height_px, fp)) {
      if (!frame_.ensure(width_px, height_px)) {
        last_ok_ = false;
        return false;
      }
      paint_scene(frame_.dc, width_px, height_px, orbit_, items_, count_);
      frame_.fingerprint = fp;
    }
    const int stride = static_cast<int>(width_px) * 4;
    last_ok_ = write_bmp_file(path, static_cast<int>(width_px),
                              static_cast<int>(height_px), frame_.bits, stride);
    return last_ok_;
  }

  bool last_present_ok() const override { return last_ok_; }

 private:
  uint32_t width_ = 0;
  uint32_t height_ = 0;
  OrbitXform orbit_{};
  const DrawItem* items_ = nullptr;
  uint32_t count_ = 0;
  bool last_ok_ = false;
  MemFrame frame_;
};

}  // namespace detail

Engine* create_scene3d_engine() { return new detail::Scene3dEngine(); }

}  // namespace scenic
