// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/print/composer/print_composer.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool file_is_bmp(const std::string& path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
    return false;
  }
  char magic[2] = {};
  const size_t n = std::fread(magic, 1, 2, f);
  std::fclose(f);
  return n == 2 && magic[0] == 'B' && magic[1] == 'M';
}

}  // namespace

int main() {
  plugin::PrintComposerInput in;
  in.page_width_px = 640;
  in.page_height_px = 480;
  in.map_units_per_px = 10.0;
  in.scale_label = "1:10000";
  in.legend = {{"Roads", 0xffccaa44}, {"Water", 0xff4488cc}};

  std::vector<uint8_t> bgra;
  int w = 0;
  int h = 0;
  int stride = 0;
  expect(plugin::PrintComposer::compose(in, &bgra, &w, &h, &stride),
         "compose page");
  expect(w == 640 && h == 480, "page size");
  expect(!bgra.empty() && stride > 0, "page buffer");

  char tmp[MAX_PATH] = {};
  GetTempPathA(MAX_PATH, tmp);
  const std::string path = std::string(tmp) + "smartgis_print_composer_p0.bmp";
  DeleteFileA(path.c_str());
  expect(plugin::PrintComposer::export_page_bmp(in, path), "export_page_bmp");
  expect(file_is_bmp(path), "bmp magic");
  DeleteFileA(path.c_str());

  if (g_fails) {
    std::fprintf(stderr, "%d checks failed\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "print_composer_test ok\n");
  return 0;
}
