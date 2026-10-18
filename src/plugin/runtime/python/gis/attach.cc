// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/gis/attach.h"

#include "plugin/runtime/python/gis/marshal.h"

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyModuleDef kGisMod = {
    PyModuleDef_HEAD_INIT, "smartgis.gis",
    "Bindings mirroring src/gis (analysis + scene + style).", -1, nullptr};

}  // namespace

bool attach_gis_module(PyObject* smartgis_module) {
  if (!smartgis_module) {
    return false;
  }
  PyObject* gis = PyModule_Create(&kGisMod);
  PyObject* analysis = detail::make_analysis_module();
  PyObject* scene = detail::make_scene_module();
  PyObject* style = detail::make_style_module();
  if (!gis || !analysis || !scene || !style) {
    Py_XDECREF(gis);
    Py_XDECREF(analysis);
    Py_XDECREF(scene);
    Py_XDECREF(style);
    return false;
  }
  if (PyModule_AddObject(gis, "analysis", analysis) < 0) {
    Py_DECREF(analysis);
    Py_DECREF(scene);
    Py_DECREF(style);
    Py_DECREF(gis);
    return false;
  }
  if (PyModule_AddObject(gis, "scene", scene) < 0) {
    Py_DECREF(scene);
    Py_DECREF(style);
    Py_DECREF(gis);
    return false;
  }
  if (PyModule_AddObject(gis, "style", style) < 0) {
    Py_DECREF(style);
    Py_DECREF(gis);
    return false;
  }
  if (PyModule_AddObject(smartgis_module, "gis", gis) < 0) {
    Py_DECREF(gis);
    return false;
  }
  return true;
}

#endif
}  // namespace plugin
