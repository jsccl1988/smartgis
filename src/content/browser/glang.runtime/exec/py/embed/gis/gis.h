// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_PYTHON_GIS_GIS_H_
#define PLUGIN_RUNTIME_PYTHON_GIS_GIS_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)
PyObject* make_analysis_module();
PyObject* make_scene_module();
PyObject* make_style_module();
#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_GIS_GIS_H_
