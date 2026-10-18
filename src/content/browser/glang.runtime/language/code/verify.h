// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// CRS check for kBuffer. Geometry eval lives in op/gis/eval.

#ifndef IL_RUNTIME_CODEGEN_CODE_VERIFY_H_
#define IL_RUNTIME_CODEGEN_CODE_VERIFY_H_

#include <string>

#include "content/browser/glang.runtime/language/code/module.h"

namespace content {
namespace code {

// kBuffer operands: geometry WKT (a) and distance text (b). Both carry
// crs_epsg. Reject when either code is 0 or the codes differ. Eval lives in
// op/gis/eval.
// Returns false and appends a message when a buffer pair is rejected.
bool verify_module(const Module& module, std::string* error);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_CODE_VERIFY_H_
