// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Views host PE entry. Parses launch options (CLI11), then dispatches via
// content::content_main to browser / gpu / renderer.

#include <windows.h>
#include <imm.h>
#include <shellapi.h>

#pragma comment(lib, "imm32.lib")

#include <cstdio>
#include <string>
#include <utility>

#include "app/views/app/host/content_host.h"
#include "app/views/app/cmdline/views_launch_options.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "base/trace/diag/diagnostic_bootstrap.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "content/public/content_client.h"
#include "ui/gfx/canvas/shell_canvas_backend.h"

namespace {

// Showcase / self-test / interact harness must not load third-party TSF IMEs
// (Sogou etc.): their uncaught C++ EH becomes STATUS_FATAL_USER_CALLBACK_EXCEPTION
// or a null-call AV during CreateWindow / ShowWindow / present.

// Call before any CreateWindow. ImmDisableIME after Widget.init is too late.
void disable_ime_for_harness() {
  ::ImmDisableIME(static_cast<DWORD>(-1));
  using ImmDisableTextFrameServiceFn = BOOL(WINAPI*)(DWORD);
  if (HMODULE imm = ::GetModuleHandleW(L"imm32.dll")) {
    if (auto* fn = reinterpret_cast<ImmDisableTextFrameServiceFn>(
            ::GetProcAddress(imm, "ImmDisableTextFrameService"))) {
      fn(static_cast<DWORD>(-1));
    }
  }
  // Prefer US English so Chinese TSF profiles are less likely to activate.
  if (HKL us = ::LoadKeyboardLayoutW(L"00000409", KLF_ACTIVATE)) {
    ::ActivateKeyboardLayout(us, KLF_SETFORPROCESS);
  }
}

}  // namespace

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  // Before any GDALAllRegister: AutoLoadDrivers LoadLibrary's every DLL in
  // cwd and, on some GDAL builds, next to gdald.dll (out/Debug). A second
  // ucrtbased mapping heap-corrupts PluginShell / CommandCatalog. The GDAL
  // sentinel "disable" skips plugin autoload; do not use SetDefaultDllDirectories
  // here — it can remap ucrtbased to a second base on this SDK.
  ::SetEnvironmentVariableW(L"GDAL_DRIVER_PATH", L"disable");
  // Redirected stderr (plain_browse capture) is fully buffered by default;
  // harness polls "startup: first show complete" — keep lines visible.
  setvbuf(stderr, nullptr, _IONBF, 0);
  base::init_switches_from_argv(argc, argv);
  base::trace::maybe_init_tracing_from_env();
  base::trace::maybe_init_startup_profile_from_env();
  BASE_TRACE_EVENT("wWinMain", "startup");
  LOGGING(LOG_INFO, "startup: wWinMain begin");

  app::ViewsLaunchOptions options;
  {
    BASE_TRACE_EVENT("ParseLaunchOptions", "startup");
    options = app::parse_views_launch_options(argc, argv);
  }
  if (!options.ok) {
    LOGGING(LOG_WARNING, "startup: launch options parse failed exit=%d",
            options.exit_code);
    if (argv) {
      LocalFree(argv);
    }
    return options.exit_code;
  }
  LOGGING(LOG_INFO, "startup: launch options ok process_type set");

  {
    BASE_TRACE_EVENT("PeekPluginStartup", "startup");
    // CLI --plugin-showcase wins when set; plugin.json startup.scenario fills
    // only when the argv scenario is empty (interactive / pack-default).
    std::string peeked_scenario;
    app::peek_plugin_startup(options.plugins_dir, &peeked_scenario,
                             &options.plugin_present,
                             &options.atmosphere_fields);
    if (options.scenario_id.empty()) {
      options.scenario_id = std::move(peeked_scenario);
    }
  }

  if (app::is_harness_launch(options)) {
    BASE_TRACE_EVENT("DisableImeForHarness", "startup");
    disable_ime_for_harness();
    LOGGING(LOG_INFO, "startup: ImmDisableIME+TextFrame+US layout (harness)");
  }
  // Product always-on diagnostics start after Browser::init (PluginShell).
  // Starting the memory sampler here raced debug CRT malloc in CommandCatalog.

  {
    BASE_TRACE_EVENT("ShellCanvasPreference", "startup");
    ui::gfx::apply_shell_canvas_preference(
        options.shell_canvas.empty() ? nullptr : options.shell_canvas.c_str());
  }

  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.process_type = options.process_type;
  params.process_type_set = true;

  app::ViewsContentHost host;
  host.options = std::move(options);
  LOGGING(LOG_INFO, "startup: content_main dispatch");
  int rc = 0;
  {
    BASE_TRACE_EVENT("ContentMain", "startup");
    rc = content::content_main(params, host);
  }
  LOGGING(LOG_INFO, "startup: content_main returned %d", rc);
  base::trace::maybe_dump_startup_profile();
  base::trace::maybe_dump_tracing_to_env();
  base::trace::stop_always_on_diagnostics();
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
