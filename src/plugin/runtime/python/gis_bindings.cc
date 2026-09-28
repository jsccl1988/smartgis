// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/gis_bindings.h"

#include "plugin/runtime/python/runtime.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/processing/ops_runner.h"

#include <filesystem>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#if defined(SMT_HAS_PYTHON)
#define PY_SSIZE_T_CLEAN
#ifdef _DEBUG
#define SMT_PYTHON_RESTORE_DEBUG
#undef _DEBUG
#endif
#include <Python.h>
#ifdef SMT_PYTHON_RESTORE_DEBUG
#define _DEBUG
#undef SMT_PYTHON_RESTORE_DEBUG
#endif
#endif

namespace plugin {
namespace {

GisConsoleBridge g_gis_bridge;
content::PluginHost* g_gis_host = nullptr;

#if defined(SMT_HAS_PYTHON)

std::string json_escape_path(std::string_view path) {
  std::string out;
  out.reserve(path.size());
  for (char c : path) {
    if (c == '\\') {
      out += '/';
    } else if (c == '"') {
      out += "\\\"";
    } else {
      out += c;
    }
  }
  return out;
}

bool needs_clip_op(std::string_view id) {
  return id == "native.clip" || id == "native.intersection" ||
         id == "native.union" || id == "native.difference" ||
         id == "native.symmetric_difference";
}

bool needs_distance_op(std::string_view id) {
  return id == "native.buffer" || id == "native.simplify";
}

PyObject* analysis_ops(PyObject*, PyObject*) {
  PyObject* list = PyList_New(0);
  if (!list) {
    return nullptr;
  }
  for (const BuiltinOpDesc& d : builtin_op_catalog()) {
    PyObject* item =
        Py_BuildValue("{s:s,s:s}", "id", d.id, "title", d.title);
    if (!item || PyList_Append(list, item) < 0) {
      Py_XDECREF(item);
      Py_DECREF(list);
      return nullptr;
    }
    Py_DECREF(item);
  }
  return list;
}

PyObject* analysis_run(PyObject*, PyObject* args, PyObject* kwargs) {
  const char* processing_id = nullptr;
  double distance = 0.05;
  static const char* kKw[] = {"id", "distance", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "s|d",
                                   const_cast<char**>(kKw), &processing_id,
                                   &distance)) {
    return nullptr;
  }
  if (!g_gis_host) {
    PyErr_SetString(PyExc_RuntimeError, "PluginHost not bound");
    return nullptr;
  }
  if (!g_gis_bridge.write_active_geojson || !g_gis_bridge.load_result_geojson) {
    PyErr_SetString(PyExc_RuntimeError, "gis console bridge not bound");
    return nullptr;
  }

  namespace fs = std::filesystem;
  const fs::path dir = fs::temp_directory_path();
  const fs::path in = dir / "smartgis_py_analysis_in.geojson";
  const fs::path out = dir / "smartgis_py_analysis_out.geojson";

  if (!g_gis_bridge.write_active_geojson(in.string())) {
    PyErr_SetString(PyExc_RuntimeError, "write_active_geojson failed");
    return nullptr;
  }

  std::ostringstream args_json;
  args_json << "{\"input\":\"" << json_escape_path(in.string())
            << "\",\"output\":\"" << json_escape_path(out.string()) << "\"";
  if (needs_distance_op(processing_id)) {
    args_json << ",\"distance\":" << distance;
  }
  if (needs_clip_op(processing_id)) {
    // Phase-1: clip against the same active geometry when no clip layer.
    args_json << ",\"clip\":\"" << json_escape_path(in.string()) << "\"";
  }
  args_json << "}";

  const bool ok = g_gis_host->run_processing(processing_id, args_json.str());
  if (g_gis_bridge.flush_processing) {
    g_gis_bridge.flush_processing();
  }
  int features = 0;
  std::string text;
  if (!ok) {
    text = std::string("Processing failed: ") + processing_id;
  } else if (!g_gis_bridge.load_result_geojson(out.string())) {
    text = std::string("write-back failed: ") + processing_id;
  } else {
    if (g_gis_bridge.feature_count) {
      features = g_gis_bridge.feature_count();
    }
    text = std::string("ok: ") + processing_id +
           " features=" + std::to_string(features);
    if (g_gis_bridge.refresh_map) {
      g_gis_bridge.refresh_map();
    }
  }

