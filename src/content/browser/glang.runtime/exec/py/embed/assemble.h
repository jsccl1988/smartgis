// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Attaches Host / content / ui / debug / gis / script onto glang.plugin.

#ifndef PLUGIN_RUNTIME_PYTHON_ASSEMBLE_H_
#define PLUGIN_RUNTIME_PYTHON_ASSEMBLE_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

#include "content/public/plugin_host.h"

#include <string_view>

namespace plugin {

// Adds the embed types onto the already-installed glang.plugin module and
// aliases that module as smartgis. No-op when Python is compiled out.
bool attach_plugin_embed();

#if defined(HAS_PYTHON)
PyObject* make_python_host(content::PluginHost* host);
void bind_python_host(content::PluginHost* host);
void unbind_python_host();
void set_python_load_directory(std::string_view directory);
void drop_python_plugin_holds(std::string_view directory);
#endif

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_ASSEMBLE_H_
