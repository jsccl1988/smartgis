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
#include "base/core/log.h"
#include "base/trace/process_trace.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
#include "ui/views/kernel/shell/dpi.h"

namespace app {

int run_browser_main(const content::ContentMainParams&,
                     const ViewsLaunchOptions& options) {
  BASE_TRACE_EVENT("BrowserMain", "startup");
  LOGGING(LOG_INFO, "startup: BrowserMain begin");
  {
    BASE_TRACE_EVENT("DpiAwareness", "startup");
    ui::views::enable_process_dpi_awareness();
  }
  const AtmosphereShowcaseMode showcase = options.atmosphere_showcase;
  // Default Scene3d prefers FlyCube RHI. Switch via View → Engine or
  // content::set_scene3d_engine (not env vars). --self-test / console /
  // atmosphere-showcase select GDI before Browser::init so multi-viewport
  // FlyCube attach does not hang; showcase may acquire FlyCube on its own HWND.
  const bool self_test = options.self_test;
  const bool self_test_console = options.self_test_console;
  if (self_test || self_test_console ||
      showcase != AtmosphereShowcaseMode::kNone) {
    content::set_scene3d_engine(content::Scene3dEngine::kGdi);
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_2D", "1");
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
  {
    BASE_TRACE_EVENT("Browser.init", "startup");
    LOGGING(LOG_INFO, "startup: Browser::init");
    if (!browser->init()) {
      LOGGING(LOG_ERROR, "startup: Browser::init failed");
      return 1;
    }
  }
  if (options.debug_console || self_test_console ||
      content::debug_console_env_enabled()) {
    BASE_TRACE_EVENT("DebugAgent.start", "startup");
    LOGGING(LOG_INFO, "startup: DebugAgent start");
    content::debug_agent().start();
  }
  if (!options.atmosphere_fields.empty()) {
    BASE_TRACE_EVENT("AtmosphereFields", "startup");
    std::fprintf(stderr, "atmosphere-fields: %s\n",
                 options.atmosphere_fields.c_str());
    if (!browser->apply_atmosphere_fields(options.atmosphere_fields)) {
      std::fprintf(stderr, "atmosphere-fields: load failed\n");
      LOGGING(LOG_WARNING, "startup: atmosphere-fields load failed");
    }
  }
  {
    BASE_TRACE_EVENT("Browser.show", "startup");
    LOGGING(LOG_INFO, "startup: Browser::show");
    browser->show();
  }
  LOGGING(LOG_INFO, "startup: first show complete");
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
  // Mutual exclusion: if both --self-test and --self-test-console are set,
  // prefer the shorter console path (host wiring + :cmd checks + bench JSON)
  // instead of the full shell self-test.
  if (self_test_console) {
    return run_views_console_self_test(*browser);
  }
  if (self_test) {
    return run_views_self_test(*browser);
  }
  return browser->run_loop();
}

}  // namespace app