  return Py_BuildValue("{s:N,s:s,s:i}", "ok", PyBool_FromLong(ok ? 1 : 0),
                       "text", text.c_str(), "feature_count", features);
}

PyObject* analysis_buffer(PyObject* self, PyObject* args, PyObject* kwargs) {
  double distance = 0.05;
  static const char* kKw[] = {"distance", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|d",
                                   const_cast<char**>(kKw), &distance)) {
    return nullptr;
  }
  PyObject* id = PyUnicode_FromString("native.buffer");
  PyObject* call_args = PyTuple_Pack(1, id);
  Py_DECREF(id);
  PyObject* call_kw = Py_BuildValue("{s:d}", "distance", distance);
  PyObject* result = analysis_run(self, call_args, call_kw);
  Py_DECREF(call_args);
  Py_DECREF(call_kw);
  return result;
}

#define DEFINE_SIMPLE_OP(fn_name, op_id)                               \
  PyObject* fn_name(PyObject* self, PyObject* args, PyObject* kwargs) { \
    (void)args;                                                        \
    (void)kwargs;                                                      \
    PyObject* id = PyUnicode_FromString(op_id);                        \
    PyObject* call_args = PyTuple_Pack(1, id);                         \
    Py_DECREF(id);                                                     \
    PyObject* result = analysis_run(self, call_args, nullptr);         \
    Py_DECREF(call_args);                                              \
    return result;                                                     \
  }

DEFINE_SIMPLE_OP(analysis_centroid, "native.centroid")
DEFINE_SIMPLE_OP(analysis_envelope, "native.envelope")
DEFINE_SIMPLE_OP(analysis_convex_hull, "native.convex_hull")
DEFINE_SIMPLE_OP(analysis_boundary, "native.boundary")

PyObject* analysis_simplify(PyObject* self, PyObject* args, PyObject* kwargs) {
  double distance = 0.01;
  static const char* kKw[] = {"distance", "tolerance", nullptr};
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|d",
                                   const_cast<char**>(kKw), &distance)) {
    return nullptr;
  }
  PyObject* id = PyUnicode_FromString("native.simplify");
  PyObject* call_args = PyTuple_Pack(1, id);
  Py_DECREF(id);
  PyObject* call_kw = Py_BuildValue("{s:d}", "distance", distance);
  PyObject* result = analysis_run(self, call_args, call_kw);
  Py_DECREF(call_args);
  Py_DECREF(call_kw);
  return result;
}

