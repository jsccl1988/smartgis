// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/ext_module.h"

#include "base/core/log.h"
#include "content/browser/glang.runtime/exec/py/plugin_fn.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(HAS_PYTHON)
#define PY_SSIZE_T_CLEAN
#ifdef _DEBUG
#define PYTHON_RESTORE_DEBUG
#undef _DEBUG
#endif
#include <Python.h>
#ifdef PYTHON_RESTORE_DEBUG
#define _DEBUG
#undef PYTHON_RESTORE_DEBUG
#endif
#endif

namespace content {
namespace py {
namespace {

struct ExtFn {
  std::string name;
  const code::Param* params = nullptr;
  int param_count = 0;
  code::EvalFn eval = nullptr;
};

struct ExtTable {
  std::mutex mu;
  // plugin_id -> name -> fn
  std::unordered_map<std::string, std::unordered_map<std::string, ExtFn>> by_id;
  // plugin_id -> fully-qualified module name (glang.ext.… )
  std::unordered_map<std::string, std::string> mounted;
};

ExtTable& table() {
  static ExtTable instance;
  return instance;
}

#if defined(HAS_PYTHON)

bool valid_segment(std::string_view seg) {
  if (seg.empty()) {
    return false;
  }
  for (char c : seg) {
    const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                    (c >= '0' && c <= '9') || c == '_';
    if (!ok) {
      return false;
    }
  }
  return true;
}

bool valid_plugin_id(std::string_view id) {
  if (id.empty() || id.front() == '.' || id.back() == '.') {
    return false;
  }
  size_t start = 0;
  for (size_t i = 0; i <= id.size(); ++i) {
    if (i == id.size() || id[i] == '.') {
      if (!valid_segment(id.substr(start, i - start))) {
        return false;
      }
      start = i + 1;
    }
  }
  return true;
}

std::string full_module_name(std::string_view plugin_id) {
  return std::string("glang.ext.") + std::string(plugin_id);
}

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
  if (n != 2) {
    return false;
  }
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

PyObject* ext_dispatch(PyObject* /*self*/, PyObject* args) {
  const char* plugin_id = nullptr;
  const char* name = nullptr;
  PyObject* positional = nullptr;
  PyObject* named = nullptr;
  if (!PyArg_ParseTuple(args, "ssOO", &plugin_id, &name, &positional, &named)) {
    return nullptr;
  }
  ExtFn fn;
  {
    std::lock_guard<std::mutex> lock(table().mu);
    auto id_it = table().by_id.find(plugin_id ? plugin_id : "");
    if (id_it == table().by_id.end()) {
      PyErr_SetString(PyExc_RuntimeError, "extension not mounted");
      return nullptr;
    }
    auto fn_it = id_it->second.find(name ? name : "");
    if (fn_it == id_it->second.end() || !fn_it->second.eval) {
      PyErr_Format(PyExc_AttributeError, "unknown extension verb %s",
                   name ? name : "");
      return nullptr;
    }
    fn = fn_it->second;
  }

  code::ScriptCall call;
  call.name = name ? name : "";
  const Py_ssize_t npos =
      PySequence_Check(positional) ? PySequence_Size(positional) : 0;
  call.positional.reserve(static_cast<size_t>(npos > 0 ? npos : 0));
  for (Py_ssize_t i = 0; i < npos; ++i) {
    PyObject* item = PySequence_GetItem(positional, i);
    std::string text;
    if (!item || !value_to_string(item, &text)) {
      Py_XDECREF(item);
      PyErr_SetString(PyExc_TypeError,
                      "extension positional arg must be str, int, or (x, y)");
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
        PyErr_SetString(PyExc_TypeError,
                        "extension keyword must be str, int, or (x, y)");
        return nullptr;
      }
      call.named.emplace_back(k, std::move(text));
    }
  }

