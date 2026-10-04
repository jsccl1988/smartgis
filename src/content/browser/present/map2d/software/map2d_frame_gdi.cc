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
#include "vista/map/multiply.h"

#pragma comment(lib, "Msimg32.lib")

namespace content {
namespace detail {
namespace {

inline LONG iround(double v) {
  return static_cast<LONG>(v + (v >= 0.0 ? 0.5 : -0.5));
}

// Precomputed world→view scale so mesh vertices skip per-point divides.
struct ViewXform {
  double min_x = 0.0;
  double max_y = 0.0;
  double sx = 1.0;
  double sy = 1.0;
  int width_px = 0;
  int height_px = 0;

  void bind(const vista::View& view) {
    width_px = static_cast<int>(view.width_px);
    height_px = static_cast<int>(view.height_px);
    min_x = view.min_x;
    max_y = view.max_y;
    const double dx = view.max_x - view.min_x;
    const double dy = view.max_y - view.min_y;
    sx = (dx == 0.0) ? 0.0 : static_cast<double>(view.width_px) / dx;
    sy = (dy == 0.0) ? 0.0 : static_cast<double>(view.height_px) / dy;
  }

  POINT map(float x, float y, bool pixel_space) const {
    if (pixel_space) {
      return POINT{iround(static_cast<double>(x)),
                   iround(static_cast<double>(y))};
    }
    return POINT{iround((static_cast<double>(x) - min_x) * sx),
                 iround((max_y - static_cast<double>(y)) * sy)};
  }
};

void item_to_points(const vista::DrawItem& item, const ViewXform& xform,
                    std::vector<POINT>* out) {
  if (!out) {
    return;
  }
  out->clear();
  out->reserve(item.vertices.size());
  const bool pixel = item.pixel_space;
  for (const vista::Vertex& v : item.vertices) {
    out->push_back(xform.map(v.x, v.y, pixel));
  }
}

// Sticky SelectObject: avoid restore/select per DrawItem when brush/pen match.
struct DcStyle {
  HGDIOBJ brush = nullptr;
  HGDIOBJ pen = nullptr;
  bool known = false;

  void ensure(HDC hdc, HBRUSH brush_in, HPEN pen_in) {
    HGDIOBJ b = brush_in ? static_cast<HGDIOBJ>(brush_in)
                         : GetStockObject(NULL_BRUSH);
    HGDIOBJ p =
        pen_in ? static_cast<HGDIOBJ>(pen_in) : GetStockObject(NULL_PEN);
    if (!known || brush != b) {
      SelectObject(hdc, b);
      brush = b;
    }
    if (!known || pen != p) {
      SelectObject(hdc, p);
      pen = p;
    }
    known = true;
  }

