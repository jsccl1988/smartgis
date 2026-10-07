// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/raster/direct/direct.h"

#include "gpu/raster/direct/mesh.h"
#include "vista/terrain/dem/raster/dem_raster.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace gpu {
namespace {

bool draw_direct_bitmap(content::ViewKind kind, int w, int h, uint8_t cb,
                        uint8_t cg, uint8_t cr, uint8_t ca,
                        std::vector<uint8_t>* out) {
  if (!out || w < 8 || h < 8) {
    return false;
  }
  const size_t bytes = static_cast<size_t>(w) * static_cast<size_t>(h) * 4u;
  std::vector<uint8_t>& pixels = *out;
  pixels.assign(bytes, 0);
  for (size_t i = 0; i < bytes; i += 4) {
    pixels[i + 0] = cb;
    pixels[i + 1] = cg;
    pixels[i + 2] = cr;
    pixels[i + 3] = ca;
  }

  BITMAPINFO bi = {};
  bi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bi.bmiHeader.biWidth = w;
  bi.bmiHeader.biHeight = -h;
  bi.bmiHeader.biPlanes = 1;
  bi.bmiHeader.biBitCount = 32;
  bi.bmiHeader.biCompression = BI_RGB;

  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
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
    return false;
  }

  std::memcpy(dib_bits, pixels.data(), bytes);
  HGDIOBJ old = SelectObject(mem, bmp);

  const COLORREF grid =
      (kind == content::ViewKind::kScene3d)
          ? RGB(180, 120, 80)
          : (kind == content::ViewKind::kMapData) ? RGB(90, 180, 120)
                                                 : RGB(120, 180, 220);
  const COLORREF feature = RGB(255, 200, 120);
  const COLORREF ink = RGB(235, 245, 255);

