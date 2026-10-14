// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/export/map2d_mapir_export.h"

#include "content/browser/present/host/gdi/gdi_primitives.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {
namespace {

struct WorldToPixel {
  double min_x = 0;
  double min_y = 0;
  double span_x = 1;
  double span_y = 1;
  int width_px = 1;
  int height_px = 1;

  POINT to_point(float x, float y) const {
    const double u = (static_cast<double>(x) - min_x) / span_x;
    const double v = (max_y() - static_cast<double>(y)) / span_y;
    POINT p;
    p.x = static_cast<LONG>(std::lround(u * width_px));
    p.y = static_cast<LONG>(std::lround(v * height_px));
    return p;
  }

  double max_y() const { return min_y + span_y; }
};

WorldToPixel make_xform(const Map2dFrameCache::CameraKey& cam) {
  WorldToPixel x;
  x.min_x = cam.min_x;
  x.min_y = cam.min_y;
  x.span_x = cam.max_x - cam.min_x;
  x.span_y = cam.max_y - cam.min_y;
  if (!(x.span_x > 0.0)) {
    x.span_x = 1.0;
  }
  if (!(x.span_y > 0.0)) {
    x.span_y = 1.0;
  }
  x.width_px = cam.width_px > 0 ? static_cast<int>(cam.width_px) : 1;
  x.height_px = cam.height_px > 0 ? static_cast<int>(cam.height_px) : 1;
  return x;
}

COLORREF rgba_opaque(uint32_t rgba) {
  return rgba_to_colorref(rgba);
}

void fill_background(HDC hdc, int w, int h, uint32_t rgba) {
  gdi_fill_rect(hdc, 0, 0, w, h, rgba_opaque(rgba));
}

void paint_fill(HDC hdc, const vista::DrawItem& item, const WorldToPixel& x) {
  if (item.vertices.size() < 3) {
    return;
  }
  const COLORREF color = rgba_opaque(item.rgba);
  HBRUSH brush = ::CreateSolidBrush(color);
  HPEN pen = ::CreatePen(PS_NULL, 0, color);
  HGDIOBJ old_b = ::SelectObject(hdc, brush);
  HGDIOBJ old_p = ::SelectObject(hdc, pen);
  if (!item.indices.empty()) {
    for (size_t i = 0; i + 2 < item.indices.size(); i += 3) {
      POINT pts[3];
      for (int k = 0; k < 3; ++k) {
        const uint32_t vi = item.indices[i + static_cast<size_t>(k)];
        if (vi >= item.vertices.size()) {
          pts[0].x = pts[1].x = pts[2].x = 0;
          break;
        }
        const vista::Vertex& v = item.vertices[vi];
        pts[k] = x.to_point(v.x, v.y);
      }
      ::Polygon(hdc, pts, 3);
    }
  } else {
    std::vector<POINT> pts;
    pts.reserve(item.vertices.size());
    for (const vista::Vertex& v : item.vertices) {
      pts.push_back(x.to_point(v.x, v.y));
    }
    if (pts.size() >= 3) {
      ::Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
    }
  }
  ::SelectObject(hdc, old_b);
  ::SelectObject(hdc, old_p);
  ::DeleteObject(brush);
  ::DeleteObject(pen);
}

void paint_line(HDC hdc, const vista::DrawItem& item, const WorldToPixel& x) {
  // Thick strokes are triangle meshes (same as fill). Polyline over those
  // verts draws random chords across China.
  if (!item.indices.empty() && item.vertices.size() >= 3) {
    paint_fill(hdc, item, x);
    return;
  }
  if (item.vertices.size() < 2) {
    return;
  }
  HPEN pen = ::CreatePen(PS_SOLID, 1, rgba_opaque(item.rgba));
  HGDIOBJ old = ::SelectObject(hdc, pen);
  std::vector<POINT> pts;
  pts.reserve(item.vertices.size());
  for (const vista::Vertex& v : item.vertices) {
    pts.push_back(x.to_point(v.x, v.y));
  }
  ::Polyline(hdc, pts.data(), static_cast<int>(pts.size()));
  ::SelectObject(hdc, old);
  ::DeleteObject(pen);
}

void paint_raster(HDC hdc, const vista::DrawItem& item, const WorldToPixel& x,
                  const MapIrRasterLoader& load_raster) {
  if (!load_raster || item.vertices.size() < 2) {
    return;
  }
  std::vector<uint8_t> rgba;
  int rw = 0;
  int rh = 0;
  if (!load_raster(item.codepoint, &rgba, &rw, &rh) || rw <= 0 || rh <= 0 ||
      rgba.size() < static_cast<size_t>(rw) * static_cast<size_t>(rh) * 4u) {
    return;
  }
  float min_x = item.vertices[0].x;
  float min_y = item.vertices[0].y;
  float max_x = min_x;
  float max_y = min_y;
  float u0 = item.vertices[0].u;
  float u1 = u0;
  float v0 = item.vertices[0].v;
  float v1 = v0;
  for (const vista::Vertex& vert : item.vertices) {
    min_x = (std::min)(min_x, vert.x);
    min_y = (std::min)(min_y, vert.y);
    max_x = (std::max)(max_x, vert.x);
    max_y = (std::max)(max_y, vert.y);
    u0 = (std::min)(u0, vert.u);
    u1 = (std::max)(u1, vert.u);
    v0 = (std::min)(v0, vert.v);
    v1 = (std::max)(v1, vert.v);
  }
  const POINT tl = x.to_point(min_x, max_y);
  const POINT br = x.to_point(max_x, min_y);
  const int dx = br.x - tl.x;
  const int dy = br.y - tl.y;
  if (dx <= 0 || dy <= 0) {
    return;
  }
  // Honor DrawItem UVs: emit_hillshade clips the DEM quad to the view and
  // remaps u/v. Stretching the full sheet onto that AABB shifted jet DEM
  // vs cream/admin vectors (software MapIR export; GPU MapPass samples UV).
  u0 = (std::max)(0.f, (std::min)(1.f, u0));
  u1 = (std::max)(0.f, (std::min)(1.f, u1));
  v0 = (std::max)(0.f, (std::min)(1.f, v0));
  v1 = (std::max)(0.f, (std::min)(1.f, v1));
  if (u1 <= u0 || v1 <= v0) {
    u0 = 0.f;
    u1 = 1.f;
    v0 = 0.f;
    v1 = 1.f;
  }
  int src_x = static_cast<int>(std::lround(static_cast<double>(u0) * rw));
  int src_y = static_cast<int>(std::lround(static_cast<double>(v0) * rh));
  int src_w =
      static_cast<int>(std::lround(static_cast<double>(u1 - u0) * rw));
  int src_h =
      static_cast<int>(std::lround(static_cast<double>(v1 - v0) * rh));
  if (src_x < 0) {
    src_x = 0;
  }
  if (src_y < 0) {
    src_y = 0;
  }
  if (src_x >= rw) {
    src_x = rw - 1;
  }
  if (src_y >= rh) {
    src_y = rh - 1;
  }
  if (src_w < 1) {
    src_w = 1;
  }
  if (src_h < 1) {
    src_h = 1;
  }
  if (src_x + src_w > rw) {
    src_w = rw - src_x;
  }
  if (src_y + src_h > rh) {
    src_h = rh - src_y;
  }
  // RGBA8 → BGRA for StretchDIBits.
  std::vector<uint8_t> bgra(rgba.size());
  for (size_t i = 0; i + 3 < rgba.size(); i += 4) {
    bgra[i + 0] = rgba[i + 2];
    bgra[i + 1] = rgba[i + 1];
    bgra[i + 2] = rgba[i + 0];
    bgra[i + 3] = rgba[i + 3];
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = rw;
  bmi.bmiHeader.biHeight = -rh;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  const BLENDFUNCTION blend{AC_SRC_OVER, 0,
                            static_cast<BYTE>(std::lround(
                                (std::max)(0.f, (std::min)(1.f, item.opacity)) *
                                255.f)),
                            AC_SRC_ALPHA};
  HDC mem = ::CreateCompatibleDC(hdc);
  void* bits = nullptr;
  HBITMAP dib =
      ::CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!mem || !dib || !bits) {
    if (dib) {
      ::DeleteObject(dib);
    }
    if (mem) {
      ::DeleteDC(mem);
    }
    return;
  }
  std::memcpy(bits, bgra.data(), bgra.size());
  HGDIOBJ old = ::SelectObject(mem, dib);
  // #region agent log
  if (FILE* f = std::fopen("C:/Dev/src/gis/smartgis/debug-6e0a8e.log", "a")) {
    std::fprintf(
        f,
        "{\"sessionId\":\"6e0a8e\",\"runId\":\"post-fix\","
        "\"hypothesisId\":\"F\",\"location\":\"map2d_mapir_export.cc:"
        "paint_raster\","
        "\"message\":\"raster uv blit\","
        "\"data\":{\"uv\":[%.4f,%.4f,%.4f,%.4f],\"src\":[%d,%d,%d,%d],"
        "\"dst\":[%d,%d,%d,%d],\"tex\":[%d,%d]},"
        "\"timestamp\":%llu}\n",
        u0, v0, u1, v1, src_x, src_y, src_w, src_h, tl.x, tl.y, dx, dy, rw, rh,
        static_cast<unsigned long long>(::GetTickCount64()));
    std::fclose(f);
  }
  // #endregion
  ::AlphaBlend(hdc, tl.x, tl.y, dx, dy, mem, src_x, src_y, src_w, src_h,
               blend);
  ::SelectObject(mem, old);
  ::DeleteObject(dib);
  ::DeleteDC(mem);
}

bool write_dib_bmp(const std::string& path, int w, int h, const void* bgra) {
  if (path.empty() || !bgra || w <= 0 || h <= 0) {
    return false;
  }
  FILE* fp = nullptr;
  if (fopen_s(&fp, path.c_str(), "wb") != 0 || !fp) {
    return false;
  }
  const DWORD row_bytes =
      static_cast<DWORD>(w) * 4u;
  const DWORD img_bytes = row_bytes * static_cast<DWORD>(h);
  BITMAPFILEHEADER bfh = {};
  BITMAPINFOHEADER bih = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + img_bytes;
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = w;
  bih.biHeight = -h;
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = img_bytes;
  const bool ok =
      std::fwrite(&bfh, sizeof(bfh), 1, fp) == 1 &&
      std::fwrite(&bih, sizeof(bih), 1, fp) == 1 &&
      std::fwrite(bgra, 1, img_bytes, fp) == img_bytes;
  std::fclose(fp);
  return ok;
}

}  // namespace

