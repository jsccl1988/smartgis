// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_LOWER_CODE_H_
#define IL_RUNTIME_CODEGEN_LOWER_CODE_H_

#include <string>
#include <vector>

#include "app/views/il.runtime/codegen/lower/ops.h"

namespace app {
namespace detail {

// Lowered Interact program. Eval runs this; it has no CallStmt / AST.
struct Code {
  enum class Kind { kNop, kRun, kSeq, kRepeat, kChord };

  Kind kind = Kind::kNop;
  std::string name;
  Action run;
  int repeat_count = 1;
  std::vector<std::string> chord_mods;
  std::vector<Code> body;

  static Code nop() { return Code{}; }

  static Code run_named(std::string name, Action action) {
    Code c;
    c.kind = Kind::kRun;
    c.name = std::move(name);
    c.run = std::move(action);
    return c;
  }
};

}  // namespace detail
}  // namespace app

#endif  // IL_RUNTIME_CODEGEN_LOWER_CODE_H_
