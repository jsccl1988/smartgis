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
#include "content/browser/glang.runtime/exec/session/session.h"

namespace app {

class Browser;

// Browser-side face of content::run_script. Same ScriptCall as the runtime.
content::ScriptStatus run_script(Browser& browser, const content::ScriptCall& call);
int dispatch_plugin_command(Browser& browser,
                            const char* command_id,
                            std::string_view payload = {});

namespace detail {

void detach_views(Browser& browser);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_APP_STARTUP_SHELL_RUNTIME_H_