bool export_mapir_bmp(const vista::MapIR& frame,
                      const Map2dFrameCache::CameraKey& cam,
                      const MapIrRasterLoader& load_raster,
                      const std::string& path) {
  if (path.empty() || cam.width_px == 0 || cam.height_px == 0) {
    return false;
  }
  const int w = static_cast<int>(cam.width_px);
  const int h = static_cast<int>(cam.height_px);
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = w;
  bmi.bmiHeader.biHeight = -h;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HDC screen = ::GetDC(nullptr);
  HDC mem = ::CreateCompatibleDC(screen);
  HBITMAP dib =
      ::CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!screen || !mem || !dib || !bits) {
    if (dib) {
      ::DeleteObject(dib);
    }
    if (mem) {
      ::DeleteDC(mem);
    }
    if (screen) {
      ::ReleaseDC(nullptr, screen);
    }
    return false;
  }
  HGDIOBJ old = ::SelectObject(mem, dib);
  fill_background(mem, w, h, frame.background_rgba);
  const WorldToPixel xform = make_xform(cam);
  ::SetBkMode(mem, TRANSPARENT);
  for (const vista::DrawItem& item : frame.items) {
    switch (item.kind) {
      case vista::DrawKind::kFill:
        paint_fill(mem, item, xform);
        break;
      case vista::DrawKind::kLine:
        paint_line(mem, item, xform);
        break;
      case vista::DrawKind::kRaster:
        paint_raster(mem, item, xform, load_raster);
        break;
      case vista::DrawKind::kCircle:
      case vista::DrawKind::kIcon:
      case vista::DrawKind::kText:
        // Labels / circles stay on MapPass for interactive; BMP gold is
        // land + jet + roads (align carto).
        break;
    }
  }
  const bool ok = write_dib_bmp(path, w, h, bits);
  ::SelectObject(mem, old);
  ::DeleteObject(dib);
  ::DeleteDC(mem);
  ::ReleaseDC(nullptr, screen);
  return ok;
}

}  // namespace detail
}  // namespace content