  void invalidate() { known = false; }
};

inline bool poly_outside_view(const POINT* p, INT n, int w, int h) {
  if (w <= 0 || h <= 0 || n <= 0 || !p) {
    return false;
  }
  LONG min_x = p[0].x;
  LONG max_x = p[0].x;
  LONG min_y = p[0].y;
  LONG max_y = p[0].y;
  for (INT i = 1; i < n; ++i) {
    min_x = (std::min)(min_x, p[i].x);
    max_x = (std::max)(max_x, p[i].x);
    min_y = (std::min)(min_y, p[i].y);
    max_y = (std::max)(max_y, p[i].y);
  }
  return max_x < 0 || min_x > w || max_y < 0 || min_y > h;
}

inline bool tri_zero_area(const POINT& a, const POINT& b, const POINT& c) {
  return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x) == 0;
}

// Line tessellation emits two triangles per segment (A,B,C / B,D,C) or a
// miter fan (A,B,C / A,C,D). Fold those into one convex quad for GDI.
enum class TessQuad : uint8_t { kNone, kSegment, kFan };

TessQuad classify_tess_quad(const uint32_t* idx, size_t nvert, uint32_t* q) {
  const uint32_t a = idx[0];
  const uint32_t b = idx[1];
  const uint32_t c = idx[2];
  const uint32_t d = idx[3];
  const uint32_t e = idx[4];
  const uint32_t f = idx[5];
  if (a >= nvert || b >= nvert || c >= nvert || d >= nvert || e >= nvert ||
      f >= nvert) {
    return TessQuad::kNone;
  }
  // emit_segment_quad: (base, base+1, base+2) (base+1, base+3, base+2)
  if (d == b && f == c) {
    q[0] = a;
    q[1] = b;
    q[2] = e;
    q[3] = c;
    return TessQuad::kSegment;
  }
  // miter: (base, base+1, base+2) (base, base+2, base+3)
  if (d == a && e == c) {
    q[0] = a;
    q[1] = b;
    q[2] = c;
    q[3] = f;
    return TessQuad::kFan;
  }
  return TessQuad::kNone;
}

inline POINT midpoint(const POINT& a, const POINT& b) {
  return POINT{(a.x + b.x) / 2, (a.y + b.y) / 2};
}

inline bool near_point(const POINT& a, const POINT& b) {
  const LONG dx = a.x > b.x ? a.x - b.x : b.x - a.x;
  const LONG dy = a.y > b.y ? a.y - b.y : b.y - a.y;
  return dx <= 1 && dy <= 1;
}

// Coalesce same-style fill triangles. Land fills use PolyPolygon (few large
// polys). Tessellated line meshes stay on sticky SelectObject + chunked
// PolyPolygon — a single mega batch of coastline strips is pathological in
// GDI (~10× slower). Segment quads (2 tris) emit as 4-gons.
struct FillBatch {
  HBRUSH brush = nullptr;
  HPEN pen = nullptr;
  bool active = false;
  bool use_poly_polygon = true;
  std::vector<POINT> points;
  std::vector<INT> counts;
  std::vector<POINT> mesh_pts;
  std::vector<INT> mesh_counts;
  size_t flush_count = 0;
  size_t tri_draw_count = 0;
  size_t mesh_flush_count = 0;
  int view_w = 0;
  int view_h = 0;

  // Keep PolyPolygon batches modest; large coastline meshes use chunked path.
  static constexpr size_t kMaxPoints = 3072;
  static constexpr size_t kMaxPolys = 512;
  static constexpr size_t kMeshChunkPolys = 96;

  void reset_payload() {
    points.clear();
    counts.clear();
  }

  void flush_mesh_chunk(HDC hdc) {
    if (mesh_counts.empty()) {
      return;
    }
    if (mesh_counts.size() == 1) {
      Polygon(hdc, mesh_pts.data(), mesh_counts[0]);
    } else {
      PolyPolygon(hdc, mesh_pts.data(), mesh_counts.data(),
                  static_cast<int>(mesh_counts.size()));
    }
    ++mesh_flush_count;
    mesh_pts.clear();
    mesh_counts.clear();
  }

  void emit_mesh_poly(HDC hdc, const POINT* poly, INT n) {
    if (n < 3 || !poly) {
      return;
    }
    if (n == 3 && tri_zero_area(poly[0], poly[1], poly[2])) {
      return;
    }
    if (poly_outside_view(poly, n, view_w, view_h)) {
      return;
    }
    mesh_pts.insert(mesh_pts.end(), poly, poly + n);
    mesh_counts.push_back(n);
    ++tri_draw_count;
    if (mesh_counts.size() >= kMeshChunkPolys) {
      flush_mesh_chunk(hdc);
    }
  }

  void flush(HDC hdc, DcStyle* style) {
    if (active && !use_poly_polygon) {
      style->ensure(hdc, brush, pen);
      flush_mesh_chunk(hdc);
      active = false;
      reset_payload();
      return;
    }
    if (!active || counts.empty()) {
      reset_payload();
      active = false;
      return;
    }
    style->ensure(hdc, brush, pen);
    // Coalesced land tris: draw each Polygon alone. One PolyPolygon over a
    // triangle soup uses even-odd / opposing-winding cancel and punches
    // cream/ocean holes at province overlaps (china coastal fringe).
    size_t cursor = 0;
    for (INT n : counts) {
      if (n >= 2 && cursor + static_cast<size_t>(n) <= points.size()) {
        Polygon(hdc, points.data() + cursor, n);
      }
      cursor += static_cast<size_t>(n);
    }
    ++flush_count;
    reset_payload();
    active = false;
  }

