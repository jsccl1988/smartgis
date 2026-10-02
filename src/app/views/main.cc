// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Views host PE entry. Parses launch options (CLI11), then dispatches via
// content::content_main to browser / gpu / renderer.

#include <windows.h>
#include <shellapi.h>

#include <utility>

#include "app/views/shell/app/views_content_host.h"
#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "base/core/log.h"
#include "base/trace/diag/diagnostic_bootstrap.h"
#include "base/trace/diag/startup_profile.h"
#include "base/trace/event/process_trace.h"
#include "content/app/content_main.h"
#include "ui/gfx/canvas/shell_canvas_backend.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  base::trace::maybe_init_tracing_from_env();
  base::trace::maybe_init_startup_profile_from_env();
  base::trace::start_always_on_diagnostics();
  BASE_TRACE_EVENT("wWinMain", "startup");
  LOGGING(LOG_INFO, "startup: wWinMain begin");

  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
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
