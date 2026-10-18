// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/session/cpython.h"

#include "content/browser/glang.runtime/exec/session/plugin_module.h"

#include "base/core/log.h"
#include "base/process/switches.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <string>

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

#if defined(HAS_PYTHON)
#ifndef PYTHON_HOME
#define PYTHON_HOME ""
#endif

std::wstring utf8_to_wide(const std::string& in) {
  if (in.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, in.c_str(), -1, nullptr, 0);
  std::wstring out(static_cast<size_t>(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, in.c_str(), -1, out.data(), n);
  if (!out.empty() && out.back() == L'\0') {
    out.pop_back();
  }
  return out;
}

std::string python_home() {
  const char* env = base::switch_cstr("python-root");
  if (env && env[0]) {
    return env;
  }
  if (PYTHON_HOME[0]) {
    return PYTHON_HOME;
  }
  return {};
}

void add_python_dll_dir(const std::string& home) {
  if (home.empty()) {
    return;
  }
  const std::wstring w = utf8_to_wide(home);
  SetDllDirectoryW(w.c_str());
}

std::string take_python_error(const char* fallback) {
  std::string message = fallback ? fallback : "python error";
  if (!PyErr_Occurred()) {
    return message;
  }
  PyObject* ptype = nullptr;
  PyObject* pval = nullptr;
  PyObject* ptb = nullptr;
  PyErr_Fetch(&ptype, &pval, &ptb);
  PyErr_NormalizeException(&ptype, &pval, &ptb);
  PyObject* text = pval ? PyObject_Str(pval) : nullptr;
  const char* msg = text ? PyUnicode_AsUTF8(text) : nullptr;
  if (msg && msg[0]) {
    message = msg;
  }
  Py_XDECREF(text);
  Py_XDECREF(ptype);
  Py_XDECREF(pval);
  Py_XDECREF(ptb);
  return message;
}

void drop_script_module(const std::string& name, PyObject* mod) {
  if (!name.empty()) {
    PyObject* modules = PyImport_GetModuleDict();
    if (modules && PyDict_DelItemString(modules, name.c_str()) < 0) {
      PyErr_Clear();
    }
  }
  Py_XDECREF(mod);
}

std::string next_script_mod_name() {
  static unsigned long long serial = 0;
  char buf[64];
  std::snprintf(buf, sizeof(buf), "sgscript_%llu", ++serial);
  return buf;
}

bool run_script_file(const std::string& path, std::string* error) {
  auto fail = [&](const std::string& message) {
    if (error) {
      *error = message;
    }
    return false;
  };

  PyGILState_STATE gil = PyGILState_Ensure();
  struct ReleaseGil {
    PyGILState_STATE state;
    ~ReleaseGil() { PyGILState_Release(state); }
  } release_gil{gil};

  const std::string name = next_script_mod_name();
  PyObject* util = PyImport_ImportModule("importlib.util");
  if (!util) {
    return fail(take_python_error("error: importlib.util missing"));
  }
  PyObject* spec = PyObject_CallMethod(util, "spec_from_file_location", "ss",
                                       name.c_str(), path.c_str());
  if (!spec || spec == Py_None) {
    Py_XDECREF(spec);
    Py_DECREF(util);
    return fail(take_python_error("error: cannot load script module"));
  }
  PyObject* mod = PyObject_CallMethod(util, "module_from_spec", "O", spec);
  PyObject* loader = mod ? PyObject_GetAttrString(spec, "loader") : nullptr;
  Py_DECREF(spec);
  Py_DECREF(util);
  if (!mod || !loader) {
    Py_XDECREF(loader);
    drop_script_module(name, mod);
    return fail(take_python_error("error: cannot load script module"));
  }

  PyObject* modules = PyImport_GetModuleDict();
  if (!modules || PyDict_SetItemString(modules, name.c_str(), mod) < 0) {
    Py_DECREF(loader);
    drop_script_module(name, mod);
    return fail(take_python_error("error: cannot register script module"));
  }

  PyObject* executed = PyObject_CallMethod(loader, "exec_module", "O", mod);
  Py_DECREF(loader);
  if (!executed) {
    const std::string message = take_python_error("python error");
    drop_script_module(name, mod);
    return fail(message);
  }
  Py_DECREF(executed);

  PyObject* main_fn = PyObject_GetAttrString(mod, "main");
  if (!main_fn || !PyCallable_Check(main_fn)) {
    Py_XDECREF(main_fn);
    PyErr_Clear();
    drop_script_module(name, mod);
    return fail("error: script missing callable main");
  }
  PyObject* result = PyObject_CallNoArgs(main_fn);
  Py_DECREF(main_fn);
  if (!result) {
    const std::string message = take_python_error("python error");
    drop_script_module(name, mod);
    return fail(message);
  }
  Py_DECREF(result);
  drop_script_module(name, mod);
  return true;
}

bool initialize_isolated(const InitHooks& hooks) {
  const std::string home = python_home();
  add_python_dll_dir(home);
  if (hooks.before_initialize && !hooks.before_initialize()) {
    LOGGING(LOG_ERROR, "cpython: before_initialize failed");
    return false;
  }

  PyConfig config;
  PyConfig_InitIsolatedConfig(&config);
  config.isolated = 1;
  PyStatus status{};
  std::wstring home_w;
  std::wstring zip_w;
  if (!home.empty()) {
    home_w = utf8_to_wide(home);
    zip_w = utf8_to_wide(home + "/python312.zip");
    status = PyConfig_SetString(&config, &config.home, home_w.c_str());
    if (PyStatus_Exception(status)) {
      PyConfig_Clear(&config);
      return false;
    }
    status = PyWideStringList_Append(&config.module_search_paths, home_w.c_str());
    const std::wstring lib_w = utf8_to_wide(home + "/Lib");
    const std::wstring dll_w = utf8_to_wide(home + "/DLLs");
    if (!PyStatus_Exception(status)) {
      status = PyWideStringList_Append(&config.module_search_paths, lib_w.c_str());
    }
    if (!PyStatus_Exception(status)) {
      status = PyWideStringList_Append(&config.module_search_paths, dll_w.c_str());
    }
    if (!PyStatus_Exception(status) &&
        GetFileAttributesW(zip_w.c_str()) != INVALID_FILE_ATTRIBUTES) {
      status = PyWideStringList_Append(&config.module_search_paths, zip_w.c_str());
    }
    if (PyStatus_Exception(status)) {
      PyConfig_Clear(&config);
      return false;
    }
    config.module_search_paths_set = 1;
  }
  status = Py_InitializeFromConfig(&config);
  PyConfig_Clear(&config);
  if (PyStatus_Exception(status) || !Py_IsInitialized()) {
    const char* detail = status.err_msg ? status.err_msg : "no status";
    LOGGING(LOG_ERROR, "cpython: Py_InitializeFromConfig failed: %s", detail);
    return false;
  }
  install_glang_plugin();
  if (hooks.after_initialize) {
    hooks.after_initialize();
  }
  return true;
}

#endif

}  // namespace

