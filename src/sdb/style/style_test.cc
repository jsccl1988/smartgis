// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/style/paint_resolve.h"
#include "sdb/style/style_document.h"
#include "sdb/style/style_rules.h"
#include "sdb/style/symbol_library.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using sdb::style::AttrMap;
  using sdb::style::LayerType;
  using sdb::style::ResolvedPaint;
  using sdb::style::StyleDocument;
  using sdb::style::SymbolLibrary;
  using sdb::style::eval_filter;
  using sdb::style::parse_color;
  using sdb::style::parse_style_document;
  using sdb::style::resolve;
  using sdb::style::serialize_style_document;
  using sdb::style::to_smt_style;

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
      "\"paint\":{\"line-color\":\"#3366ff\",\"line-width\":2.5}"
      "},"
      "{"
      "\"id\":\"poi\","
      "\"type\":\"symbol\","
      "\"filter\":[\"has\",\"name\"],"
      "\"layout\":{\"icon-image\":\"marker\",\"icon-size\":1.5,"
      "\"text-field\":\"{name}\"},"
      "\"paint\":{}"
      "},"
      "{"
      "\"id\":\"water\","
      "\"type\":\"fill\","
      "\"paint\":{\"fill-color\":\"rgb(0, 128, 255)\",\"fill-opacity\":0.5}"
      "}"
      "]"
      "}";

  StyleDocument doc;
  expect(parse_style_document(kStyle, &doc), "parse_style_document");
  expect(doc.version == 8, "version");
  expect(doc.layers.size() == 3, "layer count");
  expect(doc.layers[0].type == LayerType::kLine, "line type");
  expect(doc.layers[0].has_minzoom && doc.layers[0].minzoom == 5.0, "minzoom");

  const std::string roundtrip = serialize_style_document(doc);
  StyleDocument doc2;
  expect(parse_style_document(roundtrip, &doc2), "roundtrip parse");
  expect(doc2.layers.size() == 3, "roundtrip layers");

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
