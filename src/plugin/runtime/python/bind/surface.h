// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Submodule factories. module.cc is the only assembler.

#ifndef PLUGIN_RUNTIME_PYTHON_BIND_SURFACE_H_
#define PLUGIN_RUNTIME_PYTHON_BIND_SURFACE_H_

#include "plugin/runtime/python/bind/py_compat.h"

#include "content/public/plugin_host.h"

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)

bool init_host_type();
PyObject* make_host_object(content::PluginHost* host);

PyObject* make_tool_module();
PyObject* make_events_module();
PyObject* make_ui_module();
bool init_debug_type();
PyObject* make_debug_module();

#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_BIND_SURFACE_H_
