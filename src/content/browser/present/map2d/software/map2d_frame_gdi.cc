// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <vector>

#include "base/trace/event/process_trace.h"

#pragma comment(lib, "Msimg32.lib")

namespace content {
namespace detail {
namespace {

COLORREF rgba_to_colorref(uint32_t rgba) {
  const uint8_t r = static_cast<uint8_t>((rgba >> 16) & 0xff);
  const uint8_t g = static_cast<uint8_t>((rgba >> 8) & 0xff);
  const uint8_t b = static_cast<uint8_t>(rgba & 0xff);
  return RGB(r, g, b);
}

void world_to_view(const gis::vista::View& view, float x, float y, int* sx,
                   int* sy) {
  if (!sx || !sy || view.width_px == 0 || view.height_px == 0) {
    return;
  }
  const double dx = view.max_x - view.min_x;
  const double dy = view.max_y - view.min_y;
  if (dx == 0.0 || dy == 0.0) {
    *sx = 0;
    *sy = 0;
    return;
  }
  *sx = static_cast<int>(
      std::lround((static_cast<double>(x) - view.min_x) / dx * view.width_px));
  *sy = static_cast<int>(
      std::lround((view.max_y - static_cast<double>(y)) / dy * view.height_px));
}

void item_to_points(const gis::vista::DrawItem& item,
                    const gis::vista::View& view, std::vector<POINT>* out) {
  if (!out) {
    return;
  }
  out->clear();
  out->reserve(item.vertices.size());
  for (const gis::vista::Vertex& v : item.vertices) {
    int sx = 0;
    int sy = 0;
    if (item.pixel_space) {
      sx = static_cast<int>(std::lround(v.x));
      sy = static_cast<int>(std::lround(v.y));
    } else {
      world_to_view(view, v.x, v.y, &sx, &sy);
    }
    out->push_back(POINT{sx, sy});
  }
}

void fill_indexed_tris(HDC hdc, const std::vector<POINT>& pts,
                       const std::vector<uint32_t>& indices, HBRUSH brush,
                       HPEN pen) {
  if (pts.empty() || indices.size() < 3) {
    return;
  }
  HGDIOBJ old_brush = SelectObject(hdc, brush ? brush : GetStockObject(NULL_BRUSH));
  HGDIOBJ old_pen = SelectObject(hdc, pen ? pen : GetStockObject(NULL_PEN));
  for (size_t i = 0; i + 2 < indices.size(); i += 3) {
    const uint32_t a = indices[i];
    const uint32_t b = indices[i + 1];
    const uint32_t c = indices[i + 2];
    if (a >= pts.size() || b >= pts.size() || c >= pts.size()) {
      continue;
    }
    POINT tri[3] = {pts[a], pts[b], pts[c]};
    Polygon(hdc, tri, 3);
  }
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
}

void stroke_polyline(HDC hdc, const std::vector<POINT>& pts, HPEN pen) {
  if (pts.size() < 2 || !pen) {
    return;
  }
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  Polyline(hdc, pts.data(), static_cast<int>(pts.size()));
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
}

// Stretch tightly packed RGBA8 into the axis-aligned bbox of |pts|.
// Hillshade uses MapLibre-style multiply into the dest land (AlphaBlend
// SRC_OVER onto export CreateDIBSection left cream flat — gray_frac≈0).
bool blit_rgba_quad(HDC hdc, const std::vector<POINT>& pts,
                    const std::vector<uint8_t>& rgba, int tw, int th,
                    float opacity) {
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

  const float k =
      (std::max)(0.f, (std::min)(1.f, opacity));

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

  auto* dest = static_cast<uint8_t*>(dest_bits);
  for (int dy = 0; dy < dst_h; ++dy) {
    const int sy = (dy * th) / dst_h;
    for (int dx = 0; dx < dst_w; ++dx) {
      const int sx = (dx * tw) / dst_w;
      const size_t so =
          (static_cast<size_t>(sy) * static_cast<size_t>(tw) +
           static_cast<size_t>(sx)) *
          4u;
      const unsigned a = rgba[so + 3];
      if (a == 0) {
        continue;
      }
      const float sr = static_cast<float>(rgba[so + 0]) / 255.f;
      const float sg = static_cast<float>(rgba[so + 1]) / 255.f;
      const float sb = static_cast<float>(rgba[so + 2]) / 255.f;
      const float shade = 0.299f * sr + 0.587f * sg + 0.114f * sb;
      const float m = 1.f - k + k * shade;
      const size_t o =
          (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
           static_cast<size_t>(dx)) *
          4u;
      // Dest DIB is BGRA.
      dest[o + 0] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(dest[o + 0]) * m + 0.5f));
      dest[o + 1] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(dest[o + 1]) * m + 0.5f));
      dest[o + 2] = static_cast<uint8_t>(
          (std::min)(255.f, static_cast<float>(dest[o + 2]) * m + 0.5f));
    }
  }

  const BOOL ok = BitBlt(hdc, static_cast<int>(min_x), static_cast<int>(min_y),
                         dst_w, dst_h, mem, 0, 0, SRCCOPY);
  SelectObject(mem, old);
  DeleteObject(dest_dib);
  DeleteDC(mem);
  return ok != FALSE;
}

}  // namespace

