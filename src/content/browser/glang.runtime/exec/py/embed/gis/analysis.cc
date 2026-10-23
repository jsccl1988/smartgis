// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/gis/marshal.h"

#include "content/browser/glang.runtime/host/bind/console.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/gil.h"

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

// |kwargs| is borrowed. The caller frees a dict it built itself.
PyObject* call_analysis(PyObject* self, const char* op_id, PyObject* kwargs) {
  PyObject* id = PyUnicode_FromString(op_id);
  if (!id) {
    return nullptr;
  }
  PyObject* call_args = PyTuple_Pack(1, id);
  Py_DECREF(id);
  if (!call_args) {
    return nullptr;
  }
  PyObject* result = analysis_run(self, call_args, kwargs);
  Py_DECREF(call_args);
  return result;
}

PyObject* analysis_with_distance(PyObject* self,
                                 PyObject* args,
                                 PyObject* kwargs,
                                 const char* op_id,
                                 double fallback,
                                 const char* const* names) {
  double distance = fallback;
  if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|d", const_cast<char**>(names),
                                   &distance)) {
    return nullptr;
  }
  PyObject* kw = Py_BuildValue("{s:d}", "distance", distance);
  if (!kw) {
    return nullptr;
  }
  PyObject* result = call_analysis(self, op_id, kw);
  Py_DECREF(kw);
  return result;
}

PyObject* analysis_buffer(PyObject* self, PyObject* args, PyObject* kwargs) {
  static const char* kKw[] = {"distance", nullptr};
  return analysis_with_distance(self, args, kwargs, "native.buffer", 0.05, kKw);
}

// Active-map helpers stay on analysis_run. Pure geometry is buffer /
// centroid / envelope / convex_hull / boundary / simplify.
#define DEFINE_SIMPLE_OP(fn_name, op_id)                                \
  PyObject* fn_name(PyObject* self, PyObject* args, PyObject* kwargs) { \
    (void)args;                                                         \
    (void)kwargs;                                                       \
    return call_analysis(self, op_id, nullptr);                         \
  }

DEFINE_SIMPLE_OP(analysis_centroid, "native.centroid")
DEFINE_SIMPLE_OP(analysis_envelope, "native.envelope")
DEFINE_SIMPLE_OP(analysis_convex_hull, "native.convex_hull")
DEFINE_SIMPLE_OP(analysis_boundary, "native.boundary")

PyObject* analysis_simplify(PyObject* self, PyObject* args, PyObject* kwargs) {
  static const char* kKw[] = {"distance", "tolerance", nullptr};
  return analysis_with_distance(self, args, kwargs, "native.simplify", 0.01, kKw);
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
