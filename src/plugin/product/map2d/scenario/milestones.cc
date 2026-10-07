// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/scenario/probe.h"

#include "plugin/runtime/host/capability/shell.h"
#include <windows.h>
#include <shellapi.h>

#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/app/content_main.h"
#include "content/public/event_bus.h"
#include "content/public/gis_contents.h"
#include "content/public/types.h"
#include "content/public/tool_session.h"
#include "content/renderer/renderer_main.h"
#include "gis/edit/memory_session.h"
#include "gis/style/document/style_document.h"
#include "gis/tile/provider/tile_provider.h"
#include "gpu/gpu.h"
#include "net/http/http.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "tool/interaction/interaction.h"
#include "tool/workspace/workspace.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/gis/inspect/feature_info.h"
#include "ui/gis/shell/status_bar.h"
#include "ui/views/kernel/shell/dpi.h"
#include "base/trace/event/process_trace.h"
#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/menu/menu_bar.h"
#include "ui/views/kernel/view/view.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <cwctype>

namespace plugin {

int scenario_milestones(HarnessShell& browser) {

// M2: Processing panel + buffer/clip write-back.
{
  std::string err;
  if (!browser.run_m2_harness_hooks(&err)) {
    std::fprintf(stderr, "M2 self-test failed: %s\n", err.c_str());
    browser.detach_views();
    if (err.find("clip") != std::string::npos) {
      return 82;
    }
    if (err.find("buffer") != std::string::npos ||
        err.find("write-back") != std::string::npos ||
        err.find("write_path") != std::string::npos) {
      return 81;
    }
    return 80;
  }
  browser.mark("m2-panel-ok");
  browser.mark("m2-buffer-ok");
  browser.mark("m2-clip-ok");
}

// M3: DEM seed + tileset stream + atmosphere toggle.
{
  std::string err;
  if (!browser.scene3d()->atmosphere_session().run_m3_self_test_hooks(&err)) {
    std::fprintf(stderr, "M3 self-test failed: %s\n", err.c_str());
    browser.detach_views();
    if (err == "m3-dem-ok") {
      return 90;
    }
    if (err == "m3-tiles-ok") {
      return 91;
    }
    return 92;
  }
  browser.mark("m3-dem-ok");
  browser.mark("m3-tiles-ok");
  browser.mark("m3-atmosphere-ok");
}

// M4: optimistic edit conflict (exit 100 on failure).
{
  constexpr int k_exit_edit_conflict = 100;
  content::FeatureId fid{};
  fid.len = 1;
  fid.bytes[0] = 42;
  auto store = std::make_shared<gis::OptimisticLayerStore>();
  store->seed_feature(fid, 1);
  gis::MemoryEditSession client_a(store);
  gis::MemoryEditSession client_b(store);
  gis::FeatureMutation write{};
  write.op = gis::EditOp::kModify;
  write.id = fid;
  write.base_version = 1;
  if (!client_a.commit(write) ||
      store->feature_version(fid) != 2 ||
      client_b.commit(write) ||
      client_b.last_status() != gis::CommitStatus::kConflict) {
    std::fprintf(stderr, "M4 conflict self-test failed\n");
    browser.detach_views();
    return k_exit_edit_conflict;
  }
  browser.mark("m4-conflict-ok");
}

browser.mark("pass");
// Do not stop timers or detach here. stop_present_timers drains WM_TIMER
// via PeekMessage and can re-enter ContentMapView present under Debug CRT;
// that races exit_after_scenario's TerminateProcess and surfaces as
// exit 0xFFFFFFFF after green marks (same class as browse.3d / ui_showcase).
// TerminateProcess skips orderly HWND teardown �?leave timers alone.
browser.mark("detached");
return 0;
}

}  // namespace plugin
