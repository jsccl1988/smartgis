// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/gis/marshal.h"

#include "content/browser/glang.runtime/host/bind/gis_console.h"

#include <string>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyObject* style_has_document(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().has_style_document) {
    return detail::bridge_required("has_style_document");
  }
  std::string flag;
  std::string error;
  if (!detail::call_il("style_has_document", nullptr, 0, &flag, &error)) {
    PyErr_SetString(PyExc_RuntimeError,
                    error.empty() ? "style_has_document" : error.c_str());
    return nullptr;
  }
  if (flag == "1") {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

PyObject* style_load(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  if (!content::gis_console_bridge().load_style_path) {
    return detail::bridge_required("load_style_path");
  }
  const content::code::GisArg arg = detail::text_arg(path ? path : "");
  return detail::il_flag("apply_style_file", &arg, 1, true);
}

PyObject* style_clear(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().clear_style) {
    return detail::bridge_required("clear_style");
  }
  std::string ignored;
  std::string error;
  if (!detail::call_il("style_clear", nullptr, 0, &ignored, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "style_clear" : error.c_str());
    return nullptr;
  }
  if (content::gis_console_bridge().refresh_gis) {
    content::gis_console_bridge().refresh_gis();
  }
  Py_RETURN_NONE;
}

PyObject* style_summary(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().style_summary_json) {
    return detail::bridge_required("style_summary_json");
  }
  std::string json;
  std::string error;
  if (!detail::call_il("style_summary", nullptr, 0, &json, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "style_summary" : error.c_str());
    return nullptr;
  }
  if (json.empty() || json == "null") {
    Py_RETURN_NONE;
  }
  return detail::loads_json(json);
}

// Existing rows forward through detail::call_il (find_fn + registered eval).
// Do not add PyMethodDef entries.
PyMethodDef kStyleMethods[] = {
    {"has_document", style_has_document, METH_NOARGS, nullptr},
    {"load", style_load, METH_VARARGS, "Load StyleDocument JSON path."},
    {"clear", style_clear, METH_NOARGS, nullptr},
    {"summary", style_summary, METH_NOARGS, "Style name/version/layer ids."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kStyleMod = {PyModuleDef_HEAD_INIT, "smartgis.gis.style",
                         "Cartographic StyleDocument on GisScene.", -1,
                         kStyleMethods};

}  // namespace

namespace detail {

PyObject* make_style_module() {
  return PyModule_Create(&kStyleMod);
}

}  // namespace detail
#endif
}  // namespace plugin
