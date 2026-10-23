// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Single verb table: builtin opcode slots, dynamic names, exec-extension
// fills, and language lower hooks. Callers keep the public register_* APIs.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_VM_VERB_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_VM_VERB_H_

#include "content/browser/glang.runtime/language/code/op.h"

#include <string>

namespace content {
namespace code {

// How invoke_inst stopped. kMissing means no descriptor or no eval function.
enum class InvokeKind { kRan, kMissing };

// Run one instruction. Extension fills are resolved here.
// On kMissing, `error` is left unchanged. On a failed eval, `error` is the
// verb's own text.
bool invoke_inst(content::CapabilityHost& host,
                 const Module& module,
                 const Inst& inst,
                 SlotTable& slots,
                 InvokeKind* kind,
                 std::string* error);

namespace detail {

// desc->eval when set, otherwise the exec extension registered for that op.
EvalFn resolve_eval(const OpDesc* desc);

}  // namespace detail
}  // namespace code
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_VM_VERB_H_
