// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_RUNTIME_PYTHON_GIS_ATTACH_H_
#define PLUGIN_RUNTIME_PYTHON_GIS_ATTACH_H_

#include "plugin/runtime/python/bind/py_compat.h"

namespace plugin {

#if defined(HAS_PYTHON)
// Attach smartgis.gis (analysis, scene, style) under an existing module.
bool attach_gis_module(PyObject* smartgis_module);
#endif

}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_GIS_ATTACH_H_
