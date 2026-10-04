// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/common/io/sample.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/util/exe_sidecar_path.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

bool try_open_china_sample(Browser& browser,
                           bool write_stub_if_missing) {
  (void)write_stub_if_missing;
  // Only skip open when a china_city-scale OGR pack is already loaded.
  if (browser.document() && browser.document()->last_open_was_ogr() &&
      browser.document()->feature_count() >= 200) {
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
  return false;
}

}  // namespace detail
}  // namespace app
