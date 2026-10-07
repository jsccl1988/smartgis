// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/document/document_bind.h"

#include <cstdlib>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/bind/slots.h"
#include "app/views/il.runtime/backend/view/shot/export.h"
#include "app/views/il.runtime/backend/view/pixel/bmp.h"
#include "app/views/util/charset.h"
#include "app/views/util/exe_sidecar_path.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

bool export_bmp_leaf(Browser& browser,
                     const std::string& leaf,
                     const std::string& frame) {
  if (leaf.empty()) {
    return false;
  }
  const std::wstring leaf_w = utf8_to_wide(leaf);
  if (leaf_w.empty()) {
    return false;
  }
  wchar_t bmp_w[MAX_PATH] = {};
  if (!exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
    return false;
  }
  if (frame == "scene3d") {
    return export_scene3d_bmp(&browser, bmp_w);
  }
  Map2dExportOpts opts;
  auto dim = [](const char* key, int fallback, int lo, int hi) {
    const char* raw = std::getenv(key);
    if (!raw || !raw[0]) {
      return fallback;
    }
    const int v = std::atoi(raw);
    return (v >= lo && v <= hi) ? v : fallback;
  };
  opts.width = dim("MAP2D_SHOWCASE_W", kCaptureW, 320, 3840);
  opts.height = dim("MAP2D_SHOWCASE_H", kCaptureH, 240, 2160);
  return export_map2d_bmp(&browser, bmp_w, frame, opts);
}

}  // namespace

void bind_export(Browser& browser, content::CapabilityHost* out) {
  Browser* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_export_bmp> =
                   [b](const std::string& leaf, const std::string& frame) {
                     return export_bmp_leaf(*b, leaf, frame);
                   },
           });
}

}  // namespace detail
}  // namespace app
