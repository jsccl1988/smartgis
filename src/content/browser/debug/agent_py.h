// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DEBUG_AGENT_PY_H_
#define CONTENT_BROWSER_DEBUG_AGENT_PY_H_

#include <functional>
#include <string>

namespace content {
namespace detail {

using PyEvalFn = std::function<std::string(const std::string& code)>;

// Out-of-process python spawn fallback when host.py_eval is unset.
std::string run_python_code_oop(const std::string& code);

// Handles py.eval / py.run_file RPC.
bool dispatch_py_method(const std::string& method,
                        const std::string& params_json,
                        int id,
                        const PyEvalFn& eval,
                        std::string* response);

// Handles :py / :run / bare python one-liners (non-colon lines via caller).
bool exec_py_command(const std::string& line,
                     const PyEvalFn& eval,
                     std::string* output);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_DEBUG_AGENT_PY_H_
