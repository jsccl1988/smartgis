// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/gis/marshal.h"

#include "content/browser/glang.runtime/host/bind/console.h"

#include <string>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

const content::CapabilityHost& bridge() {
  return content::console_host();
}

PyObject* style_has_document(PyObject*, PyObject*) {
  if (!bridge().document.has_style_document) {
    return detail::bridge_required("has_style_document");
  }
  std::string flag;
  if (!detail::il_out("style_has_document", &flag)) {
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
  return detail::il_flag_text(
      "load_style_path", static_cast<bool>(bridge().document.apply_style_file),
      "apply_style_file", path, true);
}

PyObject* style_clear(PyObject*, PyObject*) {
  if (!bridge().document.clear_style) {
    return detail::bridge_required("clear_style");
  }
  std::string ignored;
  if (!detail::il_out("style_clear", &ignored)) {
    return nullptr;
  }
  if (bridge().document.refresh_gis) {
    bridge().document.refresh_gis();
  }
  Py_RETURN_NONE;
}

PyObject* style_summary(PyObject*, PyObject*) {
  if (!bridge().document.style_summary_json) {
    return detail::bridge_required("style_summary_json");
  }
  return detail::il_json_or_none("style_summary");
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
