// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/gis/marshal.h"

#include "content/browser/glang.runtime/host/bind/gis_console.h"

#include <cstdlib>
#include <string>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyObject* scene_layers(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().layers_json) {
    return detail::bridge_required("layers_json");
  }
  std::string json;
  std::string error;
  if (!detail::call_il("list_layers", nullptr, 0, &json, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "list_layers" : error.c_str());
    return nullptr;
  }
  return detail::loads_json(json);
}

PyObject* scene_select_layer(PyObject*, PyObject* args) {
  const char* id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &id)) {
    return nullptr;
  }
  if (!content::gis_console_bridge().select_layer) {
    return detail::bridge_required("select_layer");
  }
  const content::code::GisArg arg = detail::text_arg(id ? id : "");
  return detail::il_flag("select_layer", &arg, 1, true);
}

PyObject* scene_set_visible(PyObject*, PyObject* args) {
  const char* id = nullptr;
  int on = 1;
  if (!PyArg_ParseTuple(args, "sp", &id, &on)) {
    return nullptr;
  }
  if (!content::gis_console_bridge().set_layer_visible) {
    return detail::bridge_required("set_layer_visible");
  }
  content::code::GisArg call_args[2] = {detail::text_arg(id ? id : ""), detail::text_arg(on ? "1" : "0")};
  return detail::il_flag("set_layer_visible", call_args, 2, true);
}

PyObject* scene_extent(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().extent_json) {
    return detail::bridge_required("extent_json");
  }
  std::string json;
  std::string error;
  if (!detail::call_il("map_extent", nullptr, 0, &json, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "map_extent" : error.c_str());
    return nullptr;
  }
  if (json.empty() || json == "null") {
    Py_RETURN_NONE;
  }
  return detail::loads_json(json);
}

PyObject* scene_open(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  if (!content::gis_console_bridge().open_path) {
    return detail::bridge_required("open_path");
  }
  const content::code::GisArg arg = detail::text_arg(path ? path : "");
  return detail::il_flag("open_document", &arg, 1, true);
}

PyObject* scene_write(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  if (!content::gis_console_bridge().write_path && !content::gis_console_bridge().write_active_geojson) {
    return detail::bridge_required("write_path");
  }
  const content::code::GisArg arg = detail::text_arg(path ? path : "");
  return detail::il_flag("write_document", &arg, 1, false);
}

PyObject* scene_feature_count(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().feature_count) {
    return detail::bridge_required("feature_count");
  }
  std::string text;
  std::string error;
  if (!detail::call_il("feature_count", nullptr, 0, &text, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "feature_count" : error.c_str());
    return nullptr;
  }
  return PyLong_FromLong(std::strtol(text.c_str(), nullptr, 10));
}

PyObject* scene_present_mode(PyObject*, PyObject*) {
  if (!content::gis_console_bridge().present_mode) {
    return detail::bridge_required("present_mode");
  }
  std::string mode;
  std::string error;
  if (!detail::call_il("present_mode", nullptr, 0, &mode, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "present_mode" : error.c_str());
    return nullptr;
  }
  return PyUnicode_FromString(mode.c_str());
}

PyObject* scene_set_present_mode(PyObject*, PyObject* args) {
  const char* mode = nullptr;
  if (!PyArg_ParseTuple(args, "s", &mode)) {
    return nullptr;
  }
  if (!content::gis_console_bridge().set_present_mode) {
    return detail::bridge_required("set_present_mode");
  }
  const content::code::GisArg arg = detail::text_arg(mode ? mode : "");
  return detail::il_flag("set_present_mode", &arg, 1, false);
}

// Existing rows forward through detail::call_il (find_fn + registered eval).
// Do not add PyMethodDef entries.
PyMethodDef kSceneMethods[] = {
    {"layers", scene_layers, METH_NOARGS, "List GisScene layers."},
    {"select_layer", scene_select_layer, METH_VARARGS, "Select layer by id."},
    {"set_visible", scene_set_visible, METH_VARARGS, "Set layer visibility."},
    {"extent", scene_extent, METH_NOARGS, "Map extent dict or None."},
    {"open", scene_open, METH_VARARGS, "Open path into GisScene."},
    {"write", scene_write, METH_VARARGS, "Write active layer GeoJSON."},
    {"feature_count", scene_feature_count, METH_NOARGS, "Feature count."},
    {"present_mode", scene_present_mode, METH_NOARGS, "map2d|data|scene3d."},
    {"set_present_mode", scene_set_present_mode, METH_VARARGS,
     "Switch Map/Data/3D tab."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kSceneMod = {PyModuleDef_HEAD_INIT, "smartgis.gis.scene",
                         "GisScene 2D/3D document façade.", -1, kSceneMethods};

}  // namespace

namespace detail {

PyObject* make_scene_module() {
  return PyModule_Create(&kSceneMod);
}

}  // namespace detail
#endif
}  // namespace plugin
