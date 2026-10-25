// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/document/document_bind.h"
#include "content/browser/glang.runtime/host/bind/shell.h"

#include <cstdlib>
#include <string>

#include "base/core/log.h"
#include "content/browser/glang.runtime/host/bind/slots.h"
#include "content/browser/debug/capture/shot/export.h"
#include "content/browser/debug/capture/pixel/bmp.h"
#include "content/browser/glang.runtime/host/utils/charset.h"
#include "content/browser/glang.runtime/host/utils/exe_path.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace content {
namespace detail {
namespace {

bool export_bmp_leaf(content::Shell& browser,
                     const std::string& leaf,
                     const std::string& frame) {
  if (leaf.empty()) {
    LOGGING(LOG_ERROR, "export_bmp: empty leaf");
    return false;
  }
  const std::wstring leaf_w = utf8_to_wide(leaf);
  if (leaf_w.empty()) {
    LOGGING(LOG_ERROR, "export_bmp: leaf utf8_to_wide failed");
    return false;
  }
  wchar_t bmp_w[MAX_PATH] = {};
  if (!exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
    LOGGING(LOG_ERROR, "export_bmp: exe_capture_path failed leaf=%s",
            leaf.c_str());
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
  // Plugin / harness scripts often present rasters (flood mask, DEM) with
  // feature_count()==0; requiring vectors blocks export_bmp after a green
  // processing mark.
  opts.require_features = false;
  if (!export_map2d_bmp(&browser, bmp_w, frame, opts)) {
    LOGGING(LOG_ERROR, "export_bmp: export_map2d_bmp failed leaf=%s frame=%s",
            leaf.c_str(), frame.c_str());
    return false;
  }
  return true;
}

}  // namespace

void bind_export(content::Shell& browser, content::CapabilityHost* out) {
  content::Shell* b = &browser;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_export_bmp> =
                   [b](const std::string& leaf, const std::string& frame) {
                     return export_bmp_leaf(*b, leaf, frame);
                   },
           });
}

}  // namespace detail
}  // namespace content
