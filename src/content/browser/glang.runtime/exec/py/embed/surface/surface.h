// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Submodule factories. assemble.cc is the only runner.

#ifndef PLUGIN_RUNTIME_PYTHON_SURFACE_SURFACE_H_
#define PLUGIN_RUNTIME_PYTHON_SURFACE_SURFACE_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

#include "content/public/plugin_host.h"

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)

bool init_host_type();
PyObject* make_host_object(content::PluginHost* host);

PyObject* make_events_module();
PyObject* make_ui_module();
bool init_debug_type();
PyObject* make_debug_module();

#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_SURFACE_SURFACE_H_
