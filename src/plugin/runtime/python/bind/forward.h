// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Capability-table notes for the smartgis extension surface.
//
// glang.plugin verbs (mark, click, list_layers, export_bmp, ...) resolve
// through content::py::find_fn and run the registered eval. That path lives in
// gis/marshal.cc (detail::call_il) and script/attach.cc (call_registered).
//
// Host, events, tool, ui, and debug are PluginHost / Views surfaces. Their
// names are not capability verbs, and call_registered needs a suite
// ScriptHandler that plugin start does not install. Those PyMethodDef tables
// stay thin local wrappers. Do not add rows. Callables are integer ids;
// glang.plugin owns the PyObject.

#ifndef PLUGIN_RUNTIME_PYTHON_BIND_FORWARD_H_
#define PLUGIN_RUNTIME_PYTHON_BIND_FORWARD_H_

#include <string_view>

namespace plugin {
namespace detail {

// Remember a verb this plugin directory registered. stop unregisters it
// before callbacks are dropped. Builtins copied by find_fn are not tracked.
void track_registered_verb(std::string_view name);

void unregister_tracked_verbs(std::string_view directory);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_BIND_FORWARD_H_
