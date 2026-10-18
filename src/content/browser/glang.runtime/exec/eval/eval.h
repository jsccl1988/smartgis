// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Run a Module by opcode descriptor. Does not include backend or Browser.

#ifndef IL_RUNTIME_CODEGEN_EVAL_EVAL_H_
#define IL_RUNTIME_CODEGEN_EVAL_EVAL_H_

#include "content/browser/glang.runtime/language/code/module.h"
#include "content/browser/glang.runtime/exec/session/script_call.h"
#include "content/browser/capability/host.h"

#include <string>
#include <unordered_map>

namespace content {
namespace code {

// ensure_ops_registered, then each descriptor eval. The first false stops.
// Earlier host effects stay. `error` includes the op name and source line.
bool eval_module(content::CapabilityHost& host,
                 const Module& module,
                 std::string* error);

// One live Python sg call. Builds a one-instruction module and evals it.
// Unknown names are a semantic error and do not skip the call.
// produces_slot results are written back into `vars`. When |result| is
// set, it receives that slot text (as_ or the implicit _ret slot).
bool eval_script_call(content::CapabilityHost& host,
                      const ScriptCall& call,
                      std::unordered_map<std::string, std::string>* vars,
                      std::string* error,
                      std::string* result = nullptr);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_EVAL_EVAL_H_
