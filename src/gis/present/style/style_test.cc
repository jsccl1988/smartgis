// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "gis/present/style/expression.h"
#include "gis/present/style/paint_resolve.h"
#include "gis/present/style/style_document.h"
#include "gis/present/style/style_rules.h"
#include "gis/present/style/symbol_library.h"

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
  expect(symbol_paint.symbol_placement == "point", "symbol-placement default");
  expect(approx_eq(symbol_paint.text_halo_width, 0.f), "text-halo-width default");

  StyleDocument halo_doc;
  expect(parse_style_document(
             "{\"version\":8,\"layers\":[{\"id\":\"lab\",\"type\":\"symbol\","
             "\"layout\":{\"symbol-placement\":\"line\",\"text-field\":\"{name}\"},"
             "\"paint\":{\"text-halo-color\":\"#ffffff\",\"text-halo-width\":2}}]}",
             &halo_doc),
         "parse symbol halo");
  ResolvedPaint halo_paint;
  expect(resolve(halo_doc, nullptr, named, 10.0, "", &halo_paint),
         "resolve symbol halo");
  expect(halo_paint.symbol_placement == "line", "symbol-placement");
  expect(halo_paint.text_halo_color == 0xFFFFFFFFu, "text-halo-color");
  expect(approx_eq(halo_paint.text_halo_width, 2.f), "text-halo-width");

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

  // interpolate / step / match / case / coalesce (mini richness).
  ExprValue interp_ev;
  expect(eval_expression(
             "[\"interpolate\",[\"linear\"],[\"zoom\"],5,1,15,5]", expr_attrs,
             10.0, &interp_ev),
         "eval interpolate linear");
  expect(approx_eq(static_cast<float>(interp_ev.as_number()), 3.f),
         "interpolate mid");
  ExprValue interp_lo;
  expect(eval_expression(
             "[\"interpolate\",[\"linear\"],[\"zoom\"],5,1,15,5]", expr_attrs,
             3.0, &interp_lo),
         "eval interpolate clamp lo");
  expect(approx_eq(static_cast<float>(interp_lo.as_number()), 1.f),
         "interpolate lo");
  ExprValue interp_color;
  expect(eval_expression(
             "[\"interpolate\",[\"linear\"],[\"zoom\"],0,\"#000000\",10,"
             "\"#ffffff\"]",
             expr_attrs, 5.0, &interp_color),
         "eval interpolate color");
  expect(interp_color.as_string() == "#808080", "interpolate color mid");
  ExprValue exp_ev;
  expect(eval_expression(
             "[\"interpolate\",[\"exponential\",2],[\"zoom\"],0,0,10,10]",
             expr_attrs, 5.0, &exp_ev),
         "eval interpolate exponential");
  {
    const double t = 0.5;
    const double progress = (std::pow(2.0, t) - 1.0) / (2.0 - 1.0);
    expect(std::fabs(exp_ev.as_number() - 10.0 * progress) < 1e-4,
           "exponential mid");
  }
  ExprValue step_ev;
  expect(eval_expression("[\"step\",[\"zoom\"],1,8,2,12,4]", expr_attrs, 10.0,
                         &step_ev),
         "eval step");
  expect(approx_eq(static_cast<float>(step_ev.as_number()), 2.f), "step value");
  expr_attrs["class"] = "primary";
  ExprValue match_ev;
  expect(eval_expression(
             "[\"match\",[\"get\",\"class\"],[\"primary\",\"trunk\"],\"#ff0000\","
             "\"secondary\",\"#00ff00\",\"#000000\"]",
             expr_attrs, 10.0, &match_ev),
         "eval match label-array");
  expect(match_ev.as_string() == "#ff0000", "match hit");
  ExprValue match_def;
  expect(eval_expression(
             "[\"match\",[\"get\",\"class\"],\"secondary\",\"#00ff00\","
             "\"#111111\"]",
             expr_attrs, 10.0, &match_def),
         "eval match default");
  expect(match_def.as_string() == "#111111", "match default");
  ExprValue case_ev;
  expect(eval_expression(
             "[\"case\",[\"==\",[\"get\",\"rank\"],\"3\"],\"#aabbcc\","
             "\"#000000\"]",
             expr_attrs, 10.0, &case_ev),
         "eval case");
  expect(case_ev.as_string() == "#aabbcc", "case true");
  ExprValue coal_ev;
  expect(eval_expression(
             "[\"coalesce\",[\"get\",\"missing\"],[\"get\",\"stroke\"]]",
             expr_attrs, 10.0, &coal_ev),
         "eval coalesce");
  expect(coal_ev.as_string() == "#ff0000", "coalesce value");

  StyleDocument rich_doc;
  expect(parse_style_document(
             "{\"version\":8,\"layers\":["
             "{\"id\":\"rl\",\"type\":\"line\",\"paint\":{"
             "\"line-width\":[\"interpolate\",[\"linear\"],[\"zoom\"],5,1,15,5],"
             "\"line-color\":[\"match\",[\"get\",\"class\"],\"primary\","
             "\"#3366ff\",\"#112233\"]}},"
             "{\"id\":\"rf\",\"type\":\"fill\",\"paint\":{"
             "\"fill-opacity\":[\"step\",[\"zoom\"],0.2,10,0.6],"
             "\"fill-color\":[\"case\",[\"==\",[\"get\",\"rank\"],\"3\"],"
             "\"#00aa00\",\"#ff0000\"]}},"
             "{\"id\":\"rc\",\"type\":\"circle\",\"paint\":{"
             "\"circle-radius\":[\"interpolate\",[\"linear\"],[\"get\","
             "\"rank\"],1,4,5,12],"
             "\"circle-color\":[\"coalesce\",[\"get\",\"missing\"],"
             "\"#abcdef\"]}},"
             "{\"id\":\"rs\",\"type\":\"symbol\",\"layout\":{"
             "\"text-size\":[\"interpolate\",[\"linear\"],[\"zoom\"],8,12,16,"
             "20],\"icon-size\":[\"step\",[\"zoom\"],0.5,12,1.5]}}"
             "]}",
             &rich_doc),
         "parse rich expr style");
  ResolvedPaint rich_line;
  fill_resolved_paint(rich_doc.layers[0], nullptr, expr_attrs, 10.0, &rich_line);
  expect(approx_eq(rich_line.line_width, 3.f), "rich line-width");
  expect(rich_line.line_color == 0xFF3366FFu, "rich line-color match");
  ResolvedPaint rich_fill;
  fill_resolved_paint(rich_doc.layers[1], nullptr, expr_attrs, 12.0, &rich_fill);
  expect(approx_eq(rich_fill.fill_opacity, 0.6f), "rich fill-opacity step");
  expect(rich_fill.fill_color == 0xFF00AA00u, "rich fill-color case");
  ResolvedPaint rich_circle;
  fill_resolved_paint(rich_doc.layers[2], nullptr, expr_attrs, 10.0,
                      &rich_circle);
  expect(approx_eq(rich_circle.circle_radius, 8.f), "rich circle-radius");
  expect(rich_circle.circle_color == 0xFFABCDEFu, "rich circle coalesce");
  ResolvedPaint rich_sym;
  fill_resolved_paint(rich_doc.layers[3], nullptr, expr_attrs, 12.0, &rich_sym);
  expect(approx_eq(rich_sym.text_size, 16.f), "rich text-size interpolate");
  expect(approx_eq(rich_sym.icon_size, 1.5f), "rich icon-size step");

  base::SmtStyle smt = to_smt_style(paint, "roads_smt");
  expect(std::strcmp(smt.get_style_name(), "roads_smt") == 0, "smt name");

  ResolvedPaint zoom_miss;
  expect(!resolve(doc, &lib, attrs, 3.0, "transport", &zoom_miss),
         "zoom miss roads");

  {
    StyleDocument hs_doc;
    expect(parse_style_document(
               "{\"version\":8,\"layers\":[{\"id\":\"shade\",\"type\":"
               "\"hillshade\",\"paint\":{"
               "\"hillshade-illumination-direction\":210,"
               "\"hillshade-exaggeration\":0.75,"
               "\"hillshade-shadow-color\":\"#112233\","
               "\"hillshade-highlight-color\":\"#eeddcc\""
               "}}]}",
               &hs_doc),
           "parse hillshade style");
    ResolvedPaint hs;
    fill_resolved_paint(hs_doc.layers[0], nullptr, {}, 10.0, &hs);
    expect(hs.type == LayerType::kHillshade, "hillshade type");
    expect(approx_eq(hs.hillshade_illumination_direction, 210.f),
           "hillshade direction");
    expect(approx_eq(hs.hillshade_exaggeration, 0.75f), "hillshade exag");
    expect(hs.hillshade_shadow_color == 0xFF112233u, "hillshade shadow");
    expect(hs.hillshade_highlight_color == 0xFFEEDDCCu, "hillshade highlight");
  }

  {
    StyleDocument ex_doc;
    expect(parse_style_document(
               "{\"version\":8,\"layers\":[{\"id\":\"bldg\",\"type\":"
               "\"fill-extrusion\",\"paint\":{"
               "\"fill-extrusion-height\":42,"
               "\"fill-extrusion-base\":2,"
               "\"fill-extrusion-color\":\"#8899aa\","
               "\"fill-extrusion-opacity\":0.8"
               "}}]}",
               &ex_doc),
           "parse fill-extrusion style");
    ResolvedPaint ex;
    fill_resolved_paint(ex_doc.layers[0], nullptr, {}, 14.0, &ex);
    expect(ex.type == LayerType::kFillExtrusion, "fill-extrusion type");
    expect(approx_eq(ex.fill_extrusion_height, 42.f), "extrusion height");
    expect(approx_eq(ex.fill_extrusion_base, 2.f), "extrusion base");
    expect(ex.fill_extrusion_color == 0xFF8899AAu, "extrusion color");
    expect(approx_eq(ex.fill_extrusion_opacity, 0.8f), "extrusion opacity");
  }

  {
    StyleDocument heat_doc;
    expect(parse_style_document(
               "{\"version\":8,\"layers\":[{\"id\":\"heat\",\"type\":"
               "\"heatmap\",\"paint\":{"
               "\"heatmap-radius\":18,"
               "\"heatmap-weight\":0.5,"
               "\"heatmap-intensity\":1.25,"
               "\"heatmap-color\":\"#3366ff\","
               "\"heatmap-opacity\":0.7"
               "}}]}",
               &heat_doc),
           "parse heatmap style");
    ResolvedPaint heat;
    fill_resolved_paint(heat_doc.layers[0], nullptr, {}, 12.0, &heat);
    expect(heat.type == LayerType::kHeatmap, "heatmap type");
    expect(approx_eq(heat.heatmap_radius, 18.f), "heatmap radius");
    expect(approx_eq(heat.heatmap_weight, 0.5f), "heatmap weight");
    expect(approx_eq(heat.heatmap_intensity, 1.25f), "heatmap intensity");
    expect(heat.heatmap_color == 0xFF3366FFu, "heatmap color");
    expect(approx_eq(heat.heatmap_opacity, 0.7f), "heatmap opacity");
  }

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("style_test OK\n");
  return 0;
}
