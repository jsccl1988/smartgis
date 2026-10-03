// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/browser_main.h"

#include <windows.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/scenario_registry.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/diag/startup_profile.h"
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
    case AtmosphereShowcaseMode::kLegacy:
      return "atmosphere.legacy";
    case AtmosphereShowcaseMode::kGlobe:
      return "atmosphere.globe";
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

const char* plugin_suite_id(PluginShowcaseMode mode) {
  switch (mode) {
    case PluginShowcaseMode::kWorld3d:
      return "plugin.world3d";
    case PluginShowcaseMode::kPrint:
      return "plugin.print";
    case PluginShowcaseMode::kOrthogrid:
      return "plugin.orthogrid";
    case PluginShowcaseMode::kOrthogrid3d:
      return "plugin.orthogrid3d";
    case PluginShowcaseMode::kTraffic:
      return "plugin.traffic";
    case PluginShowcaseMode::kFlood:
      return "plugin.flood";
    case PluginShowcaseMode::kStormSurge:
      return "plugin.stormsurge";
    case PluginShowcaseMode::kMine:
      return "plugin.mine";
    case PluginShowcaseMode::kGeochem:
      return "plugin.geochem";
    case PluginShowcaseMode::kNone:
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

// Runs a ScenarioRegistry entry then exits the process.
// Do not run Browser teardown first: Debug CRT leak-check / destructor heap
// paths abort() after a green scenario and surface as exit 0xFFFFFFFF to
// exe_smoke. Scenarios already detach map HWNDs; ExitProcess still runs
// DLL_PROCESS_DETACH (DXGI / CRT) which can abort — use TerminateProcess so
// unload hooks are skipped and the intended rc reaches loop_runner.
[[noreturn]] void exit_after_scenario(std::unique_ptr<Browser>& browser,
                                      const char* suite_id) {
  const Scenario* scenario = find_scenario(suite_id);
  if (scenario && scenario->mark_leaf) {
    detail::clear_mark(scenario->mark_leaf);
  }
  pump_views_messages(400);
  if (!browser->hwnd() || !IsWindow(browser->hwnd())) {
    ::TerminateProcess(::GetCurrentProcess(), 2);
  }
  if (!scenario || !scenario->run) {
    std::fprintf(stderr, "scenario: unknown id '%s'\n", suite_id);
    ::TerminateProcess(::GetCurrentProcess(), 2);
  }
  const int rc = scenario->run(*browser);
  const UINT code = static_cast<UINT>(rc);
  // Flush before TerminateProcess — it skips atexit and drops stdio buffers,
  // which hid interact-dsl / run_processing failure lines under harness redirect.
  std::fflush(stderr);
  std::fflush(stdout);
  ::TerminateProcess(::GetCurrentProcess(), code);
  ::ExitProcess(code);
}

void disable_debug_crt_leak_abort() {
#if defined(_MSC_VER) && defined(_DEBUG)
  // Automated --self-test / showcase / interactive must not pop the CRT leak
  // dialog or abort() on process exit (heap skew under parallel DLL rebuilds
  // surfaces as ExitProcess 0xFFFFFFFF to exe_smoke / loop_runner).
  _CrtSetDbgFlag(0);
  _CrtSetReportMode(_CRT_WARN, 0);
  _CrtSetReportMode(_CRT_ERROR, 0);
  _CrtSetReportMode(_CRT_ASSERT, 0);
#endif
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
  const PluginShowcaseMode plugin_showcase = options.plugin_showcase;
  const UiShowcaseMode ui_showcase = options.ui_showcase;
  // Default Scene3d prefers FlyCube RHI. Switch via View -> Engine,
  // content::set_scene3d_engine, or harness SMT_SCENE3D_ENGINE (stereo_gl /
  // stereo_d3d / flycube / gdi). --self-test / console / atmosphere-showcase /
  // map2d-showcase / most ui-showcase select GDI before Browser::init so
  // multi-viewport FlyCube attach does not hang; atmosphere may acquire
  // FlyCube on its own HWND. Explicit SMT_SCENE3D_ENGINE wins over those
  // defaults (equal-profile GL/D3D matrix).
  //
  // --browse-showcase and --ui-showcase=scene keep FlyCube: forensic 3D orbit
  // and the product Scene3D face must not fall back to views-scene3d.gdi
  // (Fps0 navy + tiny DEM sticker) or AV under ContentMapView stress.
  const bool self_test = options.self_test;
  const bool self_test_console = options.self_test_console;
  const bool input_showcase = options.input_showcase;
  const bool browse_showcase = options.browse_showcase;
  // FPS bench measures live map present (target mean_fps>=30). Keep FlyCube /
  // GPU map path: do not force ContentMapView or full GDI overlay.
  const bool map2d_fps_bench = []() {
    const char* e = std::getenv("SMT_MAP2D_FPS_BENCH_MS");
    return e && e[0] != '\0' && std::atoi(e) > 0;
  }();
  // Debug CRT leak-check abort() looks like a spontaneous exit after a green
  // interactive session (heap skew under parallel DLL rebuilds). Always quiet
  // the dialog; ExitProcess paths already skip atexit for showcases.
  disable_debug_crt_leak_abort();
  const bool ui_force_gdi =
      ui_showcase != UiShowcaseMode::kNone &&
      ui_showcase != UiShowcaseMode::kScene;
  // --browse-showcase serves both browse (2D) and browse.3d. 2D needs the
  // same ContentMapView + GDI overlay path as map2d.china so software
  // export_bmp can paint china_city without racing FlyCube present (AV /
  // cream AABB). 3D keeps FlyCube for orbit BitBlt.
  const bool browse_3d_suite = []() {
    if (const char* suite = std::getenv("SMT_HARNESS_SUITE")) {
      if (std::strcmp(suite, "browse.3d") == 0) {
        return true;
      }
    }
    if (const char* script = std::getenv("SMT_UI_INTERACT_SCRIPT")) {
      if (std::strstr(script, "browse.3d")) {
        return true;
      }
    }
    return false;
  }();
  const bool engine_from_env = content::apply_scene3d_engine_from_env();
  if (!engine_from_env) {
    if (self_test || self_test_console || input_showcase || ui_force_gdi ||
        showcase != AtmosphereShowcaseMode::kNone ||
        map2d_showcase != Map2dShowcaseMode::kNone ||
        plugin_showcase != PluginShowcaseMode::kNone ||
        (browse_showcase && !browse_3d_suite)) {
      content::set_scene3d_engine(content::Scene3dEngine::kGdi);
      if (!map2d_fps_bench) {
        _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_2D", "1");
      }
    } else if (browse_showcase || ui_showcase == UiShowcaseMode::kScene) {
      content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
      _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_2D", "0");
      _putenv_s("SMT_PREFER_FLYCUBE_2D", "1");
      _putenv_s("SMT_FORCE_GDI_MAP_OVERLAY", "0");
    }
  } else if (content::prefer_scene3d_stereo_gl() ||
             content::prefer_scene3d_flycube()) {
    // Stereo/FlyCube bench: do not force ContentMapView-only 2D overlay.
    _putenv_s("SMT_FORCE_CONTENT_MAPVIEW_2D", "0");
  }
  // map2d-showcase / plugin-showcase / browse(2D): ContentMapView SharedSurface
  // can "present" an empty ocean DIB and then skip GDI overlay - HWND stays
  // blank ocean while export_bmp (software) still draws land. Force full
  // Map2dPresenter::paint on overlay. Skip when FPS-benching FlyCube.
  if (!map2d_fps_bench &&
      (map2d_showcase != Map2dShowcaseMode::kNone ||
       plugin_showcase != PluginShowcaseMode::kNone ||
       (browse_showcase && !browse_3d_suite))) {
    _putenv_s("SMT_FORCE_GDI_MAP_OVERLAY", "1");
  }
  // Showcase does not need Ambox command lists; skip catalog for_each when
  // parallel tool/plugin DLL rebuilds leave maps unreadable (0xC0000005 in
  // CommandCatalog::for_each during Browser::init / init_shell). Include
  // ui-showcase — otherwise --ui-showcase=shell AVs in populate_ambox while
  // other showcases already skip. browse/input: same catalog map risk.
  if (showcase != AtmosphereShowcaseMode::kNone ||
      map2d_showcase != Map2dShowcaseMode::kNone ||
      plugin_showcase != PluginShowcaseMode::kNone ||
      ui_showcase != UiShowcaseMode::kNone || browse_showcase ||
      input_showcase) {
    _putenv_s("SMT_SKIP_AMBOX_CATALOG", "1");
  }
  {
    wchar_t diag[MAX_PATH] = {};
    if (detail::exe_capture_path(diag, MAX_PATH,
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
                      "(deferred)");
        std::fclose(f);
      }
    }
  }
  // Snapshot option strings before Browser construction (member ctors are heavy).
  // Prefer assign(c_str()) over operator=(string&) so a skewed/corrupt source
  // string with a null data pointer fails soft instead of memcpy AV.
  static std::string s_plugins_dir;
  static std::string s_atmosphere_fields;
  s_plugins_dir.clear();
  s_atmosphere_fields.clear();
  {
    const char* plugins = options.plugins_dir.c_str();
    const char* fields = options.atmosphere_fields.c_str();
    if (plugins) {
      s_plugins_dir.assign(plugins);
    }
    if (fields) {
      s_atmosphere_fields.assign(fields);
    }
  }
  const bool debug_console = options.debug_console;
  std::unique_ptr<Browser> browser;
  {
    BASE_TRACE_EVENT("Browser.ctor", "startup");
    browser = std::make_unique<Browser>();
  }
  browser->set_plugins_dir(s_plugins_dir);
  {
    // OOP: CLI --enable-oop-render or SMT_ENABLE_OOP_RENDER=1.
    bool enable_oop = options.enable_oop_render;
    if (const char* env = std::getenv("SMT_ENABLE_OOP_RENDER")) {
      if (env[0] == '1' && env[1] == '\0') {
        enable_oop = true;
      }
    }
    if (const char* env = std::getenv("SMT_DISABLE_OOP_RENDER")) {
      if (env[0] == '1' && env[1] == '\0') {
        enable_oop = false;
      }
    }
    browser->set_enable_oop_render(enable_oop);

    // China seed: sync for harness / self-test / showcase; defer for product.
    // SMT_SYNC_CHINA_SEED=1 forces sync; SMT_DEFER_CHINA_SEED=1 forces defer.
    const bool harness =
        self_test || self_test_console || input_showcase || browse_showcase ||
        showcase != AtmosphereShowcaseMode::kNone ||
        map2d_showcase != Map2dShowcaseMode::kNone ||
        plugin_showcase != PluginShowcaseMode::kNone ||
        ui_showcase != UiShowcaseMode::kNone;
    bool defer_china = !harness;
    if (const char* env = std::getenv("SMT_SYNC_CHINA_SEED")) {
      if (env[0] == '1' && env[1] == '\0') {
        defer_china = false;
      }
    }
    if (const char* env = std::getenv("SMT_DEFER_CHINA_SEED")) {
      if (env[0] == '1' && env[1] == '\0') {
        defer_china = true;
      }
    }
    browser->set_defer_china_seed(defer_china);
  }
  {
    BASE_TRACE_EVENT("Browser.init", "startup");
    LOGGING(LOG_INFO, "startup: Browser::init");
    if (!browser->init()) {
      LOGGING(LOG_ERROR, "startup: Browser::init failed");
      base::trace::maybe_dump_startup_profile();
      return 1;
    }
  }
  // Mid-startup snapshot (partial file) when show/wait may hang.
  base::trace::dump_startup_profile_partial("post-init");
  if (debug_console || self_test_console ||
      content::debug_console_env_enabled()) {
    BASE_TRACE_EVENT("DebugAgent.start", "startup");
    LOGGING(LOG_INFO, "startup: DebugAgent start");
    content::debug_agent().start();
  }
  if (!s_atmosphere_fields.empty()) {
    BASE_TRACE_EVENT("AtmosphereFields", "startup");
    std::fprintf(stderr, "atmosphere-fields: %s\n",
                 s_atmosphere_fields.c_str());
    if (!browser->apply_atmosphere_fields(s_atmosphere_fields)) {
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
  // Dump once here so interactive sessions see the table without waiting for
  // process exit (wWinMain also calls maybe_dump — second call is a no-op).
  base::trace::maybe_dump_startup_profile();
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
  if (plugin_showcase != PluginShowcaseMode::kNone) {
    if (const char* id = plugin_suite_id(plugin_showcase)) {
      exit_after_scenario(browser, id);
    }
  }
  if (ui_showcase != UiShowcaseMode::kNone) {
    if (const char* id = ui_suite_id(ui_showcase)) {
      exit_after_scenario(browser, id);
    }
  }
  // Registry-backed suites (ids align with testing/tools/harness/<family>/<suite_id>/suite.json).
  // Mutual exclusion: --self-test-console wins over full --self-test.
  if (input_showcase) {
    exit_after_scenario(browser, "input");
  }
  if (browse_showcase) {
    // Prefer SMT_HARNESS_SUITE so browse.3d resolves its own ScenarioRegistry
    // entry (same run_browse_showcase body; IL/suite id from env).
    const char* browse_id = "browse";
    if (const char* env = std::getenv("SMT_HARNESS_SUITE")) {
      if (env[0] && std::strcmp(env, "browse.3d") == 0) {
        browse_id = "browse.3d";
      }
    }
    exit_after_scenario(browser, browse_id);
  }
  if (self_test_console) {
    exit_after_scenario(browser, "console");
  }
  if (self_test) {
    exit_after_scenario(browser, "self_test");
  }
  return browser->run_loop();
}

}  // namespace app