void paint_map_frame_gdi(
    HDC hdc, const gis::vista::MapFrame& frame, const gis::vista::View& view,
    bool fill_background,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster) {
  if (!hdc || view.width_px == 0 || view.height_px == 0) {
    return;
  }

  {
    size_t n_raster = 0;
    size_t n_fill = 0;
    for (const gis::vista::DrawItem& item : frame.items) {
      if (item.kind == gis::vista::DrawKind::kRaster) {
        ++n_raster;
      } else if (item.kind == gis::vista::DrawKind::kFill) {
        ++n_fill;
      }
    }
    std::fprintf(stderr,
                 "map2d: gdi paint items=%zu raster=%zu fill=%zu view=%.2f..%.2f "
                 "x %.2f..%.2f\n",
                 frame.items.size(), n_raster, n_fill, view.min_x, view.max_x,
                 view.min_y, view.max_y);
  }

  if (fill_background) {
    HBRUSH bg = CreateSolidBrush(rgba_to_colorref(frame.background_rgba));
    RECT full = {0, 0, static_cast<LONG>(view.width_px),
                 static_cast<LONG>(view.height_px)};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  std::map<COLORREF, HBRUSH> brushes;
  std::map<uint64_t, HPEN> pens;
  auto brush_for = [&](COLORREF c) -> HBRUSH {
    auto it = brushes.find(c);
    if (it != brushes.end()) {
      return it->second;
    }
    HBRUSH b = CreateSolidBrush(c);
    brushes.emplace(c, b);
    return b;
  };
  auto pen_for = [&](COLORREF c, int width) -> HPEN {
    const int w = std::max(1, width);
    const uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 16) |
        static_cast<uint64_t>(static_cast<uint16_t>(w));
    auto it = pens.find(key);
    if (it != pens.end()) {
      return it->second;
    }
    // Geometric round pens soften stair-steps vs PS_SOLID cosmetic pens
    // (showcase export AA without SDF / MapLibre Native).
    LOGBRUSH lb{};
    lb.lbStyle = BS_SOLID;
    lb.lbColor = c;
    HPEN p = ExtCreatePen(PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND |
                              PS_JOIN_ROUND,
                          w, &lb, 0, nullptr);
    if (!p) {
      p = CreatePen(PS_SOLID, w, c);
    }
    pens.emplace(key, p);
    return p;
  };

  // MapFrame text is already in view/bitmap pixels (Layout advances). Do not
  // DPI-scale CreateFont here — MulDiv(px, LOGPIXELSY, 96) on a HiDPI DC
  // draws glyphs larger than the metrics pen and stacks CJK within a label.
  auto font_height = [](int px) { return -std::max(1, px); };
  HFONT text_font =
      CreateFontW(font_height(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                  L"Microsoft YaHei UI");
  HGDIOBJ old_font =
      SelectObject(hdc, text_font ? text_font : GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);

  std::vector<POINT> pts;
  int64_t fill_us = 0;
  int64_t line_us = 0;
  int64_t text_us = 0;
  int64_t other_us = 0;
  auto add_us = [](int64_t* bucket,
                   std::chrono::steady_clock::time_point t0) {
    *bucket += std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - t0)
                   .count();
  };
  for (const gis::vista::DrawItem& item : frame.items) {
    const auto t0 = std::chrono::steady_clock::now();
    const COLORREF color = rgba_to_colorref(item.rgba);
    switch (item.kind) {
      case gis::vista::DrawKind::kFill:
      case gis::vista::DrawKind::kCircle: {
        item_to_points(item, view, &pts);
        HPEN outline = pen_for(color, 1);
        fill_indexed_tris(hdc, pts, item.indices, brush_for(color), outline);
        add_us(&fill_us, t0);
        break;
      }
      case gis::vista::DrawKind::kLine: {
        item_to_points(item, view, &pts);
        // Tessellated lines are triangle strips/meshes; prefer fill when
        // indices exist, else stroke vertex order.
        if (item.indices.size() >= 3) {
          fill_indexed_tris(hdc, pts, item.indices, brush_for(color),
                            pen_for(color, 1));
        } else {
          // Soft understroke then core — poor-man's AA without SDF.
          const int stroke_w = 2;
          const COLORREF soft =
              RGB((GetRValue(color) * 2 + 255) / 3,
                  (GetGValue(color) * 2 + 255) / 3,
                  (GetBValue(color) * 2 + 255) / 3);
          stroke_polyline(hdc, pts, pen_for(soft, stroke_w + 1));
          stroke_polyline(hdc, pts, pen_for(color, stroke_w));
        }
        add_us(&line_us, t0);
        break;
      }
      case gis::vista::DrawKind::kRaster: {
        item_to_points(item, view, &pts);
        std::vector<uint8_t> rgba;
        int tw = 0;
        int th = 0;
        const bool loaded =
            load_raster && load_raster(item.codepoint, &rgba, &tw, &th);
        if (loaded && blit_rgba_quad(hdc, pts, rgba, tw, th, item.opacity)) {
          add_us(&other_us, t0);
          break;
        }
        std::fprintf(stderr,
                     "map2d: raster blit miss load=%d tw=%d th=%d pts=%zu "
                     "key=0x%08x opacity=%.2f\n",
                     loaded ? 1 : 0, tw, th, pts.size(), item.codepoint,
                     item.opacity);
        if (pts.size() >= 3) {
          fill_indexed_tris(hdc, pts, item.indices, brush_for(color),
                            static_cast<HPEN>(GetStockObject(NULL_PEN)));
        }
        add_us(&other_us, t0);
        break;
      }
      case gis::vista::DrawKind::kIcon: {
        // No icon atlas on GDI path; skip (Layout still placed text).
        add_us(&other_us, t0);
        break;
      }
      case gis::vista::DrawKind::kText: {
        // Layout emits one DrawItem per codepoint with a pixel-space glyph
        // quad in vertices[]. Using only anchor_x/y stacks every character on
        // the label center (garbled CJK "tofu" on the china showcase BMP).
        const uint32_t cp = item.codepoint;
        if (cp == 0 || cp > 0x10ffff) {
          add_us(&text_us, t0);
          break;
        }
        wchar_t utf16[2] = {};
        int utf16_n = 0;
        if (cp <= 0xffff) {
          utf16[0] = static_cast<wchar_t>(cp);
          utf16_n = 1;
        } else {
          const uint32_t u = cp - 0x10000;
          utf16[0] = static_cast<wchar_t>(0xd800 + (u >> 10));
          utf16[1] = static_cast<wchar_t>(0xdc00 + (u & 0x3ff));
          utf16_n = 2;
        }

        int ax = static_cast<int>(std::lround(item.anchor_x));
        int ay = static_cast<int>(std::lround(item.anchor_y));
        if (item.pixel_space && !item.vertices.empty()) {
          ax = static_cast<int>(std::lround(item.vertices.front().x));
          ay = static_cast<int>(std::lround(item.vertices.front().y));
        } else if (!item.pixel_space && !item.vertices.empty()) {
          world_to_view(view, item.vertices.front().x, item.vertices.front().y,
                        &ax, &ay);
        }

        const int font_px =
            item.text_size_px > 0.5f
                ? static_cast<int>(std::lround(item.text_size_px))
                : 13;
        HFONT glyph_font = nullptr;
        const double deg = item.angle_rad * (180.0 / 3.14159265358979323846);
        if (std::fabs(deg) > 0.5) {
          const int esc = static_cast<int>(std::lround(-deg * 10.0));
          glyph_font =
              CreateFontW(font_height(font_px), 0, esc, esc, FW_NORMAL, FALSE,
                          FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        } else if (font_px != 13) {
          glyph_font =
              CreateFontW(font_height(font_px), 0, 0, 0, FW_NORMAL, FALSE, FALSE,
                          FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                          CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                          DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
        }
        if (glyph_font) {
          SelectObject(hdc, glyph_font);
        }

        const COLORREF ink = rgba_to_colorref(item.rgba);
        const COLORREF halo =
            item.halo_width_px > 0.f
                ? rgba_to_colorref(item.halo_rgba ? item.halo_rgba : 0xffffffffu)
                : RGB(255, 255, 255);
        SetTextColor(hdc, halo);
        const int halo_d[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                                  {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
        for (const auto& d : halo_d) {
          TextOutW(hdc, ax + d[0], ay + d[1], utf16, utf16_n);
        }
        SetTextColor(hdc, ink);
        TextOutW(hdc, ax, ay, utf16, utf16_n);
        if (glyph_font) {
          SelectObject(hdc, text_font ? text_font
                                      : GetStockObject(DEFAULT_GUI_FONT));
          DeleteObject(glyph_font);
        }
        add_us(&text_us, t0);
        break;
      }
    }
  }

  if (base::trace::tracing_enabled()) {
    auto flush_kind = [](const char* name, int64_t us) {
      if (us <= 0) {
        return;
      }
      const auto end = std::chrono::steady_clock::now();
      const auto begin = end - std::chrono::microseconds(us);
      base::trace::process_trace().add(name, "map2d.gdi", begin, end);
    };
    flush_kind("gdi_fill", fill_us);
    flush_kind("gdi_line", line_us);
    flush_kind("gdi_text", text_us);
    flush_kind("gdi_other", other_us);
  }

  SelectObject(hdc, old_font);
  if (text_font) {
    DeleteObject(text_font);
  }
  for (auto& kv : brushes) {
    DeleteObject(kv.second);
  }
  for (auto& kv : pens) {
    DeleteObject(kv.second);
  }
}

}  // namespace detail
}  // namespace content
