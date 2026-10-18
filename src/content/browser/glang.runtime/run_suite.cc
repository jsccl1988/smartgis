// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/script_runtime.h"

#include "base/core/log.h"
#include "content/browser/capability/host.h"
#include "content/browser/glang.runtime/exec/eval/eval.h"
#include "content/browser/glang.runtime/exec/session/cpython.h"
#include "content/browser/glang.runtime/exec/session/script_call.h"

#include <string>
#include <string_view>
#include <unordered_map>

namespace content {
namespace {

bool ends_with_py(std::string_view path) {
  if (path.size() < 3) {
    return false;
  }
  const std::string_view tail = path.substr(path.size() - 3);
  return tail == ".py" || tail == ".PY" || tail == ".Py" || tail == ".pY";
}

bool fail(std::string text, std::string* error) {
  if (error) {
    *error = text;
  }
  LOGGING(LOG_ERROR, "script: %s", text.c_str());
  return false;
}

// Live verbs for one suite file. Earlier statements keep their effects when
// a later name throws inside main().
class SuiteScriptHandler final : public content::code::ScriptHandler {
 public:
  explicit SuiteScriptHandler(CapabilityHost& host) : host_(host) {}

  bool call(const content::code::ScriptCall& call,
            std::string* error,
            std::string* result) override {
    return content::code::eval_script_call(host_, call, &vars_, error, result);
  }

 private:
  CapabilityHost& host_;
  std::unordered_map<std::string, std::string> vars_;
};

}  // namespace

bool run_suite_script(CapabilityHost& host,
                      std::string_view path_utf8,
                      std::string* error) {
  if (!ends_with_py(path_utf8)) {
    return fail("error: script must be a .py file", error);
  }
  // Do not start the interpreter here. A bare init() skips the smartgis
  // inittab hook owned by the plugin runtime, and import smartgis then fails.
  if (!content::py::interpreter().is_ready()) {
    return fail("error: python not ready", error);
  }
  SuiteScriptHandler handler(host);
  content::code::set_script_handler(&handler);
  struct ClearHandler {
    ~ClearHandler() { content::code::clear_script_handler(); }
  } clear_handler;

  std::string py_error;
  if (!content::py::interpreter().exec_file(path_utf8, &py_error)) {
    return fail(py_error.empty() ? "error: script failed" : py_error, error);
  }
  return true;
}

}  // namespace content
