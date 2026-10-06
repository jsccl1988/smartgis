// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/sample.h"

#include "plugin/product/map2d/scenario/progress.h"
#include "plugin/runtime/host/capability/shell.h"
#include "content/browser/document/map_scene.h"

#include <cstdio>
#include <iterator>
#include <windows.h>

namespace plugin {
namespace detail {

bool try_open_china_sample(HarnessShell& browser) {
  if (browser.document() && browser.document()->has_china_extent() &&
      browser.document()->feature_count() >= 200) {
    return true;
  }
  wchar_t sample_w[MAX_PATH] = {};
  if (!browser.exe_dir_slash(sample_w, MAX_PATH)) {
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
  return false;
}

bool try_path_candidates(HarnessShell& browser, const wchar_t* const* rels,
                         size_t count, char* out_utf8, size_t out_cap) {
  if (!rels || !out_utf8 || out_cap < 8) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!browser.exe_dir_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!rels[i]) {
      continue;
    }
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, full, -1, out_utf8,
                            static_cast<int>(out_cap), nullptr, nullptr) <= 0) {
      continue;
    }
    return true;
  }
  return false;
}

bool try_load_align_style(HarnessShell& browser) {
  if (!browser.document()) {
    return false;
  }
  char path_a[MAX_PATH * 3] = {};
  const wchar_t* candidates[] = {
      L"maplibre\\example\\style_align.json",
      L"..\\maplibre\\example\\style_align.json",
      L"..\\..\\third_party\\maplibre\\example\\style_align.json",
      L"third_party\\maplibre\\example\\style_align.json",
  };
  if (!try_path_candidates(browser, candidates, std::size(candidates), path_a,
                           sizeof(path_a))) {
    return false;
  }
  if (!browser.document()->load_style_path(path_a)) {
    return false;
  }
  map2d_mark("style-align");
  return true;
}

}  // namespace detail
}  // namespace plugin
