// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/style/paint_resolve.h"

#include "sdb/style/expression.h"
#include "sdb/style/json_mini.h"
#include "sdb/style/symbol_library.h"

#include <cstdio>
#include <cstdlib>

namespace sdb {
namespace style {
namespace {

int hex_nibble(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

bool parse_hex(const std::string& text, uint32_t* out) {
  if (text.empty() || text[0] != '#') {
    return false;
  }
  const char* p = text.c_str() + 1;
  size_t n = text.size() - 1;
  auto read = [&](size_t i) -> int { return hex_nibble(p[i]); };
  if (n == 3) {
    int r = read(0);
    int g = read(1);
    int b = read(2);
    if (r < 0 || g < 0 || b < 0) {
      return false;
    }
    *out = 0xFF000000u | (static_cast<uint32_t>(r * 17) << 16) |
           (static_cast<uint32_t>(g * 17) << 8) | static_cast<uint32_t>(b * 17);
    return true;
  }
  if (n == 6) {
    int r = (read(0) << 4) | read(1);
    int g = (read(2) << 4) | read(3);
    int b = (read(4) << 4) | read(5);
    if (r < 0 || g < 0 || b < 0) {
      return false;
    }
    *out = 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
  }
  if (n == 8) {
    int a = (read(0) << 4) | read(1);
    int r = (read(2) << 4) | read(3);
    int g = (read(4) << 4) | read(5);
    int b = (read(6) << 4) | read(7);
    if (a < 0 || r < 0 || g < 0 || b < 0) {
      return false;
    }
    *out = (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(r) << 16) |
           (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
    return true;
  }
  return false;
}

bool parse_rgb_fn(const std::string& text, uint32_t* out) {
  if (text.size() < 10 || text.compare(0, 4, "rgb(") != 0 || text.back() != ')') {
    return false;
  }
  int r = 0;
  int g = 0;
  int b = 0;
  if (std::sscanf(text.c_str(), "rgb(%d,%d,%d)", &r, &g, &b) != 3 &&
      std::sscanf(text.c_str(), "rgb(%d, %d, %d)", &r, &g, &b) != 3) {
    return false;
  }
  if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) {
    return false;
  }
  *out = 0xFF000000u | (static_cast<uint32_t>(r) << 16) |
         (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b);
  return true;
}

std::string map_get(const std::map<std::string, std::string>& m,
                    const char* key) {
  auto it = m.find(key);
  return it == m.end() ? std::string() : it->second;
}

// Resolve a paint/layout entry: evaluate expression arrays to constants.
std::string resolve_raw(const std::string& raw,
                        const AttrMap& attrs,
                        double zoom) {
  if (raw.empty()) {
    return raw;
  }
  if (looks_like_expression(raw)) {
    ExprValue ev;
    if (eval_expression(raw, attrs, zoom, &ev)) {
      return ev.as_string();
    }
  }
  return raw;
}

float parse_float_text(const std::string& text, float fallback) {
  if (text.empty()) {
    return fallback;
  }
  char* stop = nullptr;
  float v = static_cast<float>(std::strtod(text.c_str(), &stop));
  if (stop == text.c_str()) {
    return fallback;
  }
  return v;
}

float resolve_float(const std::map<std::string, std::string>& m,
                    const char* key,
                    const AttrMap& attrs,
                    double zoom,
                    float fallback) {
  const std::string raw = map_get(m, key);
  if (raw.empty()) {
    return fallback;
  }
  return parse_float_text(resolve_raw(raw, attrs, zoom), fallback);
}

std::string resolve_string(const std::map<std::string, std::string>& m,
                           const char* key,
                           const AttrMap& attrs,
                           double zoom) {
  return resolve_raw(map_get(m, key), attrs, zoom);
}

void parse_float_array(const std::string& raw, std::vector<float>* out) {
  out->clear();
  if (raw.empty() || raw[0] != '[') {
    return;
  }
  detail::JsonValue root;
  if (!detail::parse_json(raw.data(), raw.size(), &root) || !root.is_array()) {
    return;
  }
  // Skip expression forms; only accept literal number arrays.
  if (!root.a.empty() && root.a[0].is_string()) {
    return;
  }
  for (const auto& e : root.a) {
    if (e.is_number()) {
      out->push_back(static_cast<float>(e.n));
    }
  }
}

void parse_xy_offset(const std::string& raw, float* x, float* y) {
  if (raw.empty() || raw[0] != '[') {
    return;
  }
  detail::JsonValue root;
  if (!detail::parse_json(raw.data(), raw.size(), &root) || !root.is_array() ||
      root.a.size() < 2) {
    return;
  }
  if (root.a[0].is_number() && root.a[1].is_number()) {
    *x = static_cast<float>(root.a[0].n);
    *y = static_cast<float>(root.a[1].n);
  }
}

COLORREF argb_to_colorref(uint32_t argb) {
  const int r = static_cast<int>((argb >> 16) & 0xFF);
  const int g = static_cast<int>((argb >> 8) & 0xFF);
  const int b = static_cast<int>(argb & 0xFF);
  return RGB(r, g, b);
}

}  // namespace

bool parse_color(const std::string& text, uint32_t* out_argb) {
  if (!out_argb) {
    return false;
  }
  if (parse_hex(text, out_argb) || parse_rgb_fn(text, out_argb)) {
    return true;
  }
  return false;
}

void fill_resolved_paint(const StyleLayer& layer,
                         const SymbolLibrary* library,
                         const AttrMap& attrs,
                         double zoom,
                         ResolvedPaint* out) {
  *out = ResolvedPaint();
  out->layer_id = layer.id;
  out->type = layer.type;

  auto apply_color = [&](const char* key, uint32_t* dest) {
    const std::string c = resolve_string(layer.paint, key, attrs, zoom);
    uint32_t argb = 0;
    if (!c.empty() && parse_color(c, &argb)) {
      *dest = argb;
    }
  };

  apply_color("fill-color", &out->fill_color);
  out->fill_opacity =
      resolve_float(layer.paint, "fill-opacity", attrs, zoom, 1.f);
  out->fill_pattern = resolve_string(layer.paint, "fill-pattern", attrs, zoom);

  apply_color("line-color", &out->line_color);
  out->line_width = resolve_float(layer.paint, "line-width", attrs, zoom, 1.f);
  out->line_opacity =
      resolve_float(layer.paint, "line-opacity", attrs, zoom, 1.f);
  parse_float_array(map_get(layer.paint, "line-dasharray"),
                    &out->line_dasharray);
  out->line_cap = resolve_string(layer.layout, "line-cap", attrs, zoom);
  if (out->line_cap.empty()) {
    out->line_cap = resolve_string(layer.paint, "line-cap", attrs, zoom);
  }
  out->line_join = resolve_string(layer.layout, "line-join", attrs, zoom);
  if (out->line_join.empty()) {
    out->line_join = resolve_string(layer.paint, "line-join", attrs, zoom);
  }

  apply_color("circle-color", &out->circle_color);
  out->circle_radius =
      resolve_float(layer.paint, "circle-radius", attrs, zoom, 5.f);
  out->circle_opacity =
      resolve_float(layer.paint, "circle-opacity", attrs, zoom, 1.f);

  apply_color("background-color", &out->background_color);
  out->background_opacity =
      resolve_float(layer.paint, "background-opacity", attrs, zoom, 1.f);
  out->raster_opacity =
      resolve_float(layer.paint, "raster-opacity", attrs, zoom, 1.f);

  out->icon_image = resolve_string(layer.layout, "icon-image", attrs, zoom);
  if (out->icon_image.empty()) {
    out->icon_image = resolve_string(layer.paint, "icon-image", attrs, zoom);
  }
  out->text_field = resolve_string(layer.layout, "text-field", attrs, zoom);
  out->icon_size = resolve_float(layer.layout, "icon-size", attrs, zoom, 1.f);
  if (out->icon_size <= 0.f) {
    out->icon_size = resolve_float(layer.paint, "icon-size", attrs, zoom, 1.f);
  }
  out->text_size = resolve_float(layer.layout, "text-size", attrs, zoom, 16.f);
  if (out->text_size <= 0.f) {
    out->text_size = resolve_float(layer.paint, "text-size", attrs, zoom, 16.f);
  }
  out->text_anchor = resolve_string(layer.layout, "text-anchor", attrs, zoom);
  {
    std::string offset = map_get(layer.layout, "icon-offset");
    if (offset.empty()) {
      offset = map_get(layer.paint, "icon-offset");
    }
    parse_xy_offset(offset, &out->icon_offset_x, &out->icon_offset_y);
  }

  if (library && !out->icon_image.empty() &&
      library->find(out->icon_image, &out->symbol)) {
    out->has_symbol = true;
  }
}

base::SmtStyle to_smt_style(const ResolvedPaint& paint, const char* name) {
  base::SmtPenDesc pen;
  base::SmtBrushDesc brush;
  base::SmtAnnotationDesc anno;
  base::SmtSymbolDesc symbol;

  pen.lPenColor = argb_to_colorref(paint.line_color);
  pen.fPenWidth = paint.line_width * 0.001f;
  pen.lPenStyle = PS_SOLID;

  brush.lBrushColor = argb_to_colorref(paint.fill_color);
  if (paint.type == LayerType::kCircle) {
    brush.lBrushColor = argb_to_colorref(paint.circle_color);
  }
  brush.brushTp = base::SmtBrushDesc::BT_Solid;
  brush.lBrushStyle = 0;

  if (!paint.text_field.empty()) {
    // Keep default annotation face; color follows line/fill.
    anno.lAnnoClr = pen.lPenColor;
  }

  symbol.fSymbolWidth = 0.4f * paint.icon_size;
  symbol.fSymbolHeight = 0.4f * paint.icon_size;
  symbol.lSymbolID = 0;

  const char* style_name = name && name[0] ? name : paint.layer_id.c_str();
  return base::SmtStyle(style_name, pen, brush, anno, symbol);
}

}  // namespace style
}  // namespace sdb
