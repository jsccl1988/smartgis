// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PYTHON_GIS_BINDINGS_H_
#define PLUGIN_PYTHON_GIS_BINDINGS_H_

#if defined(SMT_HAS_PYTHON)
struct _object;
typedef struct _object PyObject;
namespace plugin {
// Attach smartgis.gis (+ analysis) under an existing smartgis module.
bool attach_gis_module(PyObject* smartgis_module);
}  // namespace plugin
#endif

#endif  // PLUGIN_PYTHON_GIS_BINDINGS_H_