  void append_tris(HDC hdc, DcStyle* style, HBRUSH b, HPEN p,
                   const std::vector<POINT>& pts,
                   const std::vector<uint32_t>& indices, bool coalesce) {
    if (pts.empty() || indices.size() < 3) {
      return;
    }
    if (active &&
        (brush != b || pen != p || use_poly_polygon != coalesce)) {
      flush(hdc, style);
    }
    if (!active) {
      brush = b;
      pen = p;
      use_poly_polygon = coalesce;
      active = true;
    }

    // Tessellated line meshes: sticky style + small PolyPolygon chunks.
    // Fold segment-quad index pairs into 4-gons (half the GDI polys).
    if (!coalesce) {
      style->ensure(hdc, brush, pen);
      if (mesh_pts.capacity() < kMeshChunkPolys * 4) {
        mesh_pts.reserve(kMeshChunkPolys * 4);
        mesh_counts.reserve(kMeshChunkPolys);
      }
      const size_t nvert = pts.size();
      size_t i = 0;
      while (i + 2 < indices.size()) {
        if (i + 5 < indices.size()) {
          uint32_t q[4] = {};
          const TessQuad kind =
              classify_tess_quad(indices.data() + i, nvert, q);
          if (kind != TessQuad::kNone) {
            const POINT quad[4] = {pts[q[0]], pts[q[1]], pts[q[2]], pts[q[3]]};
            emit_mesh_poly(hdc, quad, 4);
            i += 6;
            continue;
          }
        }
        const uint32_t a = indices[i];
        const uint32_t bi = indices[i + 1];
        const uint32_t c = indices[i + 2];
        i += 3;
        if (a >= nvert || bi >= nvert || c >= nvert) {
          continue;
        }
        const POINT tri[3] = {pts[a], pts[bi], pts[c]};
        emit_mesh_poly(hdc, tri, 3);
      }
      return;
    }

    for (size_t i = 0; i + 2 < indices.size(); i += 3) {
      if (counts.size() >= kMaxPolys || points.size() + 3 > kMaxPoints) {
        flush(hdc, style);
        brush = b;
        pen = p;
        use_poly_polygon = coalesce;
        active = true;
      }
      const uint32_t a = indices[i];
      const uint32_t bi = indices[i + 1];
      const uint32_t c = indices[i + 2];
      if (a >= pts.size() || bi >= pts.size() || c >= pts.size()) {
        continue;
      }
      points.push_back(pts[a]);
      points.push_back(pts[bi]);
      points.push_back(pts[c]);
      counts.push_back(3);
    }
  }
};



// Coalesce same-pen polylines into PolyPolyline (cosmetic tess centerlines).
struct StrokeBatch {
  HPEN pen = nullptr;
  bool active = false;
  bool credit_mesh = false;
  std::vector<POINT> points;
  std::vector<DWORD> counts;
  size_t flush_count = 0;
  size_t mesh_flush_count = 0;

  static constexpr size_t kMaxPoints = 12288;
  static constexpr size_t kMaxPolys = 4096;

  void reset_payload() {
    points.clear();
    counts.clear();
  }

  void flush(HDC hdc, DcStyle* style) {
    if (!active || counts.empty()) {
      reset_payload();
      active = false;
      credit_mesh = false;
      return;
    }
    style->ensure(hdc, nullptr, pen);
    if (counts.size() == 1) {
      Polyline(hdc, points.data(), static_cast<int>(counts[0]));
    } else {
      PolyPolyline(hdc, points.data(), counts.data(),
                   static_cast<DWORD>(counts.size()));
    }
    ++flush_count;
    if (credit_mesh) {
      ++mesh_flush_count;
    }
    reset_payload();
    active = false;
    credit_mesh = false;
  }

  void append(HDC hdc, DcStyle* style, HPEN p, const std::vector<POINT>& pts,
              bool as_mesh = false) {
    if (pts.size() < 2 || !p) {
      return;
    }
    if (active && (pen != p || credit_mesh != as_mesh)) {
      flush(hdc, style);
    }
    if (!active) {
      pen = p;
      active = true;
      credit_mesh = as_mesh;
    }
    if (counts.size() >= kMaxPolys ||
        points.size() + pts.size() > kMaxPoints) {
      flush(hdc, style);
      pen = p;
      active = true;
      credit_mesh = as_mesh;
    }
    points.insert(points.end(), pts.begin(), pts.end());
    counts.push_back(static_cast<DWORD>(pts.size()));
  }
};

// Stretch tightly packed RGBA8 into the axis-aligned bbox of |pts|.
// kMultiply bakes luma into the coverage then multiplies the snapped land.
// kOver is AlphaBlend (SRC_OVER).
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

