// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/app/process/browser_main.h"

#include <windows.h>
#include <imm.h>

#if defined(_MSC_VER) && defined(_DEBUG)
#include <crtdbg.h>
#endif

#pragma comment(lib, "imm32.lib")

#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "app/views/app/startup/policy.h"
#include "app/views/browser/browser.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/self_test/self_test.h"
#include "app/views/harness/scenario_registry.h"
#include "app/views/util/exe_sidecar_path.h"
#include "base/core/log.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "content/public/plugin_host.h"
#include "content/browser/debug/debug_agent.h"
#include "ui/views/kernel/shell/dpi.h"

namespace app {
namespace {

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
  browser.set_defer_china_seed(defer_china);
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
  browser.select_map_tab(idx);
  pump_views_messages(800);
  LOGGING(LOG_INFO, "startup: VIEWS_START_MAP_TAB=%s -> tab %d", tab, idx);
}

}  // namespace

int run_browser_main(const content::ContentMainParams&,
                     const ViewsLaunchOptions& options_in) {
  // Copy by value before any large stack use. Browser / Session member ctors
  // can smash the caller's ViewsContentHost when the PE stack reserve is too
  // small (/STACK:16MiB in BUILD.gn). A value snapshot keeps plugin paths and
  // scenario_id stable for the rest of startup.
  const ViewsLaunchOptions options = options_in;
  BASE_TRACE_EVENT("BrowserMain", "startup");
  LOGGING(LOG_INFO, "startup: BrowserMain begin");
  {
    BASE_TRACE_EVENT("DpiAwareness", "startup");
    ui::views::enable_process_dpi_awareness();
  }
  disable_debug_crt_leak_abort();
  apply_startup_policy(startup_policy_for_scenario(options.scenario_id));
  if (const Scenario* scenario = find_scenario(options.scenario_id)) {
    if (scenario->kind == ScenarioKind::kSelfTest) {
      base::set_switch("harness-self-test", "1");
    }
  }
  {
    wchar_t diag[MAX_PATH] = {};
    if (detail::exe_capture_path(diag, MAX_PATH,
                                 L"atmosphere-showcase-cmdline.txt")) {
      FILE* f = nullptr;
      if (_wfopen_s(&f, diag, L"w") == 0 && f) {
        const wchar_t* cmd = GetCommandLineW();
        std::fwprintf(f, L"cmd=%ls\nscenario=%hs\nfields=%hs\n",
                      cmd ? cmd : L"(null)",
                      options.scenario_id.empty() ? "(product)"
                                                  : options.scenario_id.c_str(),
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
  const std::string scenario_id = options.scenario_id;
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
  if (plugin_present == "preview" && browser->plugins() &&
      browser->plugins()->host()) {
    browser->plugins()->host()->set_present_surface(1);
  }
  base::trace::dump_startup_profile_partial("post-init");
  if (debug_console || scenario_id == "console" ||
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
    if (!scenario_id.empty()) {
      ImmDisableIME(static_cast<DWORD>(-1));
    }
    browser->show();
  }
  LOGGING(LOG_INFO, "startup: first show complete");
  base::trace::maybe_dump_startup_profile();
  if (browser) {
    browser->finish_deferred_shell_wiring();
  }
  maybe_select_start_map_tab(*browser);
  if (!scenario_id.empty()) {
    exit_after_scenario(browser, scenario_id.c_str());
  }
  return browser->run_loop();
}

}  // namespace app
