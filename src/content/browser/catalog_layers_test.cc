// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/catalog_layers.h"

#include <cstdio>
#include <string>
#include <vector>

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
  expect(content::layers_to_catalog_json({}) == "[]", "empty array");

  content::LayerDesc a;
  a.id = "layer.a";
  a.name = "Area";
  a.visible = true;
  a.active = true;

  content::LayerDesc b;
  b.id = "layer/b\"x";
  b.name = "Line\nB";
  b.visible = false;

  const std::string json =
      content::layers_to_catalog_json({a, b});
  expect(json ==
             "[{\"id\":\"layer.a\",\"name\":\"Area\",\"visible\":true},"
             "{\"id\":\"layer/b\\\"x\",\"name\":\"Line\\nB\",\"visible\":false}]",
         "two layers with escape");

  // active must not appear on the CEF-compatible wire.
  expect(json.find("active") == std::string::npos, "no active field");

  expect(content::json_escape_string("a\"b\\c") == "a\\\"b\\\\c",
         "json_escape_string");

  if (g_fails) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("content_catalog_layers_test: ok\n");
  return 0;
}
