// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Suite subset check. Binds sg.verb calls to opcodes. Does not call the host.

#ifndef IL_RUNTIME_CODEGEN_SEMA_CHECK_H_
#define IL_RUNTIME_CODEGEN_SEMA_CHECK_H_

#include "content/browser/glang.runtime/language/code/op.h"
#include "content/browser/glang.runtime/language/front/ast.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace content {
namespace py {

// Operand.aux value while a string is still source text. lower_program
// interns it and clears the sentinel. Never stored in a finished Module.
inline constexpr std::uint16_t kRawText = 0xFFFF;

// One bound sg call. String operands with aux == kRawText index
// BoundProgram::raw_strings. as_ is result_slot, not an operand.
struct BoundCall {
  content::code::Op op = content::code::Op::kMark;
  Loc loc;
  std::vector<content::code::Operand> operands;
  int result_slot = -1;
  // Slots visible to this call. The program list is BoundProgram::slot_names.
  std::vector<std::string> slot_names_snapshot;
};

// Semantic result. raw_strings is the pre-intern pool (Operand has no text).
struct BoundProgram {
  std::vector<std::string> slot_names;
  std::vector<std::string> raw_strings;
  std::vector<BoundCall> calls;
};

// Sugar and aliases. Null when the verb is unknown.
const content::code::OpDesc* lookup_method(std::string_view method);

// Bind one call. program->slot_names already holds earlier slots.
// False on error; earlier calls in `program` stay.
// |implicit_slot| lets a producing verb omit as_ and still write a slot
// the caller can read back. Suite checking leaves this false.
bool bind_method(const Expr& call,
                 std::string_view method,
                 Loc loc,
                 BoundProgram* program,
                 std::vector<Diagnostic>* diags,
                 bool implicit_slot = false);

// Collects up to 20 errors. False if any error. Does not touch CapabilityHost.
bool check_suite(const File& file,
                 BoundProgram* out,
                 std::vector<Diagnostic>* diags);

}  // namespace py
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_SEMA_CHECK_H_