bool Interpreter::init(const InitHooks& hooks) {
  if (ready_) {
    return true;
  }
#if defined(HAS_PYTHON)
  ready_ = initialize_isolated(hooks);
  return ready_;
#else
  // Embedding is compile-time. A python312.dll on PATH is not this binary.
  (void)hooks;
  ready_ = false;
  return false;
#endif
}

void Interpreter::shutdown() {
#if defined(HAS_PYTHON)
  if (Py_IsInitialized()) {
    Py_FinalizeEx();
  }
#endif
  ready_ = false;
}

bool Interpreter::is_ready() const {
  return ready_;
}

std::string Interpreter::eval(std::string_view code) {
#if defined(HAS_PYTHON)
  if (!ready_ || !Py_IsInitialized()) {
    return "error: python not ready";
  }
  if (code.empty()) {
    return {};
  }

  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* main_mod = PyImport_AddModule("__main__");
  if (!main_mod) {
    PyGILState_Release(gil);
    return "error: no __main__";
  }
  PyObject* globals = PyModule_GetDict(main_mod);
  PyObject* code_obj =
      PyUnicode_FromStringAndSize(code.data(), static_cast<Py_ssize_t>(code.size()));
  if (!code_obj) {
    PyErr_Clear();
    PyGILState_Release(gil);
    return "error: bad code string";
  }
  if (PyDict_SetItemString(globals, "_sg_code", code_obj) < 0) {
    Py_DECREF(code_obj);
    PyErr_Clear();
    PyGILState_Release(gil);
    return "error: cannot set _sg_code";
  }
  Py_DECREF(code_obj);

  static const char kHelper[] =
      "import io, sys\n"
      "_buf = io.StringIO()\n"
      "_old = sys.stdout\n"
      "sys.stdout = _buf\n"
      "_err = None\n"
      "_val = None\n"
      "try:\n"
      "    try:\n"
      "        _val = eval(_sg_code, globals())\n"
      "    except SyntaxError:\n"
      "        exec(_sg_code, globals())\n"
      "        _val = None\n"
      "except Exception as e:\n"
      "    _err = e\n"
      "finally:\n"
      "    sys.stdout = _old\n"
      "_sg_out = _buf.getvalue()\n"
      "if _err is not None:\n"
      "    _sg_out += (('' if not _sg_out else '\\n') + "
      "f'{type(_err).__name__}: {_err}')\n"
      "elif _val is not None:\n"
      "    _sg_out += (('' if not _sg_out else '\\n') + repr(_val))\n";

  PyObject* run = PyRun_String(kHelper, Py_file_input, globals, globals);
  std::string out;
  if (!run) {
    if (PyErr_Occurred()) {
      PyObject* ptype = nullptr;
      PyObject* pval = nullptr;
      PyObject* ptb = nullptr;
      PyErr_Fetch(&ptype, &pval, &ptb);
      PyErr_NormalizeException(&ptype, &pval, &ptb);
      PyObject* s = pval ? PyObject_Str(pval) : nullptr;
      const char* msg = s ? PyUnicode_AsUTF8(s) : "python error";
      out = std::string("error: ") + (msg ? msg : "python error");
      Py_XDECREF(s);
      Py_XDECREF(ptype);
      Py_XDECREF(pval);
      Py_XDECREF(ptb);
    } else {
      out = "error: eval failed";
    }
  } else {
    Py_DECREF(run);
    PyObject* out_obj = PyDict_GetItemString(globals, "_sg_out");
    if (out_obj && PyUnicode_Check(out_obj)) {
      const char* utf8 = PyUnicode_AsUTF8(out_obj);
      if (utf8) {
        out = utf8;
      }
    }
  }
  PyDict_DelItemString(globals, "_sg_code");
  PyDict_DelItemString(globals, "_sg_out");
  PyGILState_Release(gil);
  return out;
#else
  (void)code;
  return "error: python not compiled in";
#endif
}

