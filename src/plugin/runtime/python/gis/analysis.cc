// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/gis/marshal.h"

#include "content/browser/glang.runtime/host/bind/gis_console.h"
#include "plugin/runtime/python/bind/gil.h"

#include <sstream>
#include <string>
#include <utility>

namespace plugin {
#if defined(HAS_PYTHON)
namespace {

PyObject* analysis_ops(PyObject*, PyObject*) {
  std::string json;
  std::string error;
  if (!detail::call_il("list_analysis_ops", nullptr, 0, &json, &error)) {
    PyErr_SetString(PyExc_RuntimeError, error.empty() ? "list_analysis_ops" : error.c_str());
    return nullptr;
  }
  return detail::loads_json(json);
}

PyObject* analysis_run(PyObject*, PyObject* args, PyObject* kwargs) {
  PyObject* id_obj = nullptr;
  if (args && PyTuple_Check(args) && PyTuple_GET_SIZE(args) >= 1) {
    id_obj = PyTuple_GET_ITEM(args, 0);
  }
  if (!id_obj && kwargs) {
    id_obj = PyDict_GetItemString(kwargs, "id");
  }
  if (!id_obj || !PyUnicode_Check(id_obj)) {
    PyErr_SetString(PyExc_TypeError, "analysis.run requires id: str");
    return nullptr;
  }
  const char* processing_id = PyUnicode_AsUTF8(id_obj);
  double distance = 0.05;
  const char* input_path = nullptr;
  const char* output_path = nullptr;
  if (kwargs) {
    if (PyObject* d = PyDict_GetItemString(kwargs, "distance")) {
      distance = PyFloat_AsDouble(d);
      if (PyErr_Occurred()) {
        return nullptr;
      }
    }
    if (PyObject* p = PyDict_GetItemString(kwargs, "input")) {
      if (p != Py_None && PyUnicode_Check(p)) {
        input_path = PyUnicode_AsUTF8(p);
      }
    }
    if (PyObject* p = PyDict_GetItemString(kwargs, "output")) {
      if (p != Py_None && PyUnicode_Check(p)) {
        output_path = PyUnicode_AsUTF8(p);
      }
    }
  }
  content::code::MapAnalysisRequest request;
  request.id = processing_id ? processing_id : "";
  request.distance = distance;
  if (input_path && input_path[0]) {
    request.input = input_path;
  }
  if (output_path && output_path[0]) {
    request.output = output_path;
  }
  if (kwargs) {
    PyObject* key = nullptr;
    PyObject* val = nullptr;
    Py_ssize_t pos = 0;
    while (PyDict_Next(kwargs, &pos, &key, &val)) {
      if (!PyUnicode_Check(key)) {
        continue;
      }
      const char* k = PyUnicode_AsUTF8(key);
      if (!k) {
        continue;
      }
      const std::string name(k);
      if (name == "id" || name == "distance" || name == "input" ||
          name == "output") {
        continue;
      }
      std::ostringstream literal;
      detail::append_json_scalar(literal, val);
      request.extra_json.emplace_back(name, literal.str());
    }
  }
  content::code::MapAnalysisResult result;
  {
    detail::ReleaseGil released;
    result = content::code::run_map_analysis(request);
  }
  if (!result.error.empty()) {
    PyErr_SetString(PyExc_RuntimeError, result.error.c_str());
    return nullptr;
  }
  return Py_BuildValue("{s:N,s:s,s:i}", "ok",
                       PyBool_FromLong(result.ok ? 1 : 0), "text",
                       result.text.c_str(), "feature_count",
                       result.feature_count);
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

// Active-map helpers stay on kRunProcessing. Pure geometry is buffer /
// centroid / envelope / convex_hull / boundary / simplify on a WKT module.
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

// Existing rows forward through detail::call_il / run_map_analysis.
// Do not add PyMethodDef entries.
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

}  // namespace

namespace detail {

PyObject* make_analysis_module() {
  return PyModule_Create(&kAnalysisMod);
}

}  // namespace detail
#endif
}  // namespace plugin
