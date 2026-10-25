// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Shell registration for the IL runtime. Fills CapabilityHost from Browser.
// The runtime implementation lives in content/browser/glang.runtime.

#ifndef APP_VIEWS_APP_STARTUP_SHELL_RUNTIME_H_
#define APP_VIEWS_APP_STARTUP_SHELL_RUNTIME_H_

#include <string>
#include <string_view>

#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/horizon/atom/pump.h"
#include "content/browser/glang.runtime/exec/py/session.h"

namespace app {

class Browser;

// Browser-side face of content::run_script. Same ScriptCall as the runtime.
content::ScriptStatus run_script(Browser& browser, const content::ScriptCall& call);
int dispatch_plugin_command(Browser& browser,
                            const char* command_id,
                            std::string_view payload = {});

// Debug CPython stub for HARNESS_SUITE=ui.* (theme → panels → BMP).
int run_ui_shell_showcase(Browser& browser);
int run_ui_catalog_showcase(Browser& browser);
int run_ui_data_showcase(Browser& browser);

namespace detail {

void detach_views(Browser& browser);

// Debug CPython stub path: seed → export_bmp → fps_bench without suite .py.
int run_map2d_showcase_cpp(Browser& browser,
                           const char* mode,
                           const char* seed_mark,
                           const wchar_t* bmp_leaf,
                           const char* frame);

// Debug CPython stub for atmosphere / Scene3d suites: apply look → export_bmp
// (timed present JSON when PRESENT_COUNT is set) → harness marks.
int run_atmosphere_showcase_cpp(Browser& browser,
                                const char* look_mode,
                                const wchar_t* bmp_leaf);

// Debug CPython stub for ui.shell: theme → panels → layout gate → present BMP.
int run_ui_shell_showcase_cpp(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_SHELL_RUNTIME_H_
