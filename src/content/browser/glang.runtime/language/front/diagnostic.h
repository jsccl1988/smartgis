// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef IL_RUNTIME_CODEGEN_FRONT_DIAGNOSTIC_H_
#define IL_RUNTIME_CODEGEN_FRONT_DIAGNOSTIC_H_

#include <cstdint>
#include <string>

namespace content {
namespace py {

// 1-based source position. Column 1 is the first character on the line.
struct Loc {
  uint32_t line = 1;
  uint32_t column = 1;
};

// One frontend or later-stage diagnostic. The parser keeps the first only.
struct Diagnostic {
  Loc loc;
  std::string message;
};

}  // namespace py
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_FRONT_DIAGNOSTIC_H_
