// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scenario/product/world_preview.h"

#include <cstdio>
#include <string>

#include "plugin/runtime/host/capability/shell.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/runtime/host/capability/marks.h"
#include "plugin/runtime/host/capability/shell.h"
#include "plugin/product/world3d/scenario/common/plugin_io.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace plugin {
namespace detail {

int run_world_preview(HarnessShell& browser) {
  std::fprintf(stderr, "plugin-showcase: world_preview path\n");
  browser.mark_named(plugin::kMarkPlugin, "world_preview", /*truncate=*/true);

  content::PluginHost* host =
      browser.plugin_host();
  if (!host) {
    browser.mark_named(plugin::kMarkPlugin, "no-host", false);
    browser.detach_maps();
    return 40;
  }

  host->set_present_surface(1);
  plugin_mark("surface-preview");

  if (plugin::Scene3dSink* sink = plugin::scene3d_sink(host)) {
    if (sink->earth_bridges_installed()) {
      (void)sink->open_earth();
      plugin_mark("open-earth");
    }
  }

  if (!host->present_dataset("smartgis.world3d", "", 1, 1)) {
    browser.mark_named(plugin::kMarkPlugin, "present-fail", false);
    browser.detach_maps();
    return 41;
  }
  plugin_mark("present-ok");

  if (!browser.preview_is_open()) {
    browser.mark_named(plugin::kMarkPlugin, "preview-closed", false);
    browser.detach_maps();
    return 42;
  }
  plugin_mark("preview-open");

  browser.pump(800);

  wchar_t bmp_w[MAX_PATH] = {};
  if (!browser.capture_path(bmp_w, MAX_PATH, L"plugin-showcase-world-preview.bmp")) {
    browser.mark_named(plugin::kMarkPlugin, "bmp-path-fail", false);
    browser.preview_close();
    browser.detach_maps();
    return 43;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    browser.mark_named(plugin::kMarkPlugin, "bmp-utf8-fail", false);
    browser.preview_close();
    browser.detach_maps();
    return 44;
  }

  const bool exported = browser.preview_export_bmp(bmp_a);
  plugin_mark(exported ? "export-ok" : "export-soft");

  browser.preview_close();
  browser.detach_maps();
  browser.mark_named(plugin::kMarkPlugin, "pass", false);
  std::fprintf(stderr,
               "plugin-showcase: PASS mode=world_preview (WorldPreviewView)\n");
  return 0;
}

}  // namespace detail
}  // namespace plugin
