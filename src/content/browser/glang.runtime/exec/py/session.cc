// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/session.h"

#include "content/browser/glang.runtime/host/horizon/atom/mark.h"
#include "content/browser/glang.runtime/host/link/bind_host.h"
#include "content/browser/glang.runtime/host/view/host/capture_host.h"
#include "content/browser/glang.runtime/exec/py/script_load.h"
#include "content/browser/glang.runtime/host/utils/charset.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/vm/eval.h"
#include "content/browser/glang.runtime/exec/py/cpython.h"
#include "content/browser/glang.runtime/exec/py/script_call.h"
#include "content/public/script_runtime.h"

#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <memory>
#include <string>
#include <unordered_map>

namespace content {
namespace {

// Keeps DrawHosts alive while a script is on the stack. main() returning
// must not detach those views.
class LiveViewGuard {
 public:
  LiveViewGuard() { detail::set_keep_live_views(true); }
  ~LiveViewGuard() { detail::set_keep_live_views(false); }
  LiveViewGuard(const LiveViewGuard&) = delete;
  LiveViewGuard& operator=(const LiveViewGuard&) = delete;
};

bool ends_with_py(const std::wstring& path) {
  return path.size() >= 3 &&
         _wcsicmp(path.c_str() + path.size() - 3, L".py") == 0;
}

bool run_py(content::CapabilityHost& host,
            const std::wstring& path,
            std::string* error) {
  if (!ends_with_py(path)) {
    if (error) {
      *error = "error: script must be a .py file";
    }
    LOGGING(LOG_ERROR, "script: path is not a .py file");
    return false;
  }
  LiveViewGuard keep_views;
  return content::run_suite_script(host, detail::wide_to_utf8(path.c_str()),
                                   error);
}

bool os_driver() {
  const char* driver = base::switch_cstr("ui-interact-driver");
  return driver && std::strcmp(driver, "os") == 0;
}

int os_wait_ms() {
  if (const char* value = base::switch_cstr("ui-interact-os-wait-ms")) {
    const int n = std::atoi(value);
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

// Gesture body on an already-linked host. OS driver waits for an outer
// injector; otherwise the harness gesture .py runs. False lets the previous
// apply_scenario_panels handler run.
bool apply_interact(content::Shell& browser, content::CapabilityHost& host) {
  if (os_driver()) {
    host_mark(host, "os-driver");
    // Settle the Map tab before the outer injector. The OS script skips
    // in-process tab/catalog/inspector steps, but BMP gates still need a
    // painted tab accent.
    if (host.horizon.select_view_tab) {
      host.horizon.select_view_tab(0);
    }
    if (host.horizon.pump) {
      host.horizon.pump(400);
      const int wait_ms = os_wait_ms();
      LOGGING(LOG_INFO, "interact-script: OS driver wait %d ms", wait_ms);
      host.horizon.pump(wait_ms);
    }
    host_mark(host, "os-wait-done");
    return true;
  }
  std::wstring path;
  if (!detail::locate_suite("ui.interact", "ui-interact-gesture-script", &path)) {
    return false;
  }
  const std::string utf8 = detail::wide_to_utf8(path.c_str());
  LOGGING(LOG_INFO, "interact-gesture: %s", utf8.c_str());
  std::string error;
  if (!run_py(host, path, &error)) {
    LOGGING(LOG_ERROR, "interact-gesture: %s", error.c_str());
    return false;
  }
  return true;
}

void install_interact(content::Shell& browser, content::CapabilityHost* host) {
  if (!host) {
    return;
  }
  auto inner = host->horizon.apply_scenario_panels;
  content::CapabilityHost* linked = host;
  content::Shell* live = &browser;
  host->horizon.apply_scenario_panels =
      [live, linked, inner](const std::string& mode) {
        if (mode == "interact" && apply_interact(*live, *linked)) {
          if (linked->horizon.select_view_tab) {
            linked->horizon.select_view_tab(0);
          }
          return true;
        }
        return inner ? inner(mode) : false;
      };
}

// CapabilityHost is a large std::function bag. Keep it off the stack so a
// deep frame cannot smash RTC cookies or the return address.
std::unique_ptr<content::CapabilityHost> link_host(content::Shell& browser,
                                                   const wchar_t* mark_leaf,
                                                   bool clear_marks,
                                                   bool with_interact) {
  auto host = std::make_unique<content::CapabilityHost>();
  bind_host(browser, host.get(), mark_leaf);
  if (with_interact) {
    install_interact(browser, host.get());
  }
  if (clear_marks && host->horizon.clear_marks) {
    host->horizon.clear_marks();
  }
  return host;
}

int run_linked(content::Shell& browser,
               const std::wstring& path,
               const wchar_t* mark_leaf,
               bool clear_marks) {
  // Suite files go through the embedder, which refuses a bare
  // interpreter().init() (that skips the plugin inittab). eval_python
  // is the shell path that starts PythonRuntime with those hooks.
  if (!content::py::interpreter().is_ready()) {
    const std::string boot = browser.eval_python("None");
    if (!content::py::interpreter().is_ready()) {
      LOGGING(LOG_ERROR, "run_script: %s",
              boot.empty() ? "error: python not ready" : boot.c_str());
      return 1;
    }
  }
  auto host = link_host(browser, mark_leaf, clear_marks, true);
  std::string error;
  if (!run_py(*host, path, &error)) {
    if (error.empty()) {
      error = "script must be .py";
    }
    LOGGING(LOG_ERROR, "run_script: %s", error.c_str());
    return host->fail_rc != 0 ? host->fail_rc : 1;
  }
  return 0;
}

bool resolve_suite_logged(const char* suite_id, std::wstring* path) {
  if (!detail::locate_suite(suite_id, "ui-interact-script", path)) {
    LOGGING(LOG_ERROR, "run_script: resolve failed suite=%s",
            suite_id ? suite_id : "");
    return false;
  }
  const std::string utf8 = detail::wide_to_utf8(path->c_str());
  LOGGING(LOG_INFO, "run_script: resolved %s", utf8.c_str());
  return true;
}

ScriptStatus run_suite(content::Shell& browser, const ScriptCall& call) {
  std::wstring path;
  if (!resolve_suite_logged(call.suite_id, &path)) {
    return {};
  }
  return ScriptStatus{run_linked(browser, path, call.mark_leaf, call.clear_marks),
                      {}};
}

ScriptStatus run_path(content::Shell& browser, const ScriptCall& call) {
  const std::wstring path = detail::utf8_to_wide(call.path_utf8);
  if (!detail::wide_path_exists(path)) {
    return ScriptStatus{1, std::string("error: missing script ") + call.path_utf8};
  }
  if (run_linked(browser, path, call.mark_leaf, call.clear_marks) != 0) {
    return ScriptStatus{1, std::string("error: script failed ") + call.path_utf8};
  }
  return ScriptStatus{0, std::string("ok: ") + call.path_utf8};
}

// Binds smartgis.script calls to eval_script_call for one embedded eval.
class HostScriptHandler final : public code::ScriptHandler {
 public:
  explicit HostScriptHandler(content::CapabilityHost& host) : host_(host) {}

  bool call(const code::ScriptCall& call,
            std::string* error,
            std::string* result) override {
    return code::eval_script_call(host_, call, &vars_, error, result);
  }

 private:
  content::CapabilityHost& host_;
  std::unordered_map<std::string, std::string> vars_;
};

class ScriptBind {
 public:
  explicit ScriptBind(content::CapabilityHost& host) : handler_(host) {
    code::set_script_handler(&handler_);
  }
  ~ScriptBind() { code::clear_script_handler(); }
  ScriptBind(const ScriptBind&) = delete;
  ScriptBind& operator=(const ScriptBind&) = delete;

 private:
  HostScriptHandler handler_;
};

ScriptStatus run_eval(content::Shell& browser, const ScriptCall& call) {
  if (!browser.has_plugins()) {
    return ScriptStatus{1, "error: no plugin shell"};
  }
  auto host = link_host(browser, detail::kUiMarkLeaf, false, false);
  LiveViewGuard keep_views;
  ScriptBind script(*host);
  return ScriptStatus{0, browser.eval_python(call.code ? call.code : "")};
}

}  // namespace

ScriptStatus run_script(content::Shell& browser, const ScriptCall& call) {
  if (call.code) {
    return run_eval(browser, call);
  }
  if (call.path_utf8) {
    return run_path(browser, call);
  }
  if (call.suite_id) {
    return run_suite(browser, call);
  }
  return ScriptStatus{1, "error: empty script call"};
}

}  // namespace content
