// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/sample.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/util/exe_sidecar_path.h"

#include <cstdio>
#include <cstring>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

bool try_open_china_sample(Browser& browser,
                                   bool write_stub_if_missing) {
  if (browser.document() && browser.document()->layer_count() > 0 &&
      browser.document()->feature_count() >= 3) {
    return true;
  }
  if (!browser.document()) {
    return false;
  }
  wchar_t sample_w[MAX_PATH] = {};
  if (!exe_dir_with_slash(sample_w, MAX_PATH)) {
    return false;
  }
  char sample_a[MAX_PATH] = {};
  const wchar_t* candidates[] = {L"..\\data\\china_city.gpkg",
                                 L"..\\data\\china_city.geojson",
                                 L"..\\data\\china_plp.geojson",
                                 L"data\\china_city.gpkg",
                                 L"data\\china_city.geojson",
                                 L"data\\china_plp.geojson",
                                 L"china_city.gpkg",
                                 L"china_city.geojson",
                                 L"china_plp.geojson"};
  for (const wchar_t* name : candidates) {
    wchar_t china_w[MAX_PATH] = {};
    if (wcscpy_s(china_w, sample_w) != 0 || wcscat_s(china_w, name) != 0) {
      continue;
    }
    if (GetFileAttributesW(china_w) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    WideCharToMultiByte(CP_UTF8, 0, china_w, -1, sample_a, MAX_PATH, nullptr,
                        nullptr);
    if (browser.document()->open_path(sample_a) &&
        browser.document()->last_open_was_ogr() &&
        browser.document()->feature_count() >= 3) {
      return true;
    }
  }
  if (!write_stub_if_missing) {
    return false;
  }
  // Stub under shared out/data/ (exe is out/Debug|Release).
  wchar_t data_dir[MAX_PATH] = {};
  if (wcscpy_s(data_dir, sample_w) == 0 &&
      wcscat_s(data_dir, L"..\\data") == 0) {
    CreateDirectoryW(data_dir, nullptr);
  }
  wchar_t stub_w[MAX_PATH] = {};
  if (wcscpy_s(stub_w, sample_w) != 0 ||
      wcscat_s(stub_w, L"..\\data\\views_ogr_selftest.geojson") != 0) {
    return false;
  }
  FILE* sf = nullptr;
  if (_wfopen_s(&sf, stub_w, L"wb") != 0 || !sf) {
    return false;
  }
  static const char kGeojson[] =
      "{\"type\":\"FeatureCollection\",\"name\":\"china_plp\","
      "\"features\":["
      "{\"type\":\"Feature\",\"properties\":{\"name\":\"Beijing\","
      "\"kind\":\"point\"},"
      "\"geometry\":{\"type\":\"Point\",\"coordinates\":"
      "[116.3974,39.9093]}},"
      "{\"type\":\"Feature\",\"properties\":{\"name\":\"Jingjin\","
      "\"kind\":\"line\"},"
      "\"geometry\":{\"type\":\"LineString\",\"coordinates\":"
      "[[116.3974,39.9093],[116.7,39.7],[117.2,39.12]]}},"
      "{\"type\":\"Feature\",\"properties\":{\"name\":\"Huabei\","
      "\"kind\":\"area\"},"
      "\"geometry\":{\"type\":\"Polygon\",\"coordinates\":"
      "[[[116.2,39.7],[116.8,39.7],[116.8,40.1],[116.2,40.1],"
      "[116.2,39.7]]]}}"
      "]}";
  std::fwrite(kGeojson, 1, sizeof(kGeojson) - 1, sf);
  std::fclose(sf);
  WideCharToMultiByte(CP_UTF8, 0, stub_w, -1, sample_a, MAX_PATH, nullptr,
                      nullptr);
  return browser.document()->open_path(sample_a) &&
         browser.document()->last_open_was_ogr() &&
         browser.document()->feature_count() >= 3;
}

}  // namespace detail
}  // namespace app