PyMethodDef kAnalysisMethods[] = {
    {"ops", analysis_ops, METH_NOARGS, "List native processing operators."},
    {"run", reinterpret_cast<PyCFunction>(analysis_run),
     METH_VARARGS | METH_KEYWORDS,
     "Run a native.* operator on the active map."},
    {"buffer", reinterpret_cast<PyCFunction>(analysis_buffer),
     METH_VARARGS | METH_KEYWORDS, "Buffer active features."},
    {"simplify", reinterpret_cast<PyCFunction>(analysis_simplify),
     METH_VARARGS | METH_KEYWORDS, "Simplify active features."},
    {"centroid", reinterpret_cast<PyCFunction>(analysis_centroid),
     METH_VARARGS | METH_KEYWORDS, "Centroid of active features."},
    {"envelope", reinterpret_cast<PyCFunction>(analysis_envelope),
     METH_VARARGS | METH_KEYWORDS, "Envelope of active features."},
    {"convex_hull", reinterpret_cast<PyCFunction>(analysis_convex_hull),
     METH_VARARGS | METH_KEYWORDS, "Convex hull of active features."},
    {"boundary", reinterpret_cast<PyCFunction>(analysis_boundary),
     METH_VARARGS | METH_KEYWORDS, "Boundary of active features."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kAnalysisMod = {PyModuleDef_HEAD_INIT, "smartgis.gis.analysis",
                            "Spatial analysis (OpsRunner / OGR+GEOS).", -1,
                            kAnalysisMethods};

PyObject* bridge_required(const char* what) {
  PyErr_Format(PyExc_RuntimeError, "gis console bridge missing: %s", what);
  return nullptr;
}

PyObject* loads_json(const std::string& text) {
  PyObject* json_mod = PyImport_ImportModule("json");
  if (!json_mod) {
    return nullptr;
  }
  PyObject* loads = PyObject_GetAttrString(json_mod, "loads");
  Py_DECREF(json_mod);
  if (!loads) {
    return nullptr;
  }
  PyObject* py_s =
      PyUnicode_FromStringAndSize(text.data(), static_cast<Py_ssize_t>(text.size()));
  if (!py_s) {
    Py_DECREF(loads);
    return nullptr;
  }
  PyObject* out = PyObject_CallFunctionObjArgs(loads, py_s, nullptr);
  Py_DECREF(loads);
  Py_DECREF(py_s);
  return out;
}

PyObject* scene_layers(PyObject*, PyObject*) {
  if (!g_gis_bridge.layers_json) {
    return bridge_required("layers_json");
  }
  return loads_json(g_gis_bridge.layers_json());
}

PyObject* scene_select_layer(PyObject*, PyObject* args) {
  const char* id = nullptr;
  if (!PyArg_ParseTuple(args, "s", &id)) {
    return nullptr;
  }
  if (!g_gis_bridge.select_layer) {
    return bridge_required("select_layer");
  }
  if (!g_gis_bridge.select_layer(id ? id : "")) {
    Py_RETURN_FALSE;
  }
  if (g_gis_bridge.refresh_map) {
    g_gis_bridge.refresh_map();
  }
  Py_RETURN_TRUE;
}

PyObject* scene_set_visible(PyObject*, PyObject* args) {
  const char* id = nullptr;
  int on = 1;
  if (!PyArg_ParseTuple(args, "sp", &id, &on)) {
    return nullptr;
  }
  if (!g_gis_bridge.set_layer_visible) {
    return bridge_required("set_layer_visible");
  }
  if (!g_gis_bridge.set_layer_visible(id ? id : "", on != 0)) {
    Py_RETURN_FALSE;
  }
  if (g_gis_bridge.refresh_map) {
    g_gis_bridge.refresh_map();
  }
  Py_RETURN_TRUE;
}

PyObject* scene_extent(PyObject*, PyObject*) {
  if (!g_gis_bridge.extent_json) {
    return bridge_required("extent_json");
  }
  const std::string j = g_gis_bridge.extent_json();
  if (j.empty() || j == "null") {
    Py_RETURN_NONE;
  }
  return loads_json(j);
}

PyObject* scene_open(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  if (!g_gis_bridge.open_path) {
    return bridge_required("open_path");
  }
  if (!g_gis_bridge.open_path(path ? path : "")) {
    Py_RETURN_FALSE;
  }
  if (g_gis_bridge.refresh_map) {
    g_gis_bridge.refresh_map();
  }
  Py_RETURN_TRUE;
}

PyObject* scene_write(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  auto fn = g_gis_bridge.write_path ? g_gis_bridge.write_path
                                   : g_gis_bridge.write_active_geojson;
  if (!fn) {
    return bridge_required("write_path");
  }
  if (!fn(path ? path : "")) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyObject* scene_feature_count(PyObject*, PyObject*) {
  if (!g_gis_bridge.feature_count) {
    return bridge_required("feature_count");
  }
  return PyLong_FromLong(g_gis_bridge.feature_count());
}

PyObject* scene_present_mode(PyObject*, PyObject*) {
  if (!g_gis_bridge.present_mode) {
    return bridge_required("present_mode");
  }
  const std::string m = g_gis_bridge.present_mode();
  return PyUnicode_FromString(m.c_str());
}

PyObject* scene_set_present_mode(PyObject*, PyObject* args) {
  const char* mode = nullptr;
  if (!PyArg_ParseTuple(args, "s", &mode)) {
    return nullptr;
  }
  if (!g_gis_bridge.set_present_mode) {
    return bridge_required("set_present_mode");
  }
  if (!g_gis_bridge.set_present_mode(mode ? mode : "")) {
    Py_RETURN_FALSE;
  }
  Py_RETURN_TRUE;
}

PyMethodDef kSceneMethods[] = {
    {"layers", scene_layers, METH_NOARGS, "List MapScene layers."},
    {"select_layer", scene_select_layer, METH_VARARGS, "Select layer by id."},
    {"set_visible", scene_set_visible, METH_VARARGS, "Set layer visibility."},
    {"extent", scene_extent, METH_NOARGS, "Map extent dict or None."},
    {"open", scene_open, METH_VARARGS, "Open path into MapScene."},
    {"write", scene_write, METH_VARARGS, "Write active layer GeoJSON."},
    {"feature_count", scene_feature_count, METH_NOARGS, "Feature count."},
    {"present_mode", scene_present_mode, METH_NOARGS, "map2d|data|scene3d."},
    {"set_present_mode", scene_set_present_mode, METH_VARARGS,
     "Switch Map/Data/3D tab."},
    {nullptr, nullptr, 0, nullptr},
};

PyObject* style_has_document(PyObject*, PyObject*) {
  if (!g_gis_bridge.has_style_document) {
    return bridge_required("has_style_document");
  }
  if (g_gis_bridge.has_style_document()) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

PyObject* style_load(PyObject*, PyObject* args) {
  const char* path = nullptr;
  if (!PyArg_ParseTuple(args, "s", &path)) {
    return nullptr;
  }
  if (!g_gis_bridge.load_style_path) {
    return bridge_required("load_style_path");
  }
  if (!g_gis_bridge.load_style_path(path ? path : "")) {
    Py_RETURN_FALSE;
  }
  if (g_gis_bridge.refresh_map) {
    g_gis_bridge.refresh_map();
  }
  Py_RETURN_TRUE;
}

PyObject* style_clear(PyObject*, PyObject*) {
  if (!g_gis_bridge.clear_style) {
    return bridge_required("clear_style");
  }
  g_gis_bridge.clear_style();
  if (g_gis_bridge.refresh_map) {
    g_gis_bridge.refresh_map();
  }
  Py_RETURN_NONE;
}

PyObject* style_summary(PyObject*, PyObject*) {
  if (!g_gis_bridge.style_summary_json) {
    return bridge_required("style_summary_json");
  }
  const std::string j = g_gis_bridge.style_summary_json();
  if (j.empty() || j == "null") {
    Py_RETURN_NONE;
  }
  return loads_json(j);
}

PyMethodDef kStyleMethods[] = {
    {"has_document", style_has_document, METH_NOARGS, nullptr},
    {"load", style_load, METH_VARARGS, "Load StyleDocument JSON path."},
    {"clear", style_clear, METH_NOARGS, nullptr},
    {"summary", style_summary, METH_NOARGS, "Style name/version/layer ids."},
    {nullptr, nullptr, 0, nullptr},
};

PyModuleDef kSceneMod = {PyModuleDef_HEAD_INIT, "smartgis.gis.scene",
                         "MapScene 2D/3D document façade.", -1, kSceneMethods};
PyModuleDef kStyleMod = {PyModuleDef_HEAD_INIT, "smartgis.gis.style",
                         "Cartographic StyleDocument on MapScene.", -1,
                         kStyleMethods};
PyModuleDef kGisMod = {
    PyModuleDef_HEAD_INIT, "smartgis.gis",
    "Bindings mirroring src/gis (analysis + scene + style).", -1, nullptr};

bool attach_gis_module_impl(PyObject* smartgis_module) {
  if (!smartgis_module) {
    return false;
  }
  PyObject* gis = PyModule_Create(&kGisMod);
  PyObject* analysis = PyModule_Create(&kAnalysisMod);
  PyObject* scene = PyModule_Create(&kSceneMod);
  PyObject* style = PyModule_Create(&kStyleMod);
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

#endif  // SMT_HAS_PYTHON

}  // namespace

#if defined(SMT_HAS_PYTHON)

bool attach_gis_module(PyObject* smartgis_module) {
  return attach_gis_module_impl(smartgis_module);
}

#endif

void set_gis_console_bridge(GisConsoleBridge bridge) {
  g_gis_bridge = std::move(bridge);
}

void clear_gis_console_bridge() {
  g_gis_bridge = {};
}

void bind_gis_host_for_analysis(content::PluginHost* host) {
  g_gis_host = host;
}

bool try_activate_tool(uint32_t view_id, const std::string& tool_id) {
  if (!g_gis_bridge.activate_tool) {
    return false;
  }
  return g_gis_bridge.activate_tool(view_id, tool_id);
}

}  // namespace plugin
