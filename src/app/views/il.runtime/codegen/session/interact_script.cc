// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/codegen/session/interact_script.h"

#include "app/views/il.runtime/backend/horizon/atom/mark.h"
#include "app/views/il.runtime/backend/link/bind_host.h"
#include "app/views/il.runtime/codegen/driver.h"
#include "app/views/il.runtime/frontend/load.h"
#include "app/views/browser/browser.h"
#include "base/process/switches.h"
#include "content/browser/capability/host.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace app {
namespace {

bool env_is_os_driver() {
  const char* d = base::switch_cstr("ui-interact-driver");
  return d && std::strcmp(d, "os") == 0;
}

int env_os_wait_ms() {
  if (const char* v = base::switch_cstr("ui-interact-os-wait-ms")) {
    const int n = std::atoi(v);
    if (n > 0) {
      return n;
    }
  }
  return 6000;
}

void host_mark(content::CapabilityHost& host, const char* token) {
  if (host.horizon.mark && token) {
    host.horizon.mark(token);
  }
}

// Apply on an already-linked Host. Does not call bind_host.
bool apply_interact_on_host(content::CapabilityHost& host) {
  if (env_is_os_driver()) {
    host_mark(host, "os-driver");
    // Settle Map tab horizon before outer injector runs — OS script skips
    // select_view_tab / catalog / inspector (inproc-only), but BMP gates still
    // need a painted tab accent.
    if (host.horizon.select_view_tab) {
      host.horizon.select_view_tab(0);
    }
    if (host.horizon.pump) {
      host.horizon.pump(400);
      const int wait_ms = env_os_wait_ms();
      std::fprintf(stderr, "interact-script: OS driver wait %d ms\n", wait_ms);
      host.horizon.pump(wait_ms);
    }
    host_mark(host, "os-wait-done");
    return true;
  }
  std::wstring path;
  // Gesture body only — never UI_INTERACT_SCRIPT (that is the host entry).
  if (!resolve_interact_gesture_script(&path)) {
    return false;
  }
  std::fwprintf(stderr, L"interact-gesture: %ls\n", path.c_str());
  switch (script_kind(path)) {
    case ScriptKind::kIl:
      return apply_script(host, path);
    case ScriptKind::kPython:
    case ScriptKind::kUnknown:
      break;
  }
  return false;
}

}  // namespace

bool interact_script_os_driver() {
  return env_is_os_driver();
}

void install_interact_frontend(Browser& browser, content::CapabilityHost* host) {
  (void)browser;
  if (!host) {
    return;
  }
  auto inner = host->horizon.apply_scenario_panels;
  content::CapabilityHost* linked = host;
  host->horizon.apply_scenario_panels =
      [linked, inner](const std::string& mode) {
        if (mode == "interact" && apply_interact_on_host(*linked)) {
          if (linked->horizon.select_view_tab) {
            linked->horizon.select_view_tab(0);
          }
          return true;
        }
        return inner ? inner(mode) : false;
      };
}

bool try_apply_interact_script(Browser& browser) {
  content::CapabilityHost host;
  bind_host(browser, &host, detail::kUiMarkLeaf);
  return apply_interact_on_host(host);
}

}  // namespace app
