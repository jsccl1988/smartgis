// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/app/browser_main.h"

#include <windows.h>
#include <imm.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#pragma comment(lib, "imm32.lib")

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/scenario_registry.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/public/plugin_host.h"
#include "content/browser/debug/debug_agent.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
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

std::string plugin_suite_id(const std::string& mode) {
  if (mode.empty()) {
    return {};
  }
  return "plugin." + mode;
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
                     const ViewsLaunchOptions& options_in) {
  // Copy by value before any large stack use. Browser / Session member ctors
  // can smash the caller's ViewsContentHost when the PE stack reserve is too
  // small (/STACK:16MiB in BUILD.gn). A value snapshot keeps plugin paths and
  // atmosphere fields stable for the rest of startup.
  const ViewsLaunchOptions options = options_in;
  BASE_TRACE_EVENT("BrowserMain", "startup");
  LOGGING(LOG_INFO, "startup: BrowserMain begin");
  {
    BASE_TRACE_EVENT("DpiAwareness", "startup");
    ui::views::enable_process_dpi_awareness();
  }
  const AtmosphereShowcaseMode showcase = options.atmosphere_showcase;
  const Map2dShowcaseMode map2d_showcase = options.map2d_showcase;
  const std::string& plugin_showcase = options.plugin_showcase;
  const UiShowcaseMode ui_showcase = options.ui_showcase;
  // Default Scene3d prefers FlyCube RHI. Switch via View -> Engine,
  // content::set_scene3d_engine, or harness SCENE3D_ENGINE (stereo_gl /
  // stereo_d3d / flycube / gdi). --self-test / console / atmosphere-showcase /
  // map2d-showcase / most ui-showcase select GDI before Browser::init so
  // multi-viewport FlyCube attach does not hang; atmosphere may acquire
  // FlyCube on its own HWND. Explicit SCENE3D_ENGINE wins over those
  // defaults (equal-profile GL/D3D matrix).
  //
  // --browse-showcase, --ui-showcase=scene, and --ui-showcase=interact keep
  // FlyCube: forensic 3D orbit / 2D→3D→2D gestures must not fall back to
  // views-scene3d.gdi (Fps0 navy, no China DEM). 2D chrome-only showcases
  // still force GDI so multi-viewport FlyCube attach does not hang.
  const bool self_test = options.self_test;
  const bool self_test_console = options.self_test_console;
  const bool input_showcase = options.input_showcase;
  const bool browse_showcase = options.browse_showcase;
  // FPS bench measures live map present (target mean_fps>=30). Keep FlyCube /
  // GPU map path: do not force ContentMapView or full GDI overlay.
  const bool map2d_fps_bench = []() {
    const char* e = base::switch_cstr("map2d-fps-bench-ms");
    return e && e[0] != '\0' && std::atoi(e) > 0;
  }();
  // Debug CRT leak-check abort() looks like a spontaneous exit after a green
  // interactive session (heap skew under parallel DLL rebuilds). Always quiet
  // the dialog; ExitProcess paths already skip atexit for showcases.
  disable_debug_crt_leak_abort();
  const bool ui_scene3d_face =
      ui_showcase == UiShowcaseMode::kScene ||
      ui_showcase == UiShowcaseMode::kInteract;
  const bool ui_force_gdi =
      ui_showcase != UiShowcaseMode::kNone && !ui_scene3d_face;
  // --browse-showcase serves both browse (2D) and browse.3d. 2D needs the
  // same ContentMapView + GDI overlay path as map2d.china so software
  // export_bmp can paint china_city without racing FlyCube present (AV /
  // cream AABB). 3D keeps FlyCube for orbit BitBlt.
  const bool browse_3d_suite = []() {
    if (const char* suite = base::switch_cstr("harness-suite")) {
      if (std::strcmp(suite, "browse.3d") == 0) {
        return true;
      }
    }
    if (const char* script = base::switch_cstr("ui-interact-script")) {
      if (std::strstr(script, "browse.3d")) {
        return true;
      }
    }
    return false;
  }();
  // Bare product (no showcase/self-test): match --ui-showcase=shell 2D face —
  // ContentMapView + GDI overlay paints china onto the shell HWND. FlyCube
  // remains available via PREFER_FLYCUBE_2D=1 / FPS bench / scene showcase.
  const bool bare_product =
      ui_showcase == UiShowcaseMode::kNone && !self_test &&
      !self_test_console && !input_showcase && !browse_showcase &&
      showcase == AtmosphereShowcaseMode::kNone &&
      map2d_showcase == Map2dShowcaseMode::kNone &&
      plugin_showcase.empty();
  const bool engine_from_env = content::apply_scene3d_engine_from_env();
  // Scene3D product plugin faces need FlyCube lit DEM (world3d / mine /
  // stormsurge / orthogrid3d). Map2d plugin IL suites still prefer GDI.
  const bool plugin_scene3d_face =
      plugin_showcase == "world3d" || plugin_showcase == "world_preview" ||
      plugin_showcase == "mine" || plugin_showcase == "stormsurge" ||
      plugin_showcase == "orthogrid3d";
  // Scene3D plugin faces present on an owned showcase HWND + Device
  // (prepare_plugin_device_session). Keep content Scene3dEngine::kFlyCube
  // for that path, but force ContentMapView on the shell DrawHost so the
  // async FlyCube display thread cannot abort before the plugin body runs.
  // Must apply even when SCENE3D_ENGINE=flycube already set the engine
  // (otherwise the prefer_scene3d_flycube block below clears the gate).
  if (plugin_scene3d_face) {
    if (!engine_from_env) {
      content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
    }
    base::set_switch("force-content-mapview-2d", "1");
    base::set_switch("force-gdi-map-overlay", "1");
  } else if (!engine_from_env) {
    if (ui_showcase == UiShowcaseMode::kInteract) {
      // 2D stays ContentMapView (no dual FlyCube hang). 3D lazy-attaches
      // FlyCube on select_map_tab(1) so China DEM + labels is the SoT.
      content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
      base::set_switch("force-content-mapview-2d", "1");
      base::set_switch("prefer-flycube-2d", "0");
      base::set_switch("force-gdi-map-overlay", "0");
    } else if (self_test || self_test_console || input_showcase ||
               ui_force_gdi || showcase != AtmosphereShowcaseMode::kNone ||
               map2d_showcase != Map2dShowcaseMode::kNone ||
               !plugin_showcase.empty() ||
               (browse_showcase && !browse_3d_suite)) {
      content::set_scene3d_engine(content::Scene3dEngine::kGdi);
      if (!map2d_fps_bench) {
        base::set_switch("force-content-mapview-2d", "1");
      }
    } else if (bare_product && !map2d_fps_bench) {
      // 2D china carto on the Map/Data HWND only. Do not demote Scene3d to
      // GDI/ContentMapView — that dual-SoT navy-fills the 3D tab every tick.
      base::set_switch("force-content-mapview-2d", "1");
    } else if (browse_showcase || ui_showcase == UiShowcaseMode::kScene) {
      content::set_scene3d_engine(content::Scene3dEngine::kFlyCube);
      base::set_switch("force-content-mapview-2d", "0");
      base::set_switch("prefer-flycube-2d", "1");
      base::set_switch("force-gdi-map-overlay", "0");
    }
  }
  // Stereo/FlyCube bench: do not force ContentMapView-only 2D overlay.
  // Interact keeps Content 2D + FlyCube 3D (set above / suite env).
  if (!plugin_scene3d_face &&
      (content::prefer_scene3d_stereo_gl() ||
       content::prefer_scene3d_flycube())) {
    if (ui_showcase != UiShowcaseMode::kInteract) {
      base::set_switch("force-content-mapview-2d", "0");
    }
  }
  // Scenic software present: never attach FlyCube HWND on shell map panes
  // (display_run_present SEH). Product HWND + Map2d/Scene3dPresenter paint
  // is the SoT — ContentMapView SharedSurface hid scenic HUD and froze FPS.
  if (content::prefer_map2d_scenic() || content::prefer_scene3d_scenic()) {
    base::set_switch("force-content-mapview-2d", "0");
    base::set_switch("prefer-flycube-2d", "0");
    if (content::prefer_map2d_scenic()) {
      base::set_switch("map2d-engine", "scenic");
    }
    if (content::prefer_scene3d_scenic()) {
      base::set_switch("scene3d-engine", "scenic");
    }
  }
  // Force full Map2dPresenter::paint on the shell overlay so the HWND never
  // stays ocean-only while FlyCube DXGI is still hidden / clearing. Covers
  // map2d/plugin/browse(2D) showcases and bare SmartGIS.exe (same china
  // face as --ui-showcase=shell). Skip when FPS-benching FlyCube or when the
  // operator explicitly sets FORCE_GDI_MAP_OVERLAY=0.
  if (!map2d_fps_bench) {
    const char* force_gdi = base::switch_cstr("force-gdi-map-overlay");
    const bool force_off =
        force_gdi && force_gdi[0] == '0' && force_gdi[1] == '\0';
    if (!force_off &&
        (map2d_showcase != Map2dShowcaseMode::kNone ||
         !plugin_showcase.empty() ||
         (browse_showcase && !browse_3d_suite) || bare_product ||
         (ui_showcase != UiShowcaseMode::kNone && !ui_scene3d_face))) {
      base::set_switch("force-gdi-map-overlay", "1");
    }
  }
  // Map2d / plugin / atmosphere / browse / input skip CommandCatalog walks
  // (parallel DLL rebuild AVs). UI showcase follows the product Browser::show
  // path so Ambox chips + deferred China match no-args SmartGIS.exe.
  if (showcase != AtmosphereShowcaseMode::kNone ||
      map2d_showcase != Map2dShowcaseMode::kNone ||
      !plugin_showcase.empty() || browse_showcase ||
      input_showcase) {
    base::set_switch("skip-ambox-catalog", "1");
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
  // Keep string copies for post-Browser use (set_plugins_dir / fields ingest).
  // Do not assign(c_str()): a poison 0xCC pointer (stale ViewsLaunchOptions
  // layout) passes a null check and still AVs inside char_traits::length.
  const std::string plugins_dir = options.plugins_dir;
  const std::string atmosphere_fields = options.atmosphere_fields;
  const bool debug_console = options.debug_console;
  std::unique_ptr<Browser> browser;
  {
    BASE_TRACE_EVENT("Browser.ctor", "startup");
    browser = std::make_unique<Browser>();
  }
  browser->set_plugins_dir(plugins_dir);
  {
    // OOP: CLI --enable-oop-render or ENABLE_OOP_RENDER=1.
    bool enable_oop = options.enable_oop_render;
    if (const char* env = base::switch_cstr("enable-oop-render")) {
      if (env[0] == '1' && env[1] == '\0') {
        enable_oop = true;
      }
    }
    if (const char* env = base::switch_cstr("disable-oop-render")) {
      if (env[0] == '1' && env[1] == '\0') {
        enable_oop = false;
      }
    }
    browser->set_enable_oop_render(enable_oop);

    // China seed: sync by default so bare SmartGIS.exe matches the
    // --ui-showcase=shell carto face (china_city Land/Lines/Points/Labels).
    // DEFER_CHINA_SEED=1 restores the post-show timer path; SYNC_CHINA_SEED=1
    // forces sync. Showcase / harness that set SKIP_AMBOX_CATALOG still skip
    // OGR here and re-seed in their own china_seed helpers.
    bool defer_china = false;
    if (const char* env = base::switch_cstr("defer-china-seed")) {
      if (env[0] == '1' && env[1] == '\0') {
        defer_china = true;
      }
    }
    if (const char* env = base::switch_cstr("sync-china-seed")) {
      if (env[0] == '1' && env[1] == '\0') {
        defer_china = false;
      }
    }
    browser->set_defer_china_seed(defer_china);
    // Locked startup profile: china sync seed + first-present gate share the
    // same harness env; default sync-first-map-present when only seed is set.
    if (!defer_china) {
      const char* sync_present = base::switch_cstr("sync-first-map-present");
      const bool have_sync_present =
          sync_present && sync_present[0] == '1' && sync_present[1] == '\0';
      if (!have_sync_present) {
        if (const char* sync_seed = base::switch_cstr("sync-china-seed")) {
          if (sync_seed[0] == '1' && sync_seed[1] == '\0') {
            base::set_switch("sync-first-map-present", "1");
          }
        }
      }
    }
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
  if (options.plugin_present == "preview" && browser->plugins() &&
      browser->plugins()->host()) {
    browser->plugins()->host()->set_present_surface(1);
  }
  // Mid-startup snapshot (partial file) when show/wait may hang.
  base::trace::dump_startup_profile_partial("post-init");
  if (debug_console || self_test_console ||
      content::debug_console_env_enabled()) {
    BASE_TRACE_EVENT("DebugAgent.start", "startup");
    LOGGING(LOG_INFO, "startup: DebugAgent start");
    content::debug_agent().start();
  }
  if (!atmosphere_fields.empty()) {
    BASE_TRACE_EVENT("AtmosphereFields", "startup");
    std::fprintf(stderr, "atmosphere-fields: %s\n", atmosphere_fields.c_str());
    if (!browser->apply_atmosphere_fields(atmosphere_fields)) {
      std::fprintf(stderr, "atmosphere-fields: load failed\n");
      LOGGING(LOG_WARNING, "startup: atmosphere-fields load failed");
    }
  }
  {
    BASE_TRACE_EVENT("Browser.show", "startup");
    LOGGING(LOG_INFO, "startup: Browser::show");
    // Belt-and-suspenders: wWinMain already disables IME/TSF before any
    // CreateWindow for harness launches. Keep a pre-ShowWindow call in case
    // a non-harness path later gains showcase-like window churn.
    const bool harness_show =
        showcase != AtmosphereShowcaseMode::kNone ||
        map2d_showcase != Map2dShowcaseMode::kNone ||
        !plugin_showcase.empty() ||
        ui_showcase != UiShowcaseMode::kNone || browse_showcase ||
        input_showcase || self_test || self_test_console;
    if (harness_show) {
      ImmDisableIME(static_cast<DWORD>(-1));
    }
    browser->show();
  }
  LOGGING(LOG_INFO, "startup: first show complete");
  // Dump once here so interactive sessions see the table without waiting for
  // process exit (wWinMain also calls maybe_dump — second call is a no-op).
  base::trace::maybe_dump_startup_profile();
  // Catalog / inspector / tool seams / gestures are not on the first-carto
  // gate. Run after the dump so WireShell.deferred does not inflate wall_ms.
  if (browser) {
    browser->finish_deferred_shell_wiring();
  }
  // Agent / shot hooks: open 3D without flaky synthetic clicks. Data tab is gone.
  if (const char* tab = base::switch_cstr("views-start-map-tab")) {
    int idx = 0;
    if (std::strcmp(tab, "scene3d") == 0 || std::strcmp(tab, "2") == 0 ||
        std::strcmp(tab, "1") == 0) {
      idx = 1;
    } else if (std::strcmp(tab, "data") == 0) {
      idx = 0;
    } else if (tab[0] == '0' && tab[1] == '\0') {
      idx = 0;
    } else {
      idx = -1;
    }
    if (idx >= 0) {
      browser->select_map_tab(idx);
      pump_views_messages(800);
      LOGGING(LOG_INFO, "startup: VIEWS_START_MAP_TAB=%s -> tab %d", tab,
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
  if (!plugin_showcase.empty()) {
    const std::string suite = plugin_suite_id(plugin_showcase);
    if (!suite.empty()) {
      exit_after_scenario(browser, suite.c_str());
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
    // Prefer HARNESS_SUITE so browse.3d resolves its own ScenarioRegistry
    // entry (same run_browse_showcase body; IL/suite id from env).
    const char* browse_id = "browse";
    if (const char* env = base::switch_cstr("harness-suite")) {
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
