// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/gis/marshal.h"

#include "content/browser/glang.runtime/host/bind/console.h"

#include <cstdlib>
#include <string>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

const content::CapabilityHost& bridge() {
  return content::console_host();
}

PyObject* scene_layers(PyObject*, PyObject*) {
  if (!bridge().document.layers_json) {
    return detail::bridge_required("layers_json");
  }
  std::string json;
  if (!detail::il_out("list_layers", &json)) {
    return nullptr;
  }
  return detail::loads_json(json);
}

PyObject* scene_select_layer(PyObject*, PyObject* args) {
  const char* id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &id)) {
    return nullptr;
  }
  return detail::il_flag_text(
      "select_layer", static_cast<bool>(bridge().document.select_layer),
      "select_layer", id, true);
}

PyObject* scene_set_visible(PyObject*, PyObject* args) {
  const char* id = nullptr;
  int on = 1;
  if (!PyArg_ParseTuple(args, "sp", &id, &on)) {
    return nullptr;
  }
  if (!bridge().document.set_layer_visible) {
    return detail::bridge_required("set_layer_visible");
  }
  content::code::GisArg call_args[2] = {detail::text_arg(id ? id : ""),
                                        detail::text_arg(on ? "1" : "0")};
  return detail::il_flag("set_layer_visible", call_args, 2, true);
}

PyObject* scene_extent(PyObject*, PyObject*) {
  if (!bridge().document.extent_json) {
    return detail::bridge_required("extent_json");
  }
  return detail::il_json_or_none("map_extent");
}

PyObject* scene_open(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  return detail::il_flag_text(
      "open_path", static_cast<bool>(bridge().document.open_document),
      "open_document", path, true);
}

PyObject* scene_write(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  const bool ready = static_cast<bool>(bridge().document.write_document) ||
                     static_cast<bool>(bridge().document.write_active_geojson);
  return detail::il_flag_text("write_path", ready, "write_document", path, false);
}

PyObject* scene_feature_count(PyObject*, PyObject*) {
  if (!bridge().document.feature_count) {
    return detail::bridge_required("feature_count");
  }
  std::string text;
  if (!detail::il_out("feature_count", &text)) {
    return nullptr;
  }
  return PyLong_FromLong(std::strtol(text.c_str(), nullptr, 10));
}

PyObject* scene_present_mode(PyObject*, PyObject*) {
  if (!bridge().view.present_mode) {
    return detail::bridge_required("present_mode");
  }
  std::string mode;
  if (!detail::il_out("present_mode", &mode)) {
    return nullptr;
  }
  return PyUnicode_FromString(mode.c_str());
}

PyObject* scene_set_present_mode(PyObject*, PyObject* args) {
  const char* mode = nullptr;
  if (!PyArg_ParseTuple(args, "s", &mode)) {
    return nullptr;
  }
  return detail::il_flag_text(
      "set_present_mode", static_cast<bool>(bridge().view.set_present_mode),
      "set_present_mode", mode, false);
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
