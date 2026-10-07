// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_frame_gdi.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <map>
#include <mutex>
#include <vector>

#include "base/trace/event/process_trace.h"
#include "content/browser/present/map2d/software/map2d_gdi_fill.h"
#include "content/browser/present/map2d/software/map2d_gdi_raster.h"
#include "content/browser/present/map2d/software/map2d_gdi_style.h"
#include "content/browser/present/map2d/software/map2d_gdi_text.h"

namespace content {
namespace detail {

HBRUSH GdiPaintResourceCache::brush_for(COLORREF c) {
  std::lock_guard<std::mutex> lock(mu);
  auto it = brushes.find(c);
  if (it != brushes.end()) {
    return it->second;
  }
  HBRUSH b = CreateSolidBrush(c);
  if (b) {
    brushes.emplace(c, b);
  }
  return b;
}

HPEN GdiPaintResourceCache::pen_for(COLORREF c, int width) {
  const int w = std::max(1, width);
  const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 16) |
                       static_cast<uint64_t>(static_cast<uint16_t>(w));
  std::lock_guard<std::mutex> lock(mu);
  auto it = pens.find(key);
  if (it != pens.end()) {
    return it->second;
  }
  HPEN p = CreatePen(PS_SOLID, w, c);
  if (p) {
    pens.emplace(key, p);
  }
  return p;
}

HFONT GdiPaintResourceCache::font_for(int font_px, int esc) {
  const uint64_t key =
      (static_cast<uint64_t>(static_cast<uint32_t>(font_px)) << 32) |
      static_cast<uint64_t>(static_cast<uint32_t>(esc));
  std::lock_guard<std::mutex> lock(mu);
  auto it = fonts.find(key);
  if (it != fonts.end()) {
    return it->second;
  }
  const int height = -std::max(1, font_px);
  HFONT created = CreateFontW(
      height, 0, esc, esc, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  if (created) {
    fonts.emplace(key, created);
  }
  return created;
}

HFONT GdiPaintResourceCache::default_text() {
  std::lock_guard<std::mutex> lock(mu);
  if (default_text_font) {
    return default_text_font;
  }
  default_text_font = CreateFontW(
      -13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  return default_text_font;
}

GdiPaintResourceCache& gdi_paint_resources() {
  static GdiPaintResourceCache cache;
  return cache;
}

void paint_map_frame_gdi(
    HDC hdc, const vista::MapIR& frame, const vista::View& view,
    bool fill_background,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster,
    const std::function<bool(uint32_t texture_key, const uint8_t** rgba, int* w,
                             int* h)>& borrow_raster) {
  if (!hdc || view.width_px == 0 || view.height_px == 0) {
    return;
  }

  ViewXform xform;
  xform.bind(view);

  size_t n_raster = 0;
  size_t n_fill = 0;
  size_t n_line = 0;
  size_t n_line_mesh = 0;
  size_t n_line_stroke = 0;
  size_t n_text = 0;
  size_t n_circle = 0;
  for (const vista::DrawItem& item : frame.items) {
    switch (item.kind) {
      case vista::DrawKind::kRaster:
        ++n_raster;
        break;
      case vista::DrawKind::kFill:
        ++n_fill;
        break;
      case vista::DrawKind::kCircle:
        ++n_circle;
        break;
      case vista::DrawKind::kLine:
        ++n_line;
        if (item.indices.size() >= 3) {
          ++n_line_mesh;
        } else {
          ++n_line_stroke;
        }
        break;
      case vista::DrawKind::kText:
        ++n_text;
        break;
      default:
        break;
    }
  }
  std::fprintf(stderr,
               "map2d: gdi paint items=%zu raster=%zu fill=%zu circle=%zu "
               "line=%zu (mesh=%zu stroke=%zu) text=%zu view=%.2f..%.2f "
               "x %.2f..%.2f\n",
               frame.items.size(), n_raster, n_fill, n_circle, n_line,
               n_line_mesh, n_line_stroke, n_text, view.min_x, view.max_x,
               view.min_y, view.max_y);

  DibSurface dib{};
  bool dib_dirty = false;
  // Qualify: DibSurface is vista::raster::DibSurface — ADL also finds
  // vista::raster::try_bind_dib / fill_dib_solid.
  const bool have_dib = content::detail::try_bind_dib(hdc, &dib);
  GdiPaintResourceCache& resources = gdi_paint_resources();

  if (fill_background) {
    if (have_dib) {
      content::detail::fill_dib_solid(&dib, rgba_to_bgra(frame.background_rgba));
      dib_dirty = true;
    } else {
      HBRUSH bg = resources.brush_for(rgba_to_colorref(frame.background_rgba));
      RECT full = {0, 0, static_cast<LONG>(view.width_px),
                   static_cast<LONG>(view.height_px)};
      FillRect(hdc, &full, bg);
    }
  }

  // GDI fallback land path uses WINDING so opposing tess windings do not
  // cancel into cream holes (ALTERNATE even-odd fringe bug).
  SetPolyFillMode(hdc, WINDING);

  // MapIR text is already in view/bitmap pixels (Layout advances). Do not
  // DPI-scale CreateFont here — MulDiv(px, LOGPIXELSY, 96) on a HiDPI DC
  // draws glyphs larger than the metrics pen and stacks CJK within a label.
  HFONT text_font = resources.default_text();
  HGDIOBJ old_font =
      SelectObject(hdc, text_font ? text_font : GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);

  DcStyle style;
  FillBatch fill_batch;
  fill_batch.view_w = xform.width_px;
  fill_batch.view_h = xform.height_px;
  if (have_dib) {
    fill_batch.dib = &dib;
    fill_batch.dib_dirty = &dib_dirty;
  }
  StrokeBatch stroke_batch;
  auto sync_dib_before_gdi = [&]() {
    if (dib_dirty) {
      GdiFlush();
      dib_dirty = false;
    }
  };
  auto brush_for = [&](COLORREF c) -> HBRUSH { return resources.brush_for(c); };
  auto pen_for = [&](COLORREF c, int width) -> HPEN {
    return resources.pen_for(c, width);
  };
  struct StrokeRun {
    COLORREF color = 0;
    bool active = false;
    std::vector<std::vector<POINT>> paths;
  } stroke_run;
  auto flush_stroke_run = [&]() {
    if (!stroke_run.active || stroke_run.paths.empty()) {
      stroke_run.paths.clear();
      stroke_run.active = false;
      return;
    }
    // 2px cosmetic stroke: 1px GDI polylines staircase on diagonals.
    HPEN core_pen = pen_for(stroke_run.color, 2);
    for (const auto& path : stroke_run.paths) {
      stroke_batch.append(hdc, &style, core_pen, path);
    }
    stroke_batch.flush(hdc, &style);
    stroke_run.paths.clear();
    stroke_run.active = false;
  };
  auto flush_geometry = [&]() {
    flush_stroke_run();
    fill_batch.flush(hdc, &style);
    stroke_batch.flush(hdc, &style);
    sync_dib_before_gdi();
  };

  std::vector<POINT> pts;
  int64_t fill_us = 0;
  int64_t line_us = 0;
  int64_t text_us = 0;
  int64_t other_us = 0;
  auto add_us = [](int64_t* bucket, std::chrono::steady_clock::time_point t0) {
    *bucket += std::chrono::duration_cast<std::chrono::microseconds>(
                   std::chrono::steady_clock::now() - t0)
                   .count();
  };

  COLORREF last_halo = 0;
  COLORREF last_ink = 0;
  bool have_halo = false;
  bool have_ink = false;
  HFONT dc_font = text_font ? text_font
                            : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  bool in_text_run = false;
  HPEN null_pen = static_cast<HPEN>(GetStockObject(NULL_PEN));

  for (const vista::DrawItem& item : frame.items) {
    const auto t0 = std::chrono::steady_clock::now();
    const COLORREF color = rgba_to_colorref(item.rgba);
    switch (item.kind) {
      case vista::DrawKind::kFill:
      case vista::DrawKind::kCircle: {
        in_text_run = false;
        item_to_points(item, xform, &pts);
        // Same-color 1px outline was redundant stroke work; NULL_PEN keeps
        // land/water fills faithful while cutting pen GDI cost.
        // Always coalesce=true (triangle PolyPolygon). coalesce=false is the
        // line-mesh path (classify_tess_quad); large land fills (≥288 idx)
        // used to take it and punch rectangular ocean holes through China.
        fill_batch.append_tris(hdc, &style, brush_for(color), null_pen, color,
                               pts, item.indices, /*coalesce=*/true);
        add_us(&fill_us, t0);
        break;
      }
      case vista::DrawKind::kLine: {
        in_text_run = false;
        sync_dib_before_gdi();
        item_to_points(item, xform, &pts);
        // Tessellated lines: filled stroke quads (Layout width) + DIB edge AA.
        if (item.indices.size() >= 3) {
          flush_stroke_run();
          append_line_mesh(hdc, &style, &fill_batch, brush_for(color), null_pen,
                           color, pts, item.indices);
        } else {
          fill_batch.flush(hdc, &style);
          if (stroke_run.active && stroke_run.color != color) {
            flush_stroke_run();
          }
          if (!stroke_run.active) {
            stroke_run.color = color;
            stroke_run.active = true;
          }
          stroke_run.paths.push_back(pts);
        }
        add_us(&line_us, t0);
        break;
      }
      case vista::DrawKind::kRaster: {
        in_text_run = false;
        flush_geometry();
        sync_dib_before_gdi();
        style.invalidate();
        item_to_points(item, xform, &pts);
        const uint8_t* rgba_ptr = nullptr;
        std::vector<uint8_t> rgba_owned;
        int tw = 0;
        int th = 0;
        bool loaded = false;
        if (borrow_raster &&
            borrow_raster(item.codepoint, &rgba_ptr, &tw, &th)) {
          loaded = rgba_ptr != nullptr;
        } else if (load_raster &&
                   load_raster(item.codepoint, &rgba_owned, &tw, &th)) {
          rgba_ptr = rgba_owned.data();
          loaded = true;
        }
        if (loaded && blit_rgba_quad(hdc, pts, rgba_ptr, tw, th, item.opacity,
                                     item.blend)) {
          add_us(&other_us, t0);
          break;
        }
        std::fprintf(stderr,
                     "map2d: raster blit miss load=%d tw=%d th=%d pts=%zu "
                     "key=0x%08x opacity=%.2f\n",
                     loaded ? 1 : 0, tw, th, pts.size(), item.codepoint,
                     item.opacity);
        if (pts.size() >= 3) {
          fill_batch.append_tris(hdc, &style, brush_for(color), null_pen, color,
                                 pts, item.indices, /*coalesce=*/true);
          fill_batch.flush(hdc, &style);
        }
        add_us(&other_us, t0);
        break;
      }
      case vista::DrawKind::kIcon: {
        add_us(&other_us, t0);
        break;
      }
      case vista::DrawKind::kText: {
        if (!in_text_run) {
          flush_geometry();
          sync_dib_before_gdi();
          AbortPath(hdc);
          SelectClipRgn(hdc, nullptr);
          SelectObject(hdc, GetStockObject(NULL_BRUSH));
          SelectObject(hdc, GetStockObject(NULL_PEN));
          style.invalidate();
          if (dc_font) {
            SelectObject(hdc, dc_font);
          }
          in_text_run = true;
        }
        paint_text_glyph(hdc, &style, text_font, &dc_font, &resources, item,
                         xform, &last_halo, &last_ink, &have_halo, &have_ink);
        add_us(&text_us, t0);
        break;
      }
    }
  }
  flush_geometry();

  std::fprintf(stderr,
               "map2d: gdi batch fill_us=%lld line_us=%lld text_us=%lld "
               "other_us=%lld fill_flushes=%zu dib_tris=%zu dib=%d "
               "mesh_polys=%zu mesh_flushes=%zu brushes=%zu fonts=%zu\n",
               static_cast<long long>(fill_us), static_cast<long long>(line_us),
               static_cast<long long>(text_us),
               static_cast<long long>(other_us), fill_batch.flush_count,
               fill_batch.dib_tri_count, have_dib ? 1 : 0,
               fill_batch.tri_draw_count, fill_batch.mesh_flush_count,
               static_cast<size_t>(0), static_cast<size_t>(0));

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
  // Process-wide GdiPaintResourceCache owns brushes/pens/fonts — do not delete.
}

}  // namespace detail
}  // namespace content
