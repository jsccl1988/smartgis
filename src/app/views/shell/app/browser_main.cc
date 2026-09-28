// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/browser_main.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <memory>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/self_test/self_test.h"
#include "app/views/shell/showcase/atmosphere_showcase.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "ui/views/kernel/shell/dpi.h"

namespace app {

int run_browser_main(const content::ContentMainParams&,
                     const ViewsLaunchOptions& options) {
  ui::views::enable_process_dpi_awareness();
  const AtmosphereShowcaseMode showcase = options.atmosphere_showcase;
  // Default Scene3d prefers FlyCube; DX12 multi-viewport attach can hang on
  // some hosts. Opt out with SMT_FORCE_CONTENT_MAPVIEW_3D=1 (or legacy
  // SMT_PREFER_FLYCUBE_3D=0) before Browser::init. Showcase acquires
  // FlyCube after the shell is up (see run_atmosphere_showcase).
  // --self-test forces ContentMapView so shell smoke stays hang-free; product
  // interactive runs keep the FlyCube default.
  const bool self_test = options.self_test;
  // Showcase also forces ContentMapView: multi-viewport FlyCube attach during
  // Browser::init can hang; run_atmosphere_showcase opens its own present
  // HWND after the shell is up.
  if (self_test || showcase != AtmosphereShowcaseMode::kNone) {
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_3D", "1");
  }
  {
    wchar_t diag[MAX_PATH] = {};
    if (detail::exe_sidecar_path(diag, MAX_PATH,
                                 L"atmosphere-showcase-cmdline.txt")) {
      FILE* f = nullptr;
      if (_wfopen_s(&f, diag, L"w") == 0 && f) {
        const wchar_t* cmd = GetCommandLineW();
        std::fwprintf(f, L"cmd=%s\nshowcase=%hs\n", cmd ? cmd : L"(null)",
                      atmosphere_showcase_name(showcase));
        std::fclose(f);
      }
    }
  }
  // Browser embeds MapScene + Scene3dPresenter (World/GpuScene/atmosphere
  // passes). Stack allocation overflows the default 1 MiB thread stack and
  // corrupts Scene3dPresenter vectors (RTC/AV in bind_map / teardown).
  auto browser = std::make_unique<Browser>();
  if (!browser->init()) {
    return 1;
  }
  if (!options.atmosphere_fields.empty()) {
    std::fprintf(stderr, "atmosphere-fields: %s\n",
                 options.atmosphere_fields.c_str());
    if (!browser->apply_atmosphere_fields(options.atmosphere_fields)) {
      std::fprintf(stderr, "atmosphere-fields: load failed\n");
    }
  }
  browser->show();
  if (showcase != AtmosphereShowcaseMode::kNone) {
    wchar_t mark_path[MAX_PATH] = {};
    if (detail::exe_sidecar_path(mark_path, MAX_PATH,
                                 L"atmosphere-showcase-mark.txt")) {
      DeleteFileW(mark_path);
    }
    pump_views_messages(400);
    if (!browser->hwnd() || !IsWindow(browser->hwnd())) {
      return 2;
    }
    return run_atmosphere_showcase(*browser, showcase);
  }
  if (self_test) {
    return run_views_self_test(*browser);
  }
  return browser->run_loop();
}

}  // namespace app
