// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/plugin_fn.h"

#include "base/core/log.h"
#include "content/browser/glang.runtime/language/sema/check.h"

namespace content {
namespace py {

bool call_registered(const code::ScriptCall& call,
                     std::string* error,
                     std::string* result) {
  // rclick / dblclick / key_down / key_up / path are sema sugar, not Op names.
  // eval_script_call applies the rewrite via lookup_method.
  if (!find_fn(call.name) && lookup_method(call.name) == nullptr) {
    const std::string text = "unknown verb '" + call.name + "'";
    if (error) {
      *error = text;
    }
    LOGGING(LOG_ERROR, "plugin_fn: %s", text.c_str());
    return false;
  }
  code::ScriptHandler* handler = code::script_handler();
  if (!handler) {
    if (error) {
      *error = "script host not bound";
    }
    return false;
  }
  return handler->call(call, error, result);
}

}  // namespace py
}  // namespace content
