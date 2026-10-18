// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/process/browser_main.h"

#include <windows.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "app/views/app/startup/policy.h"
#include "app/views/app/startup/scenario.h"
#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/shell/plugin_shell.h"
#include "app/views/app/startup/shell_runtime.h"
#include "app/views/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/diag/diagnostic_bootstrap.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/public/plugin_host.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"
#include "content/browser/session/browser_session.h"
#include "ui/views/kernel/shell/dpi.h"

namespace app {
namespace {

// CI explicit post-start API: after product show, HARNESS_SUITE / --harness-suite
// selects a ScenarioRegistry row and exits. Never used for cold-start policy.
[[noreturn]] void exit_after_scenario(std::unique_ptr<Browser>& browser,
                                      const char* suite_id) {
  const Scenario* scenario = find_scenario(suite_id);
  if (scenario && scenario->mark_leaf) {
    content::detail::clear_mark(scenario->mark_leaf);
  }
  content::pump_views_messages(400);
  if (!browser->hwnd() || !IsWindow(browser->hwnd())) {
    ::TerminateProcess(::GetCurrentProcess(), 2);
  }
  if (!scenario || (!scenario->run && !scenario->suite_id &&
                    !scenario->plugin_command)) {
    std::fprintf(stderr, "scenario: unknown id '%s'\n", suite_id);
    ::TerminateProcess(::GetCurrentProcess(), 2);
  }
  const int rc = run_scenario(*browser, *scenario);
  const UINT code = static_cast<UINT>(rc);
  std::fflush(stderr);
  std::fflush(stdout);
  ::TerminateProcess(::GetCurrentProcess(), code);
  ::ExitProcess(code);
}

void disable_debug_crt_leak_abort() {
#if defined(_MSC_VER) && defined(_DEBUG)
  // Interactive product must not pop the CRT leak dialog or abort() on
  // process exit (heap skew under parallel DLL rebuilds).
  _CrtSetDbgFlag(0);
  _CrtSetReportMode(_CRT_WARN, 0);
  _CrtSetReportMode(_CRT_ERROR, 0);
  _CrtSetReportMode(_CRT_ASSERT, 0);
#endif
}

void apply_china_seed_switches(bool enable_oop_from_cli, Browser& browser) {
  bool enable_oop = enable_oop_from_cli;
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
  browser.set_enable_oop_render(enable_oop);

  // China seed: sync by default so bare SmartGIS.exe matches product carto.
  // DEFER_CHINA_SEED=1 restores the post-show timer path; SYNC_CHINA_SEED=1
  // forces sync.
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
  browser.set_defer_china_seed(defer_china);
  // Starting on Scene3d: do not block Browser::show on Map2d WaitFirstMapPresent
  // (SYNC_CHINA_SEED otherwise forces it). Tab switch after show owns 3D gold.
  const char* start_tab = base::switch_cstr("views-start-map-tab");
  const bool start_scene3d =
      start_tab &&
      (std::strcmp(start_tab, "scene3d") == 0 ||
       std::strcmp(start_tab, "1") == 0 || std::strcmp(start_tab, "2") == 0);
  if (!defer_china && !start_scene3d) {
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

void maybe_select_start_map_tab(Browser& browser) {
  const char* tab = base::switch_cstr("views-start-map-tab");
  if (!tab || !tab[0]) {
    return;
  }
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
  if (idx < 0) {
    return;
  }
  browser.select_view_tab(idx);
  content::pump_views_messages(800);
  // Deferred China seed / resume_all_presents may run during the pump and
  // briefly revive Map-Edit DXGI. Re-pin the start tab so Scene3d DEM stays.
  browser.select_view_tab(idx);
  content::pump_views_messages(100);
  LOGGING(LOG_INFO, "startup: VIEWS_START_MAP_TAB=%s -> tab %d", tab, idx);
  std::fflush(stderr);
}

void apply_plugin_product_startup(Browser& browser) {
  PluginShell* shell = browser.plugins();
  if (!shell) {
    return;
  }
  BASE_TRACE_EVENT("PluginStartup", "startup");
  // Env tab wins over plugin.json viewport — still run apply_startup for
  // enable/seed, but do not select_view_tab from startup_viewport afterward.
  const bool env_tab = base::switch_cstr("views-start-map-tab") != nullptr;
  if (!shell->apply_startup()) {
    LOGGING(LOG_WARNING, "startup: plugin.json startup apply failed");
    return;
  }
  if (env_tab) {
    return;
  }
  const std::string& vp = shell->startup_viewport();
  if (vp == "scene3d") {
    browser.select_view_tab(1);
    content::pump_views_messages(800);
    LOGGING(LOG_INFO, "startup: plugin.json viewport=scene3d");
  } else if (vp == "map2d") {
    browser.select_view_tab(0);
    content::pump_views_messages(800);
    LOGGING(LOG_INFO, "startup: plugin.json viewport=map2d");
  }
}

}  // namespace

int run_browser_main(const content::ContentMainParams&,
                     const ViewsLaunchOptions& options_in) {
  // Copy by value before any large stack use. Browser / Session member ctors
  // can smash the caller's ViewsContentHost when the PE stack reserve is too
  // small (/STACK:16MiB in BUILD.gn).
  ViewsLaunchOptions options = options_in;
  // Peek present/fields only — never fill a cold-start scenario_id.
  {
    std::string ignored_scenario;
    std::string peeked_present;
    std::string peeked_fields;
    peek_plugin_startup(options.plugins_dir, &ignored_scenario, &peeked_present,
                        &peeked_fields);
    if (options.plugin_present.empty()) {
      options.plugin_present = std::move(peeked_present);
    }
    if (options.atmosphere_fields.empty()) {
      options.atmosphere_fields = std::move(peeked_fields);
    }
  }
  BASE_TRACE_EVENT("BrowserMain", "startup");
  LOGGING(LOG_INFO, "startup: BrowserMain begin");
  {
    BASE_TRACE_EVENT("DpiAwareness", "startup");
    ui::views::enable_process_dpi_awareness();
  }
  disable_debug_crt_leak_abort();
  // HARNESS_SUITE pins ContentMapView / FORCE_GDI before DrawHost attach so
  // browse BitBlt / motion_gate see MapIR→HDC (not product FlyCube ocean).
  if (const char* suite = base::switch_cstr("harness-suite");
      suite && suite[0]) {
    apply_startup_policy(startup_policy_for_scenario(suite));
  } else {
    apply_startup_policy(product_startup_policy());
  }
  {
    wchar_t diag[MAX_PATH] = {};
    if (content::detail::exe_capture_path(diag, MAX_PATH, L"product-startup-cmdline.txt")) {
      FILE* f = nullptr;
      if (_wfopen_s(&f, diag, L"w") == 0 && f) {
        const wchar_t* cmd = GetCommandLineW();
        std::fwprintf(f, L"cmd=%ls\npolicy=product\nfields=%hs\n",
                      cmd ? cmd : L"(null)",
                      options.atmosphere_fields.empty()
                          ? "(none)"
                          : options.atmosphere_fields.c_str());
        std::fclose(f);
      }
    }
  }
  const std::string plugins_dir = options.plugins_dir;
  const std::string atmosphere_fields = options.atmosphere_fields;
  const bool debug_console = options.debug_console;
  const std::string plugin_present = options.plugin_present;
  const bool enable_oop_render = options.enable_oop_render;
  std::unique_ptr<Browser> browser;
  {
    BASE_TRACE_EVENT("Browser.ctor", "startup");
    browser = std::make_unique<Browser>();
  }
  browser->set_plugins_dir(plugins_dir);
  apply_china_seed_switches(enable_oop_render, *browser);
  {
    BASE_TRACE_EVENT("Browser.init", "startup");
    LOGGING(LOG_INFO, "startup: Browser::init");
    if (!browser->init()) {
      LOGGING(LOG_ERROR, "startup: Browser::init failed");
      base::trace::maybe_dump_startup_profile();
      return 1;
    }
  }
  {
    BASE_TRACE_EVENT("AlwaysOnDiagnostics", "startup");
    base::trace::start_always_on_diagnostics();
  }
  if (plugin_present == "preview" && browser->plugins() &&
      browser->plugins()->host()) {
    browser->plugins()->host()->set_present_surface(1);
  }
  base::trace::dump_startup_profile_partial("post-init");
  if (debug_console || content::BrowserSession::debug_console_env_enabled()) {
    BASE_TRACE_EVENT("DebugAgent.start", "startup");
    LOGGING(LOG_INFO, "startup: DebugAgent start");
    content::BrowserSession::start_debug_agent();
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
    std::fflush(stderr);
    browser->show();
  }
  LOGGING(LOG_INFO, "startup: first show complete");
  std::fflush(stderr);
  base::trace::maybe_dump_startup_profile();
  if (browser) {
    browser->finish_deferred_shell_wiring();
  }
  // Plugin apply_startup may present_dataset → select_view_tab(0). Run that
  // first, then honor VIEWS_START_MAP_TAB so Map/3D chrome matches the live
  // present (plain-launch visual_review #6).
  apply_plugin_product_startup(*browser);
  maybe_select_start_map_tab(*browser);
  // Explicit CI suite only (loop prepare_env sets HARNESS_SUITE). Never from
  // CLI showcase, sticky SHOWCASE env, or plugin.json startup.scenario.
  if (const char* suite = base::switch_cstr("harness-suite");
      suite && suite[0]) {
    exit_after_scenario(browser, suite);
  }
  return browser->run_loop();
}

}  // namespace app
