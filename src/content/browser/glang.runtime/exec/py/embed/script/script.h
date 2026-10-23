// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_PYTHON_SCRIPT_SCRIPT_H_
#define PLUGIN_RUNTIME_PYTHON_SCRIPT_SCRIPT_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)
void attach_script_module(PyObject* smartgis_module);
#else
void attach_script_module(void* smartgis_module);
#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_SCRIPT_SCRIPT_H_