  std::string error;
  std::string result;
  // Route through the core script handler via a one-shot global register so
  // extension-only names share the same CapabilityHost path as glang.plugin.
  if (!register_fn(fn.name, fn.params, fn.param_count, fn.eval)) {
    PyErr_SetString(PyExc_RuntimeError, "extension register failed");
    return nullptr;
  }
  const bool ok = call_registered(call, &error, &result);
  unregister_fn(fn.name);
  if (!ok) {
    PyErr_SetString(PyExc_RuntimeError,
                    error.empty() ? "extension call failed" : error.c_str());
    return nullptr;
  }
  if (!result.empty()) {
    return PyUnicode_FromString(result.c_str());
  }
  Py_RETURN_NONE;
}

PyMethodDef kExtMethods[] = {
    {"_dispatch", ext_dispatch, METH_VARARGS,
     "_dispatch(plugin_id, name, args, kwargs)"},
    {nullptr, nullptr, 0, nullptr},
};

const char kInstallExt[] = R"PY(
import sys
import types

def _ensure(name):
    if name in sys.modules:
        return sys.modules[name]
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    parent, _, leaf = name.rpartition(".")
    if parent:
        setattr(_ensure(parent), leaf, m)
    return m

_ensure("glang")
_ensure("glang.ext")
)PY";

const char kMountExt[] = R"PY(
import importlib.util
import sys
import types
from pathlib import Path

def _ensure(name):
    if name in sys.modules:
        return sys.modules[name]
    m = types.ModuleType(name)
    m.__path__ = []
    sys.modules[name] = m
    parent, _, leaf = name.rpartition(".")
    if parent:
        setattr(_ensure(parent), leaf, m)
    return m

def _attach_dispatch(mod, plugin_id, dispatch):
    mod._plugin_id = plugin_id
    mod._dispatch = dispatch
    def __getattr__(name):
        if name.startswith("_"):
            raise AttributeError(name)
        def fn(*args, **kwargs):
            return dispatch(plugin_id, name, args, kwargs)
        fn.__name__ = name
        setattr(mod, name, fn)
        return fn
    mod.__getattr__ = __getattr__

def mount(plugin_id, root, dispatch):
    root = Path(root)
    parts = [p for p in plugin_id.split(".") if p]
    if not parts:
        raise ValueError("empty plugin_id")
    _ensure("glang")
    _ensure("glang.ext")
    qn = "glang.ext"
    for i, part in enumerate(parts):
        qn = qn + "." + part
        is_leaf = i + 1 == len(parts)
        if not is_leaf:
            _ensure(qn)
            continue
        # Replace a prior mount of the same leaf.
        if qn in sys.modules:
            del sys.modules[qn]
        ext_dir = root / "ext"
        ext_py = root / "ext.py"
        if ext_dir.is_dir():
            init = ext_dir / "__init__.py"
            if init.is_file():
                spec = importlib.util.spec_from_file_location(
                    qn, str(init),
                    submodule_search_locations=[str(ext_dir)])
                mod = importlib.util.module_from_spec(spec)
                sys.modules[qn] = mod
                setattr(sys.modules[qn.rpartition(".")[0]], parts[-1], mod)
                spec.loader.exec_module(mod)
            else:
                mod = _ensure(qn)
                mod.__path__ = [str(ext_dir)]
            _attach_dispatch(mod, plugin_id, dispatch)
            return qn
        if ext_py.is_file():
            spec = importlib.util.spec_from_file_location(qn, str(ext_py))
            mod = importlib.util.module_from_spec(spec)
            sys.modules[qn] = mod
            setattr(sys.modules[qn.rpartition(".")[0]], parts[-1], mod)
            spec.loader.exec_module(mod)
            _attach_dispatch(mod, plugin_id, dispatch)
            return qn
        mod = _ensure(qn)
        _attach_dispatch(mod, plugin_id, dispatch)
        return qn
)PY";

const char kUnmountExt[] = R"PY(
import sys

def unmount(qn):
    if not qn:
        return
    # Drop the leaf and any still-empty parents under glang.ext.
    parts = qn.split(".")
    while len(parts) > 2:  # keep glang.ext
        name = ".".join(parts)
        mod = sys.modules.get(name)
        if mod is not None:
            parent_name = ".".join(parts[:-1])
            parent = sys.modules.get(parent_name)
            if parent is not None and hasattr(parent, parts[-1]):
                try:
                    delattr(parent, parts[-1])
                except Exception:
                    pass
            del sys.modules[name]
        # Stop walking up if a sibling still exists under the parent.
        parent_name = ".".join(parts[:-1])
        parent = sys.modules.get(parent_name)
        if parent is not None:
            attrs = [a for a in dir(parent) if not a.startswith("_")]
            if attrs:
                break
        parts.pop()
)PY";

bool run_blob(const char* src, PyObject* globals) {
  PyObject* r = PyRun_String(src, Py_file_input, globals, globals);
  if (!r) {
    PyErr_Print();
    PyErr_Clear();
    return false;
  }
  Py_DECREF(r);
  return true;
}

PyObject* dispatch_capsule() {
  // Expose C dispatch as a Python-callable via PyCFunction on a throwaway
  // module method — callers pass the function object from kExtMethods.
  static PyObject* fn = nullptr;
  if (fn) {
    return fn;
  }
  static PyModuleDef def = {
      PyModuleDef_HEAD_INIT,
      "_glang_ext_native",
      nullptr,
      -1,
      kExtMethods,
  };
  PyObject* mod = PyModule_Create(&def);
  if (!mod) {
    return nullptr;
  }
  fn = PyObject_GetAttrString(mod, "_dispatch");
  Py_DECREF(mod);
  return fn;
}

#endif

}  // namespace

void install_glang_ext() {
#if defined(HAS_PYTHON)
  if (!Py_IsInitialized()) {
    return;
  }
  PyObject* g = PyDict_New();
  if (!g) {
    PyErr_Clear();
    return;
  }
  PyDict_SetItemString(g, "__builtins__", PyEval_GetBuiltins());
  if (!run_blob(kInstallExt, g)) {
    LOGGING(LOG_ERROR, "glang.ext: install failed");
  }
  Py_DECREF(g);
#else
  return;
#endif
}

