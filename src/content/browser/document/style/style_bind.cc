// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/style/style_bind.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <mutex>

#include "gis/carto/style/style_document.h"
#include "gis/carto/style/style_rules.h"
#include "vista/frame/frame.h"

namespace content {
namespace detail {

double style_zoom_from_scale(double scale) {
  const double z = 8.0 + std::log2(std::max(scale, 1e-3));
  if (z < 0.0) {
    return 0.0;
  }
  if (z > 22.0) {
    return 22.0;
  }
  return z;
}

COLORREF argb_to_colorref(uint32_t argb) {
  return RGB(static_cast<int>((argb >> 16) & 0xFF),
             static_cast<int>((argb >> 8) & 0xFF),
             static_cast<int>(argb & 0xFF));
}

std::string argb_to_hex_rgb(uint32_t argb) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X",
                static_cast<unsigned>((argb >> 16) & 0xFF),
                static_cast<unsigned>((argb >> 8) & 0xFF),
                static_cast<unsigned>(argb & 0xFF));
  return buf;
}

std::string colorref_to_hex(COLORREF c) {
  char buf[16];
  std::snprintf(buf, sizeof(buf), "#%02X%02X%02X", GetRValue(c), GetGValue(c),
                GetBValue(c));
  return buf;
}

const gis::style::StyleDocument* embedded_carto_style_document() {
  static std::once_flag once;
  static std::shared_ptr<gis::style::StyleDocument> doc;
  std::call_once(once, [] {
    auto parsed = std::make_shared<gis::style::StyleDocument>();
    if (gis::style::parse_style_document(vista::default_carto_style_json(),
                                         parsed.get())) {
      doc = std::move(parsed);
    }
  });
  return doc.get();
}

bool read_file_bytes(const std::string& path, std::string* out) {
  if (!out || path.empty()) {
    return false;
  }
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
    return false;
  }
  if (std::fseek(f, 0, SEEK_END) != 0) {
    std::fclose(f);
    return false;
  }
  const long len = std::ftell(f);
  if (len < 0) {
    std::fclose(f);
    return false;
  }
  if (std::fseek(f, 0, SEEK_SET) != 0) {
    std::fclose(f);
    return false;
  }
  out->assign(static_cast<size_t>(len), '\0');
  const size_t n = std::fread(out->data(), 1, out->size(), f);
  std::fclose(f);
  return n == out->size();
}

void StyleBind::set_style_document(
    std::shared_ptr<gis::style::StyleDocument> doc) {
  style_doc_ = std::move(doc);
}

void StyleBind::clear_style_document() {
  style_doc_.reset();
}

bool StyleBind::load_style_path(const std::string& path) {
  std::string json;
  if (!read_file_bytes(path, &json)) {
    return false;
  }
  auto doc = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(json, doc.get())) {
    return false;
  }
  style_doc_ = std::move(doc);
  return true;
}

void StyleBind::set_basemap_provider(
    std::shared_ptr<gis::tile::TileProvider> provider) {
  basemap_ = std::move(provider);
}

void StyleBind::clear_basemap_provider() {
  basemap_.reset();
}

bool StyleBind::resolve_style_for_test(const std::string& source_layer,
                                       const gis::style::AttrMap& attrs,
                                       double zoom,
                                       gis::style::ResolvedPaint* out) const {
  if (!style_doc_ || !out) {
    return false;
  }
  return gis::style::resolve(*style_doc_, nullptr, attrs, zoom, source_layer,
                             out);
}

bool StyleBind::style_colors_for_feature(const MapLayer& layer,
                                         const MapFeature& f, double scale,
                                         COLORREF* fill, COLORREF* stroke,
                                         int* stroke_width) const {
  if (!style_doc_ || !fill || !stroke || !stroke_width) {
    return false;
  }
  gis::style::AttrMap attrs;
  for (const content::NamedField& field : f.fields) {
    attrs[field.name] = field.value;
  }
  gis::style::ResolvedPaint paint;
  if (!gis::style::resolve(*style_doc_, nullptr, attrs, style_zoom_from_scale(scale),
                           layer.name, &paint)) {
    return false;
  }
  switch (f.kind) {
    case GeomKind::kPolygon:
      *fill = argb_to_colorref(paint.fill_color);
      *stroke = RGB(196, 190, 176);
      *stroke_width = 1;
      return true;
    case GeomKind::kLine:
      *stroke = argb_to_colorref(paint.line_color);
      *fill = *stroke;
      *stroke_width =
          std::max(1, static_cast<int>(std::lround(paint.line_width)));
      return true;
    case GeomKind::kPoint:
      *fill = argb_to_colorref(paint.circle_color);
      *stroke = *fill;
      *stroke_width = 1;
      return true;
    default:
      return false;
  }
}

}  // namespace detail
}  // namespace content
