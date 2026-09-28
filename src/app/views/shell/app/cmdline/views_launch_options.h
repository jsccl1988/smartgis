// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_
#define APP_VIEWS_SHELL_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_

#include <string>

#include "content/app/process_type.h"

namespace app {

// Automated 3D atmosphere demos (distinct from full --self-test shell path).
enum class AtmosphereShowcaseMode {
  kNone,
  kLand,
  kOcean,
  kFull,
  kCoast,
};

// Parsed Views PE switches. Pure data — no HWND / Browser.
struct ViewsLaunchOptions {
  content::ProcessType process_type = content::ProcessType::kBrowser;
  bool self_test = false;
  AtmosphereShowcaseMode atmosphere_showcase = AtmosphereShowcaseMode::kNone;
  std::string atmosphere_fields;
  // Empty = unset (caller may fall back to env SMT_SHELL_CANVAS).
  std::string shell_canvas;
  bool ok = true;
  int exit_code = 0;
};

// CLI11 parse of argc/argv (wide). On --help / parse error: ok=false and
// exit_code set for wWinMain to return directly.
ViewsLaunchOptions parse_views_launch_options(int argc, wchar_t** argv);

const char* atmosphere_showcase_name(AtmosphereShowcaseMode mode);

}  // namespace app

#endif  // APP_VIEWS_SHELL_APP_CMDLINE_VIEWS_LAUNCH_OPTIONS_H_