  // Nearest sample: hillshade is low-frequency relief. Bilinear 4-tap at
  // dest resolution dominated software paint (~400ms at 640, worse at 1280).
  auto sample_rgba = [&](float u, float v, float* out_r, float* out_g,
                         float* out_b, float* out_a) {
    int ix = static_cast<int>(u * static_cast<float>(tw));
    int iy = static_cast<int>(v * static_cast<float>(th));
    ix = (std::max)(0, (std::min)(tw - 1, ix));
    iy = (std::max)(0, (std::min)(th - 1, iy));
    const size_t so =
        (static_cast<size_t>(iy) * static_cast<size_t>(tw) +
         static_cast<size_t>(ix)) *
        4u;
    const float a = static_cast<float>(rgba[so + 3]);
    if (a < 160.f) {
      *out_r = *out_g = *out_b = 0.f;
      *out_a = 0.f;
      return;
    }
    *out_r = static_cast<float>(rgba[so + 0]);
    *out_g = static_cast<float>(rgba[so + 1]);
    *out_b = static_cast<float>(rgba[so + 2]);
    *out_a = 255.f;
  };

  auto* dest = static_cast<uint8_t*>(dest_bits);
  const float inv_dst_w = dst_w > 1 ? 1.f / static_cast<float>(dst_w) : 1.f;
  const float inv_dst_h = dst_h > 1 ? 1.f / static_cast<float>(dst_h) : 1.f;
  std::vector<uint8_t> coverage(
      static_cast<size_t>(dst_w) * static_cast<size_t>(dst_h) * 4u);
  for (int dy = 0; dy < dst_h; ++dy) {
    const float v = (static_cast<float>(dy) + 0.5f) * inv_dst_h;
    for (int dx = 0; dx < dst_w; ++dx) {
      const float u = (static_cast<float>(dx) + 0.5f) * inv_dst_w;
      float sr = 0.f;
      float sg = 0.f;
      float sb = 0.f;
      float sa = 0.f;
      sample_rgba(u, v, &sr, &sg, &sb, &sa);
      const size_t o =
          (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
           static_cast<size_t>(dx)) *
          4u;
      coverage[o + 0] = static_cast<uint8_t>((std::min)(255.f, sr + 0.5f));
      coverage[o + 1] = static_cast<uint8_t>((std::min)(255.f, sg + 0.5f));
      coverage[o + 2] = static_cast<uint8_t>((std::min)(255.f, sb + 0.5f));
      coverage[o + 3] = static_cast<uint8_t>((std::min)(255.f, sa + 0.5f));
    }
  }

