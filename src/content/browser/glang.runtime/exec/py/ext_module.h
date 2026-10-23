// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// glang.ext.<plugin_id> — third-party extension surface owned by glang.runtime.
// plugin_id matches plugin.json `id` (dotted segments → nested packages).
// The header does not include Python.h.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_EXT_MODULE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_EXT_MODULE_H_

#include "content/browser/glang.runtime/language/code/op.h"

#include <string_view>

namespace content {
namespace py {

// Creates the empty parent package `glang.ext` after the interpreter starts.
void install_glang_ext();

// Mounts `glang.ext.<plugin_id>` for a plugin directory.
// Prefers |root_dir|/ext/ (package) or |root_dir|/ext.py; otherwise an empty
// leaf that can receive register_extension_fn. Returns false on bad id,
// missing interpreter, or load failure.
bool mount_extension(std::string_view plugin_id, std::string_view root_dir);

// Removes the leaf (and unused parents) from sys.modules. Safe if not mounted.
void unmount_extension(std::string_view plugin_id);

// Registers a C++ verb on the mounted extension leaf (same EvalFn shape as
// glang.plugin). No-op / false when the leaf is missing.
bool register_extension_fn(std::string_view plugin_id,
                           std::string_view name,
                           const code::Param* params,
                           int param_count,
                           code::EvalFn eval);

void unregister_extension_fn(std::string_view plugin_id, std::string_view name);

}  // namespace py
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_EXT_MODULE_H_
