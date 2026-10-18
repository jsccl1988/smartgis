// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_PYTHON_SCRIPT_ATTACH_H_
#define PLUGIN_RUNTIME_PYTHON_SCRIPT_ATTACH_H_

namespace plugin {

// |smartgis_module| is a PyObject* when CPython is compiled in.
// Verbs dispatch through content::code::ScriptHandler.
void attach_script_module(void* smartgis_module);

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_SCRIPT_ATTACH_H_
