// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Views host PE entry. Parses launch options (CLI11), then dispatches via
// content::content_main to browser / gpu / renderer.

#include <windows.h>
#include <shellapi.h>

#include <utility>

#include "app/views/shell/app/views_content_host.h"
#include "app/views/shell/app/cmdline/views_launch_options.h"
#include "content/app/content_main.h"
#include "ui/gfx/canvas/shell_canvas_backend.h"

int APIENTRY wWinMain(HINSTANCE instance, HINSTANCE, wchar_t*, int) {
  int argc = 0;
  wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
  app::ViewsLaunchOptions options =
      app::parse_views_launch_options(argc, argv);
  if (!options.ok) {
    if (argv) {
      LocalFree(argv);
    }
    return options.exit_code;
  }

  ui::gfx::apply_shell_canvas_preference(
      options.shell_canvas.empty() ? nullptr : options.shell_canvas.c_str());

  content::ContentMainParams params;
  params.instance = instance;
  params.argc = argc;
  params.argv = argv;
  params.process_type = options.process_type;
  params.process_type_set = true;

  app::ViewsContentHost host;
  host.options = std::move(options);
  const int rc = content::content_main(params, host);
  if (argv) {
    LocalFree(argv);
  }
  return rc;
}