bool Interpreter::exec_source(std::string_view code, std::string* error) {
#if defined(HAS_PYTHON)
  if (!ready_ || !Py_IsInitialized()) {
    if (error) {
      *error = "error: python not ready";
    }
    return false;
  }
  const std::string owned(code);
  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* main_mod = PyImport_AddModule("__main__");
  if (!main_mod) {
    PyGILState_Release(gil);
    if (error) {
      *error = "error: no __main__";
    }
    return false;
  }
  PyObject* globals = PyModule_GetDict(main_mod);
  PyObject* run = PyRun_String(owned.c_str(), Py_file_input, globals, globals);
  if (!run) {
    std::string message = "python error";
    if (PyErr_Occurred()) {
      PyObject* ptype = nullptr;
      PyObject* pval = nullptr;
      PyObject* ptb = nullptr;
      PyErr_Fetch(&ptype, &pval, &ptb);
      PyErr_NormalizeException(&ptype, &pval, &ptb);
      PyObject* s = pval ? PyObject_Str(pval) : nullptr;
      const char* msg = s ? PyUnicode_AsUTF8(s) : nullptr;
      if (msg && msg[0]) {
        message = msg;
      }
      Py_XDECREF(s);
      Py_XDECREF(ptype);
      Py_XDECREF(pval);
      Py_XDECREF(ptb);
    }
    PyGILState_Release(gil);
    if (error) {
      *error = message;
    }
    return false;
  }
  Py_DECREF(run);
  PyGILState_Release(gil);
  return true;
#else
  (void)code;
  if (error) {
    *error = "error: python not compiled in";
  }
  return false;
#endif
}

bool Interpreter::exec_file(std::string_view path, std::string* error) {
  auto fail = [&](const char* message) {
    if (error) {
      *error = message;
    }
    return false;
  };
  if (path.empty()) {
    return fail("error: cannot read script");
  }
#if defined(HAS_PYTHON)
  if (!ready_ || !Py_IsInitialized()) {
    return fail("error: python not ready");
  }
  return run_script_file(std::string(path), error);
#else
  (void)path;
  return fail("error: python not compiled in");
#endif
}

Interpreter& interpreter() {
  static Interpreter instance;
  return instance;
}

}  // namespace py
}  // namespace content
