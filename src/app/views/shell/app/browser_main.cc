// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/browser_main.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/scenario_registry.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"
#include "ui/views/kernel/shell/dpi.h"

namespace app {
namespace {

const char* atmosphere_suite_id(AtmosphereShowcaseMode mode) {
  switch (mode) {
    case AtmosphereShowcaseMode::kLand:
      return "atmosphere.land";
    case AtmosphereShowcaseMode::kOcean:
      return "atmosphere.ocean";
    case AtmosphereShowcaseMode::kFull:
      return "atmosphere.full";
    case AtmosphereShowcaseMode::kCoast:
      return "atmosphere.coast";
    case AtmosphereShowcaseMode::kNone:
      break;
  }
  return nullptr;
}

const char* map2d_suite_id(Map2dShowcaseMode mode) {
  switch (mode) {
    case Map2dShowcaseMode::kChina:
      return "map2d.china";
    case Map2dShowcaseMode::kAlign:
      return "map2d.align";
    case Map2dShowcaseMode::kOrthogrid:
      return "map2d.orthogrid";
    case Map2dShowcaseMode::kNone:
      break;
  }
  return nullptr;
}

const char* ui_suite_id(UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kShell:
      return "ui.shell";
    case UiShowcaseMode::kData:
      return "ui.data";
    case UiShowcaseMode::kScene:
      return "ui.scene";
    case UiShowcaseMode::kCatalog:
      return "ui.catalog";
    case UiShowcaseMode::kInteract:
      return "ui.interact";
    case UiShowcaseMode::kNone:
      break;
  }
  return nullptr;
}

// Runs a ScenarioRegistry entry then tears down chrome (ExitProcess).
[[noreturn]] void exit_after_scenario(std::unique_ptr<Browser>& browser,
                                      const char* suite_id) {
  const Scenario* scenario = find_scenario(suite_id);
  if (scenario && scenario->mark_leaf) {
    detail::clear_mark(scenario->mark_leaf);
  }
  pump_views_messages(400);
  if (!browser->hwnd() || !IsWindow(browser->hwnd())) {
    browser->prepare_close();
    browser.reset();
    ::ExitProcess(2);
  }
  if (!scenario || !scenario->run) {
    std::fprintf(stderr, "scenario: unknown id '%s'\n", suite_id);
    browser->prepare_close();
    browser.reset();
    ::ExitProcess(2);
  }
  const int rc = scenario->run(*browser);
  browser->prepare_close();
  browser.reset();
  ::ExitProcess(static_cast<UINT>(rc));
}

}  // namespace

int run_browser_main(const content::ContentMainParams&,
                     const ViewsLaunchOptions& options) {
  BASE_TRACE_EVENT("BrowserMain", "startup");
  LOGGING(LOG_INFO, "startup: BrowserMain begin");
  {
    BASE_TRACE_EVENT("DpiAwareness", "startup");
    ui::views::enable_process_dpi_awareness();
  }
  const AtmosphereShowcaseMode showcase = options.atmosphere_showcase;
  const Map2dShowcaseMode map2d_showcase = options.map2d_showcase;
  const UiShowcaseMode ui_showcase = options.ui_showcase;
  // Default Scene3d prefers FlyCube RHI. Switch via View -> Engine or
  // content::set_scene3d_engine (not env vars). --self-test / console /
  // atmosphere-showcase / map2d-showcase / ui-showcase select GDI before
  // Browser::init so multi-viewport FlyCube attach does not hang; atmosphere
  // may acquire FlyCube on its own HWND.
  const bool self_test = options.self_test;
  const bool self_test_console = options.self_test_console;
  const bool input_showcase = options.input_showcase;
  if (self_test || self_test_console || input_showcase ||
      showcase != AtmosphereShowcaseMode::kNone ||
      map2d_showcase != Map2dShowcaseMode::kNone ||
      ui_showcase != UiShowcaseMode::kNone) {
    content::set_scene3d_engine(content::Scene3dEngine::kGdi);
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_2D", "1");
  }
  // map2d-showcase: ContentMapView SharedSurface can "present" an empty ocean
  // DIB and then skip GDI overlay - HWND stays blank ocean while export_bmp
  // (software) still draws land. Force full Map2dPresenter::paint on overlay.
  if (map2d_showcase != Map2dShowcaseMode::kNone) {
    _putenv_s("SMT_FORCE_GDI_MAP_OVERLAY", "1");
  }
  {
    wchar_t diag[MAX_PATH] = {};
    if (detail::exe_sidecar_path(diag, MAX_PATH,
                                 L"atmosphere-showcase-cmdline.txt")) {
      FILE* f = nullptr;
      if (_wfopen_s(&f, diag, L"w") == 0 && f) {
        const wchar_t* cmd = GetCommandLineW();
        // fwprintf: %ls = wchar_t*, %hs = char* (MSVC). Using %s for cmd
        // treated a wide pointer as narrow and corrupted the stack - process
        // then died before Browser::show / atmosphere-showcase.
        std::fwprintf(f,
                      L"cmd=%ls\nshowcase=%hs\nmap2d=%hs\nui=%hs\nfields=%hs\n",
                      cmd ? cmd : L"(null)", atmosphere_showcase_name(showcase),
                      map2d_showcase_name(map2d_showcase),
                      ui_showcase_name(ui_showcase),
                      options.atmosphere_fields.c_str());
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
  // Agent / shot hooks: open Data or 3D without flaky synthetic clicks.
  if (const char* tab = std::getenv("SMT_VIEWS_START_MAP_TAB")) {
    int idx = 0;
    if (std::strcmp(tab, "scene3d") == 0 || std::strcmp(tab, "2") == 0) {
      idx = 2;
    } else if (std::strcmp(tab, "data") == 0 || std::strcmp(tab, "1") == 0) {
      idx = 1;
    } else if (tab[0] == '0' && tab[1] == '\0') {
      idx = 0;
    } else {
      idx = -1;
    }
    if (idx >= 0) {
      browser->select_map_tab(idx);
      pump_views_messages(800);
      LOGGING(LOG_INFO, "startup: SMT_VIEWS_START_MAP_TAB=%s -> tab %d", tab,
              idx);
    }
  }
  if (showcase != AtmosphereShowcaseMode::kNone) {
    if (const char* id = atmosphere_suite_id(showcase)) {
      exit_after_scenario(browser, id);
    }
  }
  if (map2d_showcase != Map2dShowcaseMode::kNone) {
    if (const char* id = map2d_suite_id(map2d_showcase)) {
      exit_after_scenario(browser, id);
    }
  }
  if (ui_showcase != UiShowcaseMode::kNone) {
    if (const char* id = ui_suite_id(ui_showcase)) {
      exit_after_scenario(browser, id);
    }
  }
  // Registry-backed suites (ids align with testing/tools/suites/*.json).
  // Mutual exclusion: --self-test-console wins over full --self-test.
  if (input_showcase) {
    exit_after_scenario(browser, "input");
  }
  if (self_test_console) {
    exit_after_scenario(browser, "console");
  }
  if (self_test) {
    exit_after_scenario(browser, "browse");
  }
  return browser->run_loop();
}

}  // namespace app