bool mount_extension(std::string_view plugin_id, std::string_view root_dir) {
#if defined(HAS_PYTHON)
  if (!Py_IsInitialized() || !valid_plugin_id(plugin_id) || root_dir.empty()) {
    return false;
  }
  install_glang_ext();
  PyObject* dispatch = dispatch_capsule();
  if (!dispatch) {
    LOGGING(LOG_ERROR, "glang.ext: dispatch missing");
    PyErr_Clear();
    return false;
  }
  PyObject* g = PyDict_New();
  if (!g) {
    PyErr_Clear();
    return false;
  }
  PyDict_SetItemString(g, "__builtins__", PyEval_GetBuiltins());
  if (!run_blob(kMountExt, g)) {
    Py_DECREF(g);
    return false;
  }
  PyObject* mount = PyDict_GetItemString(g, "mount");
  if (!mount || !PyCallable_Check(mount)) {
    Py_DECREF(g);
    return false;
  }
  PyObject* id_obj = PyUnicode_FromStringAndSize(
      plugin_id.data(), static_cast<Py_ssize_t>(plugin_id.size()));
  PyObject* root_obj = PyUnicode_FromStringAndSize(
      root_dir.data(), static_cast<Py_ssize_t>(root_dir.size()));
  if (!id_obj || !root_obj) {
    Py_XDECREF(id_obj);
    Py_XDECREF(root_obj);
    Py_DECREF(g);
    PyErr_Clear();
    return false;
  }
  PyObject* qn_obj =
      PyObject_CallFunctionObjArgs(mount, id_obj, root_obj, dispatch, nullptr);
  Py_DECREF(id_obj);
  Py_DECREF(root_obj);
  if (!qn_obj) {
    PyErr_Print();
    PyErr_Clear();
    Py_DECREF(g);
    return false;
  }
  const char* qn = PyUnicode_AsUTF8(qn_obj);
  {
    std::lock_guard<std::mutex> lock(table().mu);
    table().mounted[std::string(plugin_id)] = qn ? qn : full_module_name(plugin_id);
  }
  Py_DECREF(qn_obj);
  Py_DECREF(g);
  LOGGING(LOG_INFO, "glang.ext: mounted %.*s",
          static_cast<int>(plugin_id.size()), plugin_id.data());
  return true;
#else
  (void)plugin_id;
  (void)root_dir;
  return false;
#endif
}

void unmount_extension(std::string_view plugin_id) {
#if defined(HAS_PYTHON)
  if (!Py_IsInitialized() || plugin_id.empty()) {
    return;
  }
  std::string qn;
  {
    std::lock_guard<std::mutex> lock(table().mu);
    auto it = table().mounted.find(std::string(plugin_id));
    if (it == table().mounted.end()) {
      table().by_id.erase(std::string(plugin_id));
      return;
    }
    qn = it->second;
    table().mounted.erase(it);
    table().by_id.erase(std::string(plugin_id));
  }
  PyObject* g = PyDict_New();
  if (!g) {
    PyErr_Clear();
    return;
  }
  PyDict_SetItemString(g, "__builtins__", PyEval_GetBuiltins());
  if (!run_blob(kUnmountExt, g)) {
    Py_DECREF(g);
    return;
  }
  PyObject* unmount = PyDict_GetItemString(g, "unmount");
  PyObject* qn_obj = PyUnicode_FromString(qn.c_str());
  if (unmount && PyCallable_Check(unmount) && qn_obj) {
    PyObject* r = PyObject_CallFunctionObjArgs(unmount, qn_obj, nullptr);
    if (!r) {
      PyErr_Clear();
    } else {
      Py_DECREF(r);
    }
  }
  Py_XDECREF(qn_obj);
  Py_DECREF(g);
#else
  (void)plugin_id;
#endif
}

bool register_extension_fn(std::string_view plugin_id,
                           std::string_view name,
                           const code::Param* params,
                           int param_count,
                           code::EvalFn eval) {
  if (plugin_id.empty() || name.empty() || !eval) {
    return false;
  }
  std::lock_guard<std::mutex> lock(table().mu);
  ExtFn fn;
  fn.name = std::string(name);
  fn.params = params;
  fn.param_count = param_count;
  fn.eval = eval;
  table().by_id[std::string(plugin_id)][std::string(name)] = std::move(fn);
  return true;
}

void unregister_extension_fn(std::string_view plugin_id, std::string_view name) {
  if (plugin_id.empty() || name.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(table().mu);
  auto id_it = table().by_id.find(std::string(plugin_id));
  if (id_it == table().by_id.end()) {
    return;
  }
  id_it->second.erase(std::string(name));
}

}  // namespace py
}  // namespace content
