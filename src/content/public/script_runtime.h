// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Suite script entry. One function: parse, sema, lower, eval.
// The shell binds CapabilityHost first. This header does not include Browser.

#ifndef CONTENT_PUBLIC_SCRIPT_RUNTIME_H_
#define CONTENT_PUBLIC_SCRIPT_RUNTIME_H_

#include <string>
#include <string_view>

#include "content/browser/capability/host.h"

namespace content {

// |path_utf8| is a filesystem path to a suite .py. The first failure is
// written to |error| when non-null. Host effects from earlier instructions
// stay. Does not start embedded CPython.
bool run_suite_script(CapabilityHost& host,
                      std::string_view path_utf8,
                      std::string* error);

}  // namespace content

#endif  // CONTENT_PUBLIC_SCRIPT_RUNTIME_H_
