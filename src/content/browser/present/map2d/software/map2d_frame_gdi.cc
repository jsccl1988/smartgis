// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <map>
#include <vector>

#include "base/trace/event/process_trace.h"

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

}  // namespace

void paint_map_frame_gdi(HDC hdc, const gis::vista::MapFrame& frame,
                         const gis::vista::View& view, bool fill_background) {
  if (!hdc || view.width_px == 0 || view.height_px == 0) {
    return;
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
    const uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 16) |
        static_cast<uint64_t>(static_cast<uint16_t>(std::max(1, width)));
    auto it = pens.find(key);
    if (it != pens.end()) {
      return it->second;
    }
    HPEN p = CreatePen(PS_SOLID, std::max(1, width), c);
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
          stroke_polyline(hdc, pts, pen_for(color, 2));
        }
        add_us(&line_us, t0);
        break;
      }
      case gis::vista::DrawKind::kRaster: {
        item_to_points(item, view, &pts);
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
