// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/style/paint_resolve.h"

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

float parse_float(const std::map<std::string, std::string>& m,
                  const char* key,
                  float fallback) {
  auto it = m.find(key);
  if (it == m.end()) {
    return fallback;
  }
  char* stop = nullptr;
  float v = static_cast<float>(std::strtod(it->second.c_str(), &stop));
  if (stop == it->second.c_str()) {
    return fallback;
  }
  return v;
}

std::string map_get(const std::map<std::string, std::string>& m,
                    const char* key) {
  auto it = m.find(key);
  return it == m.end() ? std::string() : it->second;
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
                         ResolvedPaint* out) {
  *out = ResolvedPaint();
  out->layer_id = layer.id;
  out->type = layer.type;

  auto apply_color = [&](const char* key, uint32_t* dest) {
    const std::string c = map_get(layer.paint, key);
    uint32_t argb = 0;
    if (!c.empty() && parse_color(c, &argb)) {
      *dest = argb;
    }
  };

  apply_color("fill-color", &out->fill_color);
  out->fill_opacity = parse_float(layer.paint, "fill-opacity", 1.f);
  apply_color("line-color", &out->line_color);
  out->line_width = parse_float(layer.paint, "line-width", 1.f);
  out->line_opacity = parse_float(layer.paint, "line-opacity", 1.f);
  apply_color("circle-color", &out->circle_color);
  out->circle_radius = parse_float(layer.paint, "circle-radius", 5.f);
  out->circle_opacity = parse_float(layer.paint, "circle-opacity", 1.f);

  out->icon_image = map_get(layer.layout, "icon-image");
  if (out->icon_image.empty()) {
    out->icon_image = map_get(layer.paint, "icon-image");
  }
  out->text_field = map_get(layer.layout, "text-field");
  out->icon_size = parse_float(layer.layout, "icon-size", 1.f);
  if (out->icon_size <= 0.f) {
    out->icon_size = parse_float(layer.paint, "icon-size", 1.f);
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
