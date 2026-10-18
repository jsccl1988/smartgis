// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Pack a bound suite into a Module. Interns strings and splits $slot.
// Does not call the host and does not include backend headers.

#ifndef IL_RUNTIME_CODEGEN_LOWER_LOWER_H_
#define IL_RUNTIME_CODEGEN_LOWER_LOWER_H_

#include "content/browser/glang.runtime/language/code/module.h"
#include "content/browser/glang.runtime/language/front/diagnostic.h"
#include "content/browser/glang.runtime/language/sema/check.h"

#include <vector>

namespace content {
namespace code {

// On success `diags` is empty. A failure here is a compiler defect.
bool lower_program(const content::py::BoundProgram& in,
                   Module* out,
                   std::vector<content::py::Diagnostic>* diags);

}  // namespace code
}  // namespace content

#endif  // IL_RUNTIME_CODEGEN_LOWER_LOWER_H_