  // Scene3d: filled elevation DEM + labels (shell paints orbitable SoT on
  // top; this underlay must not look like a wireframe / flat olive cube).
  // 2D panes: light grid only (shell overlays GisScene vectors).
  if (kind != content::ViewKind::kScene3d) {
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
  } else {
    // Ocean clear (overwrite any prior clear tint).
    HBRUSH ocean = CreateSolidBrush(RGB(28, 72, 118));
    RECT full = {0, 0, w, h};
    FillRect(mem, &full, ocean);
    DeleteObject(ocean);

    const detail::SyntheticDemMesh& mesh = detail::synthetic_dem_mesh();
    if (mesh.ready) {
      const std::vector<float>& xyz = mesh.xyz;
      const std::vector<uint32_t>& indices = mesh.indices;
      const float miny = mesh.miny;
      const float maxy = mesh.maxy;
      const float cx = mesh.cx;
      const float cy = mesh.cy;
      const float cz = mesh.cz;
      const float span = mesh.span;
      const float s = 3.2f / span;
      constexpr float kElevBoost = 1.6f;
      constexpr float kYaw = vista::kDemDefaultOrbitYaw;
      constexpr float kPitch = 0.4f;
      constexpr float kDist = 3.2f;
      const float cyaw = std::cos(kYaw);
      const float syaw = std::sin(kYaw);
      const float cp = std::cos(kPitch);
      const float sp = std::sin(kPitch);
      auto project = [&](float x, float y, float z, int* sx, int* sy) {
        x = (x - cx) * s;
        y = (y - cy) * s * kElevBoost;
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

      HPEN mesh_pen = CreatePen(PS_SOLID, 1, RGB(70, 95, 65));
      HGDIOBJ old_pen = SelectObject(mem, mesh_pen);
      const size_t total_tris = indices.size() / 3;
      constexpr size_t kMaxDraw = 2200;
      const size_t step =
          total_tris > kMaxDraw ? (total_tris + kMaxDraw - 1) / kMaxDraw : 1;
      size_t drawn = 0;
      for (size_t t = 0; t < total_tris && drawn < kMaxDraw; t += step, ++drawn) {
        const uint32_t i0 = indices[t * 3];
        const uint32_t i1 = indices[t * 3 + 1];
        const uint32_t i2 = indices[t * 3 + 2];
        if ((i0 + 1) * 3 > xyz.size() || (i1 + 1) * 3 > xyz.size() ||
            (i2 + 1) * 3 > xyz.size()) {
          continue;
        }
        const float y0 = xyz[i0 * 3 + 1];
        const float y1 = xyz[i1 * 3 + 1];
        const float y2 = xyz[i2 * 3 + 1];
        const float yavg = (y0 + y1 + y2) / 3.f;
        const float t01 = std::clamp((yavg - miny) / (std::max)(maxy - miny, 1e-3f),
                                     0.f, 1.f);
        const int r = static_cast<int>(70 + 130 * t01);
        const int g = static_cast<int>(125 + 55 * (1.f - t01) + 40 * t01);
        const int b = static_cast<int>(55 + 30 * (1.f - t01));
        HBRUSH fill = CreateSolidBrush(RGB(r, g, b));
        SelectObject(mem, fill);
        int p0[2] = {};
        int p1[2] = {};
        int p2[2] = {};
        project(xyz[i0 * 3], xyz[i0 * 3 + 1], xyz[i0 * 3 + 2], &p0[0],
                &p0[1]);
        project(xyz[i1 * 3], xyz[i1 * 3 + 1], xyz[i1 * 3 + 2], &p1[0],
                &p1[1]);
        project(xyz[i2 * 3], xyz[i2 * 3 + 1], xyz[i2 * 3 + 2], &p2[0],
                &p2[1]);
        const POINT pts[3] = {{p0[0], p0[1]}, {p1[0], p1[1]}, {p2[0], p2[1]}};
        Polygon(mem, pts, 3);
        SelectObject(mem, GetStockObject(NULL_BRUSH));
        DeleteObject(fill);
      }
      SelectObject(mem, old_pen);
      DeleteObject(mesh_pen);

      // Hardcoded major cities (GPU process avoids GDAL / china_city race).
      struct Label {
        const wchar_t* name;
        double lon;
        double lat;
      };
      const Label labels[] = {
          {L"北京", 116.40, 39.90}, {L"上海", 121.47, 31.23},
          {L"广州", 113.27, 23.13}, {L"成都", 104.07, 30.67},
          {L"武汉", 114.30, 30.60}, {L"西安", 108.94, 34.34},
          {L"乌鲁木齐", 87.62, 43.82}, {L"拉萨", 91.11, 29.97},
      };
      SetBkMode(mem, TRANSPARENT);
      for (const Label& lb : labels) {
        int sx = 0;
        int sy = 0;
        project(vista::dem_lon_to_x(lb.lon), cy, static_cast<float>(lb.lat), &sx,
                &sy);
        if (sx < 0 || sy < 0 || sx > w || sy > h) {
          continue;
        }
        SetTextColor(mem, RGB(20, 24, 32));
        for (int dx = -1; dx <= 1; ++dx) {
          for (int dy = -1; dy <= 1; ++dy) {
            if (dx || dy) {
              TextOutW(mem, sx + dx, sy + dy, lb.name, lstrlenW(lb.name));
            }
          }
        }
        SetTextColor(mem, RGB(245, 248, 252));
        TextOutW(mem, sx, sy, lb.name, lstrlenW(lb.name));
      }

      // Compass
      const int ccx = w - 56;
      const int ccy = 56;
      const int rr = 28;
      HPEN ring = CreatePen(PS_SOLID, 2, RGB(210, 225, 240));
      HGDIOBJ old_compass = SelectObject(mem, ring);
      SelectObject(mem, GetStockObject(NULL_BRUSH));
      Ellipse(mem, ccx - rr, ccy - rr, ccx + rr, ccy + rr);
      HPEN needle = CreatePen(PS_SOLID, 2, RGB(220, 60, 50));
      SelectObject(mem, needle);
      MoveToEx(mem, ccx, ccy, nullptr);
      LineTo(mem, ccx, ccy - (rr - 6));
      SelectObject(mem, old_compass);
      DeleteObject(needle);
      DeleteObject(ring);
      SetTextColor(mem, ink);
      TextOutW(mem, ccx - 5, ccy - rr - 18, L"N", 1);
    } else {
      HPEN feat_pen = CreatePen(PS_SOLID, 2, feature);
      HGDIOBJ old_pen = SelectObject(mem, feat_pen);
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
  if (kind == content::ViewKind::kScene3d) {
    const wchar_t* wasd = L"WASD  orbit / wheel zoom";
    TextOutW(mem, 12, h > 40 ? h - 28 : 52, wasd, lstrlenW(wasd));
  }

  std::memcpy(pixels.data(), dib_bits, bytes);
  SelectObject(mem, old);
  DeleteObject(bmp);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return true;
}

}  // namespace

namespace detail {

void raster_direct_quads(RenderPass* pass, content::ViewKind kind,
                         uint32_t width_px, uint32_t height_px) {
  if (!pass || width_px == 0 || height_px == 0) {
    return;
  }
  pass->width_px = width_px;
  pass->height_px = height_px;
  // Distinct clears so Map / Data / 3D panes stay visibly different once
  // shell presents Latest() into the HWND.
  uint8_t b = 0x40;
  uint8_t g = 0x80;
  uint8_t r = 0xC0;
  uint8_t a = 0xFF;
  if (kind == content::ViewKind::kMapData) {
    b = 0x30;
    g = 0x70;
    r = 0x50;
  } else if (kind == content::ViewKind::kScene3d) {
    b = 0x90;
    g = 0x40;
    r = 0x28;
  }
  const uint32_t argb = (static_cast<uint32_t>(a) << 24) |
                        (static_cast<uint32_t>(r) << 16) |
                        (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
  std::vector<uint8_t> bitmap;
  if (!draw_direct_bitmap(kind, static_cast<int>(width_px),
                          static_cast<int>(height_px), b, g, r, a, &bitmap)) {
    append_solid_quad(pass, argb, 1.f);
    return;
  }
  append_bgra_quad(pass, std::move(bitmap), 1.f, true);
}

}  // namespace detail
}  // namespace gpu
