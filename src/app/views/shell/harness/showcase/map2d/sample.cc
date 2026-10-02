// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/sample.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/document/map_scene.h"

#include <cstdio>
#include <cstdlib>
#include <iterator>
#include <windows.h>

namespace app {
namespace detail {

void map2d_showcase_mark(const char* step) {
  write_mark(kMap2dShowcaseMarkLeaf, step, /*truncate=*/false);
}

void map2d_showcase_pixel_size(int* out_w, int* out_h) {
  int w = kMap2dShowcaseDefaultW;
  int h = kMap2dShowcaseDefaultH;
  if (const char* ew = std::getenv("SMT_MAP2D_SHOWCASE_W");
      ew && ew[0] != '\0') {
    const int n = std::atoi(ew);
    if (n >= 320 && n <= 3840) {
      w = n;
    }
  }
  if (const char* eh = std::getenv("SMT_MAP2D_SHOWCASE_H");
      eh && eh[0] != '\0') {
    const int n = std::atoi(eh);
    if (n >= 240 && n <= 2160) {
      h = n;
    }
  }
  if (out_w) {
    *out_w = w;
  }
  if (out_h) {
    *out_h = h;
  }
}

bool try_open_china_sample(Browser& browser) {
  // SeedDocument may leave a stub land rect (feature_count>=3) that is not
  // china_city — still open the sample so rivers/roads/DEM framing exist.
  if (browser.document() && browser.document()->has_china_extent() &&
      browser.document()->feature_count() >= 3) {
    return true;
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
  return false;
}

bool try_path_candidates(const wchar_t* const* rels,
                         size_t count,
                         char* out_utf8,
                         size_t out_cap) {
  wchar_t base[MAX_PATH] = {};
  if (!exe_dir_with_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
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

bool try_load_align_style(Browser& browser) {
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
  if (!try_path_candidates(candidates, std::size(candidates), path_a,
                           sizeof(path_a))) {
    return false;
  }
  if (!browser.document()->load_style_path(path_a)) {
    return false;
  }
  map2d_showcase_mark("style-align");
  return true;
}

}  // namespace detail
}  // namespace app
