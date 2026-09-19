// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/datasource/gdal/ogr_text_encoding.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// Naive CP_UTF8 (the old MapScene / draw_anno_text path). Documents why
// GBK labels become scattered crumbs: conversion "succeeds" with garbage.
std::wstring utf8_loose(const char* bytes) {
  if (!bytes || !bytes[0]) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, bytes, -1, nullptr, 0);
  if (n <= 1) {
    return {};
  }
  std::wstring out(static_cast<size_t>(n - 1), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, bytes, -1, out.data(), n);
  return out;
}

}  // namespace

int main() {
  using gis::datasource::ogr_bytes_to_utf8;
  using gis::datasource::ogr_bytes_to_wide;

  expect(ogr_bytes_to_wide(static_cast<const char*>(nullptr)).empty(),
         "null → empty");
  expect(ogr_bytes_to_wide("").empty(), "empty → empty");
  expect(ogr_bytes_to_wide("Beijing") == L"Beijing", "ascii");

  // UTF-8 北京 / 罗超 (GeoJSON / GPKG).
  const char kUtf8Beijing[] = "\xE5\x8C\x97\xE4\xBA\xAC";
  const char kUtf8Luochao[] = "\xE7\xBD\x97\xE8\xB6\x85";
  expect(ogr_bytes_to_wide(kUtf8Beijing) == L"北京", "utf8 北京");
  expect(ogr_bytes_to_wide(kUtf8Luochao) == L"罗超", "utf8 罗超");
  expect(ogr_bytes_to_utf8(kUtf8Beijing) == kUtf8Beijing, "utf8 stays utf8");

  // GBK 北京 = B1 B1 BE A9. Loose CP_UTF8 must not be treated as success.
  const char kGbkBeijing[] = "\xB1\xB1\xBE\xA9";
  const std::wstring loose = utf8_loose(kGbkBeijing);
  expect(loose != L"北京", "loose CP_UTF8 must not yield 北京 from GBK");
  expect(ogr_bytes_to_wide(kGbkBeijing) == L"北京", "GBK 北京");
  expect(ogr_bytes_to_utf8(kGbkBeijing) == kUtf8Beijing, "GBK → utf8 北京");

  // GBK 罗超 = C2 DE B3 AC (the screenshot-like fragment source).
  const char kGbkLuochao[] = "\xC2\xDE\xB3\xAC";
  expect(utf8_loose(kGbkLuochao) != L"罗超",
         "loose CP_UTF8 must not yield 罗超");
  expect(ogr_bytes_to_wide(kGbkLuochao) == L"罗超", "GBK 罗超");

  if (g_fails) {
    std::fprintf(stderr, "%d FAIL\n", g_fails);
    return 1;
  }
  std::printf("ogr_text_encoding_test ok\n");
  return 0;
}
