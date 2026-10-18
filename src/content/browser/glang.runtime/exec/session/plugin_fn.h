// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Name-keyed capability table. Debug tests include this header; it does not
// include Python.h. Opcode descriptors are copied in by name and alias.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_PLUGIN_FN_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_PLUGIN_FN_H_

#include "content/browser/glang.runtime/language/code/op.h"
#include "content/browser/glang.runtime/exec/session/script_call.h"

#include <optional>
#include <string>
#include <string_view>

namespace content {
namespace py {

// One registered verb. params point at the opcode table (stable literals).
struct RegisteredFn {
  std::string name;
  const code::Param* params = nullptr;
  int param_count = 0;
  code::EvalFn eval = nullptr;
};

// Insert or replace |name|. |eval| may be null only when unregistering via
// unregister_fn. Returns false when |name| is empty.
bool register_fn(std::string_view name,
                 const code::Param* params,
                 int param_count,
                 code::EvalFn eval);

void unregister_fn(std::string_view name);

// Nullopt when the name was never registered. The first lookup also copies
// the opcode table (names and aliases) so existing verbs resolve.
std::optional<RegisteredFn> find_fn(std::string_view name);

// Resolve |call.name| on the table, then run the bound script handler.
// Unknown names fail before the handler. as_ stays a named argument.
bool call_registered(const code::ScriptCall& call,
                     std::string* error,
                     std::string* result = nullptr);

}  // namespace py
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_PLUGIN_FN_H_
