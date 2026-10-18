// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Execution extension and the one-instruction GIS entry the Python façade uses.
// Builtin descriptors keep their eval. Extensions run only when that pointer
// is null.

#ifndef IL_RUNTIME_CODEGEN_EXTEND_EXEC_H_
#define IL_RUNTIME_CODEGEN_EXTEND_EXEC_H_

#include "content/browser/glang.runtime/language/code/op.h"
#include "content/browser/glang.runtime/exec/extend/language.h"
#include "content/browser/capability/host.h"

#include <string>
#include <string_view>

namespace content {
namespace code {

// Plugin-owned eval for one Op. The IL table still owns the descriptor.
struct ExecExtension {
  Op op = Op::kMark;
  EvalFn eval = nullptr;
};

void register_exec_extension(const ExecExtension& ext);

// Null when no extension registered that op.
EvalFn find_exec_extension(Op op);

// One operand in Param order. Text is copied into the module string pool.
// crs_epsg is copied onto the operand (0 = unknown).
struct GisArg {
  std::string text;
  ArgKind kind = ArgKind::kStr;
  int32_t i32 = 0;
  int32_t crs_epsg = 0;
};

// Build a one-instruction module and eval it. `result` receives slot 0 when
// the op writes it. Host may be empty for pure Geo ops.
bool eval_gis_call(content::CapabilityHost& host,
                   std::string_view op_name,
                   const GisArg* args,
                   int arg_count,
                   std::string* result,
                   std::string* error);

// Lower a Geo / Relation / Map name into `extra`. Returns false and leaves
// `error` empty when `call.name` is not one of those ops.
bool lower_gis_call(const CallLowering& call, Module* extra, std::string* error);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_EXTEND_EXEC_H_
