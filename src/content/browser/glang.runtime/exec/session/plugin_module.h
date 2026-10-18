// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Installs the glang.plugin module after the interpreter starts.
// The header does not include Python.h.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_PLUGIN_MODULE_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_PLUGIN_MODULE_H_

namespace content {
namespace py {

void install_glang_plugin();

}  // namespace py
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_PLUGIN_MODULE_H_
