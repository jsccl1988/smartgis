// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/plugin/product/world_preview.h"

#include <cstdio>
#include <string>

#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/harness/common/io/maps.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/common/pump/pump.h"
#include "app/views/harness/showcase/plugin/common/plugin_io.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace app {
namespace detail {

int run_world_preview(Browser& browser) {
  std::fprintf(stderr, "plugin-showcase: world_preview path\n");
  write_mark(kPluginShowcaseMarkLeaf, "world_preview", /*truncate=*/true);

  content::PluginHost* host =
      browser.plugins() ? browser.plugins()->host() : nullptr;
  if (!host) {
    write_mark(kPluginShowcaseMarkLeaf, "no-host", false);
    detach_maps(browser);
    return 40;
  }

  host->set_present_surface(1);
  plugin_showcase_mark("surface-preview");

  if (plugin::Scene3dSink* sink = plugin::scene3d_sink(host)) {
    if (sink->earth_bridges_installed()) {
      (void)sink->open_earth();
      plugin_showcase_mark("open-earth");
    }
  }

  if (!host->present_dataset("smartgis.world3d", "", 1, 1)) {
    write_mark(kPluginShowcaseMarkLeaf, "present-fail", false);
    detach_maps(browser);
    return 41;
  }
  plugin_showcase_mark("present-ok");

  if (!browser.plugin_preview().is_open()) {
    write_mark(kPluginShowcaseMarkLeaf, "preview-closed", false);
    detach_maps(browser);
    return 42;
  }
  plugin_showcase_mark("preview-open");

  pump_messages(800);

  wchar_t bmp_w[MAX_PATH] = {};
  if (!exe_capture_path(bmp_w, MAX_PATH, L"plugin-showcase-world-preview.bmp")) {
    write_mark(kPluginShowcaseMarkLeaf, "bmp-path-fail", false);
    browser.plugin_preview().close();
    detach_maps(browser);
    return 43;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_UTF8, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    write_mark(kPluginShowcaseMarkLeaf, "bmp-utf8-fail", false);
    browser.plugin_preview().close();
    detach_maps(browser);
    return 44;
  }

  const bool exported = browser.plugin_preview().export_bmp(bmp_a);
  plugin_showcase_mark(exported ? "export-ok" : "export-soft");

  browser.plugin_preview().close();
  detach_maps(browser);
  write_mark(kPluginShowcaseMarkLeaf, "pass", false);
  std::fprintf(stderr,
               "plugin-showcase: PASS mode=world_preview (WorldPreviewView)\n");
  return 0;
}

}  // namespace detail
}  // namespace app
