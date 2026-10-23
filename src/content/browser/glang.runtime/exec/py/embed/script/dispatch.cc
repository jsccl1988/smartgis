// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/script/script.h"

#include "content/browser/glang.runtime/exec/py/plugin_fn.h"
#include "content/browser/glang.runtime/exec/py/script_call.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"

#include <string>

#if defined(HAS_PYTHON)

namespace plugin {
namespace {

bool value_to_string(PyObject* obj, std::string* out) {
  if (!obj || !out || obj == Py_None) {
    return false;
  }
  if (PyBool_Check(obj)) {
    *out = (obj == Py_True) ? "1" : "0";
    return true;
  }
  if (PyLong_Check(obj)) {
    const long v = PyLong_AsLong(obj);
    if (PyErr_Occurred()) {
      PyErr_Clear();
      return false;
    }
    *out = std::to_string(v);
    return true;
  }
  if (PyFloat_Check(obj)) {
    *out = std::to_string(static_cast<long>(PyFloat_AsDouble(obj)));
    return !PyErr_Occurred();
  }
  if (PyUnicode_Check(obj)) {
    const char* utf8 = PyUnicode_AsUTF8(obj);
    if (!utf8) {
      return false;
    }
    *out = utf8;
    return true;
  }
  const bool seq = PyTuple_Check(obj) || PyList_Check(obj);
  if (!seq) {
    return false;
  }
  const Py_ssize_t n = PySequence_Size(obj);
  if (n == 2) {
    PyObject* a = PySequence_GetItem(obj, 0);
    PyObject* b = PySequence_GetItem(obj, 1);
    std::string as;
    std::string bs;
    const bool ok = a && b && value_to_string(a, &as) && value_to_string(b, &bs);
    Py_XDECREF(a);
    Py_XDECREF(b);
    if (!ok) {
      return false;
    }
    *out = as + "," + bs;
    return true;
  }
  return false;
}

// smartgis.script.<verb> resolves on the glang.plugin table via
// call_registered. Named as_ is still accepted so older sources reach
// the same eval_*.
PyObject* script_dispatch(PyObject* /*self*/, PyObject* args) {
  const char* name = nullptr;
  PyObject* positional = nullptr;
  PyObject* named = nullptr;
  if (!PyArg_ParseTuple(args, "sOO", &name, &positional, &named)) {
    return nullptr;
  }
  content::code::ScriptCall call;
  call.name = name ? name : "";
  const Py_ssize_t npos = PySequence_Check(positional) ? PySequence_Size(positional) : 0;
  call.positional.reserve(static_cast<size_t>(npos > 0 ? npos : 0));
  for (Py_ssize_t i = 0; i < npos; ++i) {
    PyObject* item = PySequence_GetItem(positional, i);
    std::string text;
    if (!item || !value_to_string(item, &text)) {
      Py_XDECREF(item);
      PyErr_SetString(PyExc_TypeError, "script positional arg must be str, int, or (x, y)");
      return nullptr;
    }
    Py_DECREF(item);
    call.positional.push_back(std::move(text));
  }
  if (named && named != Py_None && PyDict_Check(named)) {
    PyObject* key = nullptr;
    PyObject* value = nullptr;
    Py_ssize_t pos = 0;
    while (PyDict_Next(named, &pos, &key, &value)) {
      if (!PyUnicode_Check(key) || !value || value == Py_None) {
        continue;
      }
      const char* k = PyUnicode_AsUTF8(key);
      std::string text;
      if (!k || !value_to_string(value, &text)) {
        PyErr_SetString(PyExc_TypeError, "script keyword must be str, int, or (x, y)");
        return nullptr;
      }
      call.named.emplace_back(k, std::move(text));
    }
  }
  std::string error;
  std::string result;
  if (!content::py::call_registered(call, &error, &result)) {
    PyErr_SetString(PyExc_RuntimeError,
                    error.empty() ? "script call failed" : error.c_str());
    return nullptr;
  }
  if (!result.empty()) {
    return PyUnicode_FromString(result.c_str());
  }
  Py_RETURN_NONE;
}

PyMethodDef kScriptMethods[] = {
    {"_dispatch", script_dispatch, METH_VARARGS,
     "_dispatch(name, args, kwargs) � one capability call."},
    {nullptr, nullptr, 0, nullptr},
};

const char kBootstrap[] = R"PY(
class chord:
    """Hold modifier keys down for the body, then release them."""

    def __init__(self, *mods):
        self._mods = mods

    def __enter__(self):
        for mod in self._mods:
            _dispatch("key_down", (mod,), {})
        return self

    def __exit__(self, exc_type, exc, tb):
        for mod in reversed(self._mods):
            _dispatch("key_up", (mod,), {})
        return False

def __getattr__(name):
    if name.startswith("_"):
        raise AttributeError(name)

    def fn(*args, **kwargs):
        return _dispatch(name, args, kwargs)

    fn.__name__ = name
    globals()[name] = fn
    return fn
)PY";

}  // namespace

namespace detail {

void attach_script_module(PyObject* smartgis_module) {
  if (!smartgis_module) {
    return;
  }
  static PyModuleDef def = {
      PyModuleDef_HEAD_INIT,
      "smartgis.script",
      "Live capability script. Each former IL verb is a function on this module.",
      -1,
      kScriptMethods,
  };
  PyObject* mod = PyModule_Create(&def);
  if (!mod) {
    return;
  }
  PyObject* dict = PyModule_GetDict(mod);
  PyObject* boot = PyRun_String(kBootstrap, Py_file_input, dict, dict);
  if (!boot) {
    PyErr_Print();
    Py_DECREF(mod);
    return;
  }
  Py_DECREF(boot);
  Py_INCREF(mod);
  if (PyModule_AddObject(smartgis_module, "script", mod) < 0) {
    Py_DECREF(mod);
    Py_DECREF(mod);
    return;
  }
  PyObject* modules = PyImport_GetModuleDict();
  if (modules) {
    PyDict_SetItemString(modules, "smartgis.script", mod);
  }
  Py_DECREF(mod);
}

}  // namespace detail
}  // namespace plugin

#else

namespace plugin {
namespace detail {

void attach_script_module(void* /*smartgis_module*/) {}

}  // namespace detail
}  // namespace plugin

#endif
