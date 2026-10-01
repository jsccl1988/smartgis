// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/runtime.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/registry/registry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdlib>
#include <filesystem>
#include <string>

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
#if defined(SMT_HAS_PYTHON)
extern "C" PyObject* PyInit_smartgis();
PyObject* make_python_host(content::PluginHost* host);
void bind_python_host(content::PluginHost* host);
void unbind_python_host();
#endif

namespace {

#if defined(SMT_HAS_PYTHON)
#ifndef SMT_PYTHON_HOME
#define SMT_PYTHON_HOME ""
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
  const char* env = std::getenv("SMT_PYTHON_ROOT");
  if (env && env[0]) {
    return env;
  }
  if (SMT_PYTHON_HOME[0]) {
    return SMT_PYTHON_HOME;
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

bool init_cpython() {
  const std::string home = python_home();
  add_python_dll_dir(home);

  if (PyImport_AppendInittab("smartgis", &PyInit_smartgis) < 0) {
    return false;
  }

  if (!home.empty()) {
    static std::wstring home_w;
    home_w = utf8_to_wide(home);
    Py_SetPythonHome(home_w.c_str());
  }
  Py_InitializeEx(0);
  if (!Py_IsInitialized()) {
    return false;
  }
  register_smartgis_bindings();
  return true;
}

PyObject* g_plugin_module = nullptr;

std::string module_name_from_entry(const std::string& entry) {
  std::string name = entry;
  const size_t slash = name.find_last_of("/\\");
  if (slash != std::string::npos) {
    name = name.substr(slash + 1);
  }
  if (name.size() > 3 && name.substr(name.size() - 3) == ".py") {
    name.resize(name.size() - 3);
  }
  return name.empty() ? "plugin" : name;
}

bool run_entry_and_start(const std::string& directory, const std::string& entry,
                         content::PluginHost* host) {
  namespace fs = std::filesystem;
  const fs::path dir(directory);
  const fs::path file = dir / entry;
  if (!fs::exists(file)) {
    return false;
  }

  PyObject* sys_path = PySys_GetObject("path");
  if (sys_path) {
    PyObject* p = PyUnicode_FromString(directory.c_str());
    if (p) {
      PyList_Insert(sys_path, 0, p);
      Py_DECREF(p);
    }
  }

  const std::string mod_name = module_name_from_entry(entry);
  PyObject* modules = PyImport_GetModuleDict();
  if (modules && PyDict_GetItemString(modules, mod_name.c_str())) {
    PyDict_DelItemString(modules, mod_name.c_str());
  }

  PyObject* mod = PyImport_ImportModule(mod_name.c_str());
  if (!mod) {
    PyErr_Print();
    return false;
  }
  Py_XDECREF(g_plugin_module);
  g_plugin_module = mod;

  bind_python_host(host);
  if (!PyObject_HasAttrString(mod, "start")) {
    return false;
  }
  PyObject* start_fn = PyObject_GetAttrString(mod, "start");
  if (!start_fn) {
    return false;
  }
  PyObject* host_obj = make_python_host(host);
  if (!host_obj) {
    Py_DECREF(start_fn);
    return false;
  }
  PyObject* result = PyObject_CallFunctionObjArgs(start_fn, host_obj, nullptr);
  Py_DECREF(host_obj);
  Py_DECREF(start_fn);
  if (!result) {
    PyErr_Print();
    return false;
  }
  Py_DECREF(result);
  return true;
}

void call_stop_if_any() {
  if (g_plugin_module && PyObject_HasAttrString(g_plugin_module, "stop")) {
    PyObject* stop_fn = PyObject_GetAttrString(g_plugin_module, "stop");
    if (stop_fn) {
      PyObject* r = PyObject_CallFunctionObjArgs(stop_fn, nullptr);
      Py_XDECREF(r);
      if (PyErr_Occurred()) {
        PyErr_Print();
      }
      Py_DECREF(stop_fn);
    }
  }
  Py_XDECREF(g_plugin_module);
  g_plugin_module = nullptr;
  unbind_python_host();
}

#else  // !SMT_HAS_PYTHON

bool python_dll_present() {
  HMODULE m = LoadLibraryW(L"python312.dll");
  if (!m) {
    return false;
  }
  FreeLibrary(m);
  return true;
}

#endif

}  // namespace

bool PythonRuntime::init() {
#if defined(SMT_HAS_PYTHON)
  if (ready_) {
    return true;
  }
  ready_ = init_cpython();
  return ready_;
#else
  ready_ = python_dll_present();
  return ready_;
#endif
}

void PythonRuntime::shutdown() {
#if defined(SMT_HAS_PYTHON)
  if (Py_IsInitialized()) {
    call_stop_if_any();
    Py_FinalizeEx();
  }
#endif
  ready_ = false;
}

bool PythonRuntime::start(std::string_view directory, std::string_view entry,
                          content::PluginHost* host) {
  if (!ready_ || !host || directory.empty() || entry.empty()) {
    return false;
  }
#if defined(SMT_HAS_PYTHON)
  try {
    return run_entry_and_start(std::string(directory), std::string(entry), host);
  } catch (...) {
    return false;
  }
#else
  (void)directory;
  (void)entry;
  (void)host;
  return false;
#endif
}

void PythonRuntime::stop() {
#if defined(SMT_HAS_PYTHON)
  if (Py_IsInitialized()) {
    call_stop_if_any();
  }
#endif
}

bool PythonRuntime::is_ready() const {
  return ready_;
}

std::string PythonRuntime::eval(std::string_view code) {
#if defined(SMT_HAS_PYTHON)
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
      PyObject *ptype = nullptr, *pval = nullptr, *ptb = nullptr;
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

void PythonRuntime::bind_host(content::PluginHost* host) {
#if defined(SMT_HAS_PYTHON)
  if (!ready_ || !Py_IsInitialized()) {
    return;
  }
  bind_python_host(host);
#else
  (void)host;
#endif
}

void bind_registry_python(Registry* registry, PythonRuntime* runtime) {
  if (!registry || !runtime) {
    return;
  }
  registry->set_python_starter(
      [runtime](const PluginRecord& rec, content::PluginHost* host) {
        return runtime->start(rec.directory, rec.manifest.entry, host);
      },
      [runtime](const PluginRecord&) { runtime->stop(); });
}

}  // namespace plugin
