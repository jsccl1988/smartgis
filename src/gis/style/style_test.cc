// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "gis/style/expression.h"
#include "gis/style/paint_resolve.h"
#include "gis/style/style_document.h"
#include "gis/style/style_rules.h"
#include "gis/style/symbol_library.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool approx_eq(float a, float b) { return std::fabs(a - b) < 1e-4f; }

}  // namespace

int main() {
  using gis::style::AttrMap;
  using gis::style::eval_expression;
  using gis::style::eval_filter;
  using gis::style::ExprValue;
  using gis::style::layer_type_from_string;
  using gis::style::LayerType;
  using gis::style::parse_color;
  using gis::style::parse_style_document;
  using gis::style::resolve;
  using gis::style::ResolvedPaint;
  using gis::style::serialize_style_document;
  using gis::style::StyleDocument;
  using gis::style::SymbolLibrary;
  using gis::style::to_smt_style;

  // Avoid raw-string literals (legacy /FI headers may interfere on MSVC).
  const std::string kStyle =
      "{"
      "\"version\":8,"
      "\"name\":\"demo\","
      "\"sprite\":\"icons\","
      "\"layers\":["
      "{"
      "\"id\":\"roads\","
      "\"type\":\"line\","
      "\"source-layer\":\"transport\","
      "\"minzoom\":5,"
      "\"maxzoom\":22,"
      "\"filter\":[\"==\",\"class\",\"primary\"],"
      "\"paint\":{"
      "\"line-color\":\"#3366ff\","
      "\"line-width\":2.5,"
      "\"line-dasharray\":[2,4],"
      "\"line-cap\":\"round\","
      "\"line-join\":\"miter\""
      "}"
      "},"
      "{"
      "\"id\":\"poi\","
      "\"type\":\"symbol\","
      "\"filter\":[\"has\",\"name\"],"
      "\"layout\":{"
      "\"icon-image\":\"marker\","
      "\"icon-size\":1.5,"
      "\"icon-offset\":[1,-2],"
      "\"text-field\":\"{name}\","
      "\"text-size\":14,"
      "\"text-anchor\":\"top\""
      "},"
      "\"paint\":{}"
      "},"
      "{"
      "\"id\":\"water\","
      "\"type\":\"fill\","
      "\"paint\":{"
      "\"fill-color\":\"rgb(0, 128, 255)\","
      "\"fill-opacity\":0.5,"
      "\"fill-pattern\":\"water-pat\""
      "}"
      "},"
      "{"
      "\"id\":\"bg\","
      "\"type\":\"background\","
      "\"paint\":{"
      "\"background-color\":\"#112233\","
      "\"background-opacity\":0.8"
      "}"
      "},"
      "{"
      "\"id\":\"sat\","
      "\"type\":\"raster\","
      "\"paint\":{\"raster-opacity\":0.6}"
      "},"
      "{"
      "\"id\":\"expr-line\","
      "\"type\":\"line\","
      "\"paint\":{"
      "\"line-color\":[\"get\",\"stroke\"],"
      "\"line-width\":[\"literal\",3.5]"
      "}"
      "},"
      "{"
      "\"id\":\"extrude\","
      "\"type\":\"fill-extrusion\","
      "\"paint\":{}"
      "},"
      "{"
      "\"id\":\"heat\","
      "\"type\":\"heatmap\","
      "\"paint\":{}"
      "},"
      "{"
      "\"id\":\"shade\","
      "\"type\":\"hillshade\","
      "\"paint\":{}"
      "}"
      "]"
      "}";

  StyleDocument doc;
  expect(parse_style_document(kStyle, &doc), "parse_style_document");
  expect(doc.version == 8, "version");
  expect(doc.layers.size() == 9, "layer count");
  expect(doc.layers[0].type == LayerType::kLine, "line type");
  expect(doc.layers[0].has_minzoom && doc.layers[0].minzoom == 5.0, "minzoom");
  expect(doc.layers[6].type == LayerType::kFillExtrusion, "fill-extrusion");
  expect(doc.layers[7].type == LayerType::kHeatmap, "heatmap");
  expect(doc.layers[8].type == LayerType::kHillshade, "hillshade");
  expect(layer_type_from_string("unknown-type") == LayerType::kUnknown,
         "unknown type");

  const std::string roundtrip = serialize_style_document(doc);
  StyleDocument doc2;
  expect(parse_style_document(roundtrip, &doc2), "roundtrip parse");
  expect(doc2.layers.size() == 9, "roundtrip layers");

  AttrMap attrs;
  attrs["class"] = "primary";
  attrs["name"] = "A1";
  expect(eval_filter(doc.layers[0].filter, attrs), "filter ==");
  AttrMap other;
  other["class"] = "secondary";
  expect(!eval_filter(doc.layers[0].filter, other), "filter miss");

  SymbolLibrary lib;
  lib.set_root("C:/symbols");
  const std::string manifest =
      "{\"symbols\":[{\"id\":\"marker\",\"path\":\"marker.png\"}]}";
  expect(lib.load_manifest(manifest), "load_manifest");
  expect(lib.size() == 1, "symbol size");

  ResolvedPaint paint;
  expect(resolve(doc, &lib, attrs, 10.0, "transport", &paint), "resolve road");
  expect(paint.layer_id == "roads", "resolved id");
  expect(paint.type == LayerType::kLine, "resolved type");
  expect(paint.line_dasharray.size() == 2, "dasharray size");
  expect(approx_eq(paint.line_dasharray[0], 2.f) &&
             approx_eq(paint.line_dasharray[1], 4.f),
         "dasharray values");
  expect(paint.line_cap == "round", "line-cap");
  expect(paint.line_join == "miter", "line-join");

  uint32_t argb = 0;
  expect(parse_color("#3366ff", &argb), "parse_color");
  expect(argb == 0xFF3366FFu, "color value");

  ResolvedPaint poi;
  expect(resolve(doc, &lib, attrs, 12.0, "", &poi), "resolve first match");
  expect(poi.layer_id == "roads", "first match is roads");

  AttrMap named;
  named["name"] = "Park";
  ResolvedPaint symbol_paint;
  expect(resolve(doc, &lib, named, 12.0, "", &symbol_paint), "resolve symbol");
  expect(symbol_paint.layer_id == "poi", "poi layer");
  expect(symbol_paint.has_symbol, "symbol resolved");
  expect(symbol_paint.symbol.path.find("marker.png") != std::string::npos,
         "symbol path");
  expect(approx_eq(symbol_paint.text_size, 14.f), "text-size");
  expect(symbol_paint.text_anchor == "top", "text-anchor");
  expect(approx_eq(symbol_paint.icon_offset_x, 1.f) &&
             approx_eq(symbol_paint.icon_offset_y, -2.f),
         "icon-offset");

  AttrMap empty;
  ResolvedPaint fill_paint;
  expect(resolve(doc, &lib, empty, 12.0, "", &fill_paint), "resolve fill");
  // roads/poi filters miss → water is first match.
  expect(fill_paint.layer_id == "water", "water layer");
  expect(fill_paint.fill_pattern == "water-pat", "fill-pattern");

  StyleDocument bg_only;
  expect(parse_style_document(
             "{\"version\":8,\"layers\":[{\"id\":\"bg\",\"type\":"
             "\"background\",\"paint\":{\"background-color\":\"#112233\","
             "\"background-opacity\":0.8}}]}",
             &bg_only),
         "parse bg-only");
  ResolvedPaint bg_paint;
  expect(resolve(bg_only, nullptr, empty, 1.0, "", &bg_paint), "resolve bg");
  expect(bg_paint.background_color == 0xFF112233u, "background-color");
  expect(approx_eq(bg_paint.background_opacity, 0.8f), "background-opacity");

  StyleDocument raster_only;
  expect(parse_style_document(
             "{\"version\":8,\"layers\":[{\"id\":\"sat\",\"type\":\"raster\","
             "\"paint\":{\"raster-opacity\":0.6}}]}",
             &raster_only),
         "parse raster-only");
  ResolvedPaint raster_paint;
  expect(resolve(raster_only, nullptr, empty, 1.0, "", &raster_paint),
         "resolve raster");
  expect(approx_eq(raster_paint.raster_opacity, 0.6f), "raster-opacity");

  // Expression subset: get / literal / binary compare.
  AttrMap expr_attrs;
  expr_attrs["stroke"] = "#ff0000";
  expr_attrs["rank"] = "3";
  ExprValue get_ev;
  expect(eval_expression("[\"get\",\"stroke\"]", expr_attrs, 10.0, &get_ev),
         "eval get");
  expect(get_ev.as_string() == "#ff0000", "get value");
  ExprValue lit_ev;
  expect(eval_expression("[\"literal\",3.5]", expr_attrs, 10.0, &lit_ev),
         "eval literal");
  expect(approx_eq(static_cast<float>(lit_ev.as_number()), 3.5f),
         "literal value");
  ExprValue cmp_ev;
  expect(eval_expression("[\"==\",[\"get\",\"rank\"],[\"literal\",\"3\"]]",
                         expr_attrs, 10.0, &cmp_ev),
         "eval ==");
  expect(cmp_ev.kind == ExprValue::Kind::kBool && cmp_ev.b, "cmp true");

  StyleDocument expr_doc;
  expect(parse_style_document(
             "{\"version\":8,\"layers\":[{\"id\":\"expr-line\",\"type\":"
             "\"line\",\"paint\":{\"line-color\":[\"get\",\"stroke\"],"
             "\"line-width\":[\"literal\",3.5]}}]}",
             &expr_doc),
         "parse expr style");
  ResolvedPaint expr_paint;
  expect(resolve(expr_doc, nullptr, expr_attrs, 12.0, "", &expr_paint),
         "resolve expr paint");
  expect(expr_paint.line_color == 0xFFFF0000u, "expr line-color");
  expect(approx_eq(expr_paint.line_width, 3.5f), "expr line-width");

  base::SmtStyle smt = to_smt_style(paint, "roads_smt");
  expect(std::strcmp(smt.get_style_name(), "roads_smt") == 0, "smt name");

  ResolvedPaint zoom_miss;
  expect(!resolve(doc, &lib, attrs, 3.0, "transport", &zoom_miss),
         "zoom miss roads");

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("style_test OK\n");
  return 0;
}