  BOOL ok = FALSE;
  if (blend == vista::DrawBlend::kMultiply) {
    vista::apply_multiply_coverage(coverage, k);
    for (int dy = 0; dy < dst_h; ++dy) {
      for (int dx = 0; dx < dst_w; ++dx) {
        const size_t o =
            (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
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
        const float m = static_cast<float>(coverage[o]) / 255.f;
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
        const size_t o =
            (static_cast<size_t>(dy) * static_cast<size_t>(dst_w) +
             static_cast<size_t>(dx)) *
            4u;
        const float af =
            (static_cast<float>(coverage[o + 3]) / 255.f) * k;
        const auto au = static_cast<uint8_t>(
            (std::min)(255.f, af * 255.f + 0.5f));
        dest[o + 0] = static_cast<uint8_t>(
            (static_cast<unsigned>(coverage[o + 2]) * au) / 255u);
        dest[o + 1] = static_cast<uint8_t>(
            (static_cast<unsigned>(coverage[o + 1]) * au) / 255u);
        dest[o + 2] = static_cast<uint8_t>(
            (static_cast<unsigned>(coverage[o + 0]) * au) / 255u);
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
  DeleteObject(dest_dib);
  DeleteDC(mem);
  return ok != FALSE;
}

void paint_text_glyph(HDC hdc, DcStyle* style, HFONT default_font,
                      HFONT* dc_font, std::map<uint64_t, HFONT>* font_cache,
                      const vista::DrawItem& item, const ViewXform& xform,
                      COLORREF* last_halo, COLORREF* last_ink, bool* have_halo,
                      bool* have_ink) {
  const uint32_t cp = item.codepoint;
  if (cp == 0 || cp > 0x10ffff) {
    return;
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

  POINT anchor = xform.map(item.anchor_x, item.anchor_y, true);
  if (!item.vertices.empty()) {
    anchor = xform.map(item.vertices.front().x, item.vertices.front().y,
                       item.pixel_space);
  }
  const int ax = static_cast<int>(anchor.x);
  const int ay = static_cast<int>(anchor.y);

  const int font_px =
      item.text_size_px > 0.5f ? static_cast<int>(std::lround(item.text_size_px))
                               : 13;
  const double deg = item.angle_rad * (180.0 / 3.14159265358979323846);
  const int esc =
      std::fabs(deg) > 0.5 ? static_cast<int>(std::lround(-deg * 10.0)) : 0;

  HFONT glyph_font = default_font;
  if (esc != 0 || font_px != 13) {
    const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(font_px))
                          << 32) |
                         static_cast<uint64_t>(static_cast<uint32_t>(esc));
    auto it = font_cache->find(key);
    if (it == font_cache->end()) {
      auto font_height = [](int px) { return -std::max(1, px); };
      HFONT created =
          CreateFontW(font_height(font_px), 0, esc, esc, FW_NORMAL, FALSE,
                      FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                      CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                      DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
      it = font_cache->emplace(key, created).first;
    }
    glyph_font = it->second ? it->second : default_font;
  }
  if (glyph_font && dc_font && glyph_font != *dc_font) {
    // Font select breaks sticky brush/pen tracking on this DC.
    SelectObject(hdc, glyph_font);
    style->invalidate();
    *dc_font = glyph_font;
  }

  const COLORREF ink = rgba_to_colorref(item.rgba);
  const COLORREF halo =
      item.halo_width_px > 0.f
          ? rgba_to_colorref(item.halo_rgba ? item.halo_rgba : 0xffffffffu)
          : RGB(255, 255, 255);
  if (!*have_halo || *last_halo != halo) {
    SetTextColor(hdc, halo);
    *last_halo = halo;
    *have_halo = true;
    *have_ink = false;
  }
  // 4-neighbor halo keeps CJK readable; diagonals cost 4 extra TextOutW.
  const int halo_d[4][2] = {{-1, 0}, {1, 0}, {0, -1}, {0, 1}};
  for (const auto& d : halo_d) {
    TextOutW(hdc, ax + d[0], ay + d[1], utf16, utf16_n);
  }
  if (!*have_ink || *last_ink != ink) {
    SetTextColor(hdc, ink);
    *last_ink = ink;
    *have_ink = true;
    *have_halo = false;
  }
  TextOutW(hdc, ax, ay, utf16, utf16_n);
}

}  // namespace

void paint_map_frame_gdi(
    HDC hdc, const vista::MapIR& frame, const vista::View& view,
    bool fill_background,
    const std::function<bool(uint32_t texture_key, std::vector<uint8_t>* rgba,
                             int* w, int* h)>& load_raster) {
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

  if (fill_background) {
    HBRUSH bg = CreateSolidBrush(rgba_to_colorref(frame.background_rgba));
    RECT full = {0, 0, static_cast<LONG>(view.width_px),
                 static_cast<LONG>(view.height_px)};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  // Land fills coalesce many same-color tris into one PolyPolygon. GDI's
  // default ALTERNATE mode punches even-odd holes at shared province edges
  // and coastal overlaps (cream fringe, ocean under city labels, dark
  // cancel patches). WINDING matches scenic::map2d_engine and keeps the union.
  SetPolyFillMode(hdc, WINDING);

  std::map<COLORREF, HBRUSH> brushes;
  std::map<uint64_t, HFONT> fonts;
  auto brush_for = [&](COLORREF c) -> HBRUSH {
    auto it = brushes.find(c);
    if (it != brushes.end()) {
      return it->second;
    }
    HBRUSH b = CreateSolidBrush(c);
    brushes.emplace(c, b);
    return b;
  };

  // MapIR text is already in view/bitmap pixels (Layout advances). Do not
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

  DcStyle style;
  FillBatch fill_batch;
  fill_batch.view_w = xform.width_px;
  fill_batch.view_h = xform.height_px;
  StrokeBatch stroke_batch;
  std::map<uint64_t, HPEN> pens;
  auto pen_for = [&](COLORREF c, int width) -> HPEN {
    const int w = std::max(1, width);
    const uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 16) |
        static_cast<uint64_t>(static_cast<uint16_t>(w));
    auto it = pens.find(key);
    if (it != pens.end()) {
      return it->second;
    }
    HPEN p = CreatePen(PS_SOLID, 1, c);
    if (!p) {
      p = CreatePen(PS_SOLID, w, c);
    }
    pens.emplace(key, p);
    return p;
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
    HPEN core_pen = pen_for(stroke_run.color, 1);
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
  };

  std::vector<POINT> pts;
  std::vector<POINT> chain;
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
        fill_batch.append_tris(hdc, &style, brush_for(color), null_pen, pts,
                               item.indices, /*coalesce=*/true);
        add_us(&fill_us, t0);
        break;
      }
      case vista::DrawKind::kLine: {
        in_text_run = false;
        item_to_points(item, xform, &pts);
        // Tessellated lines: cosmetic 1px PolyPolyline of emit_segment_quad
        // centerlines (P2d unit china). If no segment classifies, chunked mesh.
        if (item.indices.size() >= 3) {
          flush_stroke_run();
          const size_t nvert = pts.size();
          const std::vector<uint32_t>& indices = item.indices;
          chain.clear();
          size_t n_seg = 0;
          auto flush_chain = [&]() {
            if (chain.size() >= 2) {
              stroke_batch.append(hdc, &style, pen_for(color, 1), chain,
                                  /*as_mesh=*/true);
            }
            chain.clear();
          };
          size_t i = 0;
          while (i + 2 < indices.size()) {
            if (i + 5 < indices.size()) {
              uint32_t q[4] = {};
              const TessQuad kind =
                  classify_tess_quad(indices.data() + i, nvert, q);
              if (kind == TessQuad::kSegment) {
                const POINT m0 = midpoint(pts[q[0]], pts[q[1]]);
                const POINT m1 = midpoint(pts[q[3]], pts[q[2]]);
                if (poly_outside_view(&m0, 1, xform.width_px, xform.height_px) &&
                    poly_outside_view(&m1, 1, xform.width_px, xform.height_px)) {
                  i += 6;
                  continue;
                }
                if (near_point(m0, m1)) {
                  i += 6;
                  continue;
                }
                if (!chain.empty() && !near_point(chain.back(), m0)) {
                  flush_chain();
                }
                if (chain.empty()) {
                  chain.push_back(m0);
                }
                chain.push_back(m1);
                ++n_seg;
                i += 6;
                continue;
              }
              if (kind == TessQuad::kFan) {
                i += 6;
                continue;
              }
            }
            i += 3;
          }
          flush_chain();
          if (n_seg == 0) {
            fill_batch.append_tris(hdc, &style, brush_for(color), null_pen, pts,
                                   indices, /*coalesce=*/false);
          }
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
        style.invalidate();
        item_to_points(item, xform, &pts);
        std::vector<uint8_t> rgba;
        int tw = 0;
        int th = 0;
        const bool loaded =
            load_raster && load_raster(item.codepoint, &rgba, &tw, &th);
        if (loaded && blit_rgba_quad(hdc, pts, rgba, tw, th, item.opacity,
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
          fill_batch.append_tris(hdc, &style, brush_for(color), null_pen, pts,
                                 item.indices, /*coalesce=*/true);
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
        paint_text_glyph(hdc, &style, text_font, &dc_font, &fonts, item, xform,
                         &last_halo, &last_ink, &have_halo, &have_ink);
        add_us(&text_us, t0);
        break;
      }
    }
  }
  flush_geometry();

  std::fprintf(stderr,
               "map2d: gdi batch fill_us=%lld line_us=%lld text_us=%lld "
               "other_us=%lld fill_flushes=%zu "
               "mesh_polys=%zu mesh_flushes=%zu brushes=%zu fonts=%zu\n",
               static_cast<long long>(fill_us), static_cast<long long>(line_us),
               static_cast<long long>(text_us),
               static_cast<long long>(other_us), fill_batch.flush_count,
               fill_batch.tri_draw_count, fill_batch.mesh_flush_count,
               brushes.size(), fonts.size());

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
  for (auto& kv : fonts) {
    if (kv.second) {
      DeleteObject(kv.second);
    }
  }
  for (auto& kv : brushes) {
    DeleteObject(kv.second);
  }
}

}  // namespace detail
}  // namespace content
