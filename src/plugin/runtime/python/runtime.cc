// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/python/runtime.h"

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/catalog/registry.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#if defined(HAS_PYTHON)
#define PY_SSIZE_T_CLEAN
#ifdef _DEBUG
#define PYTHON_RESTORE_DEBUG
#undef _DEBUG
#endif
#include <Python.h>
#include "base/process/switches.h"
#ifdef PYTHON_RESTORE_DEBUG
#define _DEBUG
#undef PYTHON_RESTORE_DEBUG
#endif
#endif

namespace plugin {
#if defined(HAS_PYTHON)
extern "C" PyObject* PyInit_smartgis();
PyObject* make_python_host(content::PluginHost* host);
void bind_python_host(content::PluginHost* host);
void unbind_python_host();
void set_python_load_directory(std::string_view directory);
void drop_python_plugin_holds(std::string_view directory);
#endif

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

bool init_cpython() {
  const std::string home = python_home();
  add_python_dll_dir(home);

  if (PyImport_AppendInittab("smartgis", &PyInit_smartgis) < 0) {
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
    if (!PyStatus_Exception(status)) {
      status =
          PyWideStringList_Append(&config.module_search_paths, zip_w.c_str());
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
    return false;
  }
  register_smartgis_bindings();
  return true;
}

std::map<std::string, PyObject*> g_plugin_modules;

std::string unique_mod_name(const std::string& directory) {
  const std::size_t h = std::hash<std::string>{}(directory);
  char buf[40];
  std::snprintf(buf, sizeof(buf), "sgplugin_%zu", h);
  return buf;
}

void call_module_stop(PyObject* mod) {
  if (!mod || !PyObject_HasAttrString(mod, "stop")) {
    return;
  }
  PyObject* stop_fn = PyObject_GetAttrString(mod, "stop");
  if (!stop_fn) {
    return;
  }
  PyObject* r = PyObject_CallFunctionObjArgs(stop_fn, nullptr);
  Py_XDECREF(r);
  if (PyErr_Occurred()) {
    PyErr_Print();
  }
  Py_DECREF(stop_fn);
}

void unload_plugin_module(const std::string& directory) {
  auto it = g_plugin_modules.find(directory);
  if (it == g_plugin_modules.end()) {
    return;
  }
  call_module_stop(it->second);
  Py_XDECREF(it->second);
  g_plugin_modules.erase(it);
  drop_python_plugin_holds(directory);
}

PyObject* import_entry_module(const std::string& directory,
                              const std::string& file_utf8) {
  PyObject* util = PyImport_ImportModule("importlib.util");
  if (!util) {
    PyErr_Print();
    return nullptr;
  }
  const std::string name = unique_mod_name(directory);
  PyObject* spec = PyObject_CallMethod(util, "spec_from_file_location", "ss",
                                       name.c_str(), file_utf8.c_str());
  if (!spec || spec == Py_None) {
    Py_XDECREF(spec);
    Py_DECREF(util);
    PyErr_Print();
    return nullptr;
  }
  PyObject* mod =
      PyObject_CallMethod(util, "module_from_spec", "O", spec);
  PyObject* loader = mod ? PyObject_GetAttrString(spec, "loader") : nullptr;
  Py_DECREF(spec);
  Py_DECREF(util);
  if (!mod || !loader) {
    Py_XDECREF(mod);
    Py_XDECREF(loader);
    PyErr_Print();
    return nullptr;
  }
  PyObject* exec =
      PyObject_CallMethod(loader, "exec_module", "O", mod);
  Py_DECREF(loader);
  if (!exec) {
    Py_DECREF(mod);
    PyErr_Print();
    return nullptr;
  }
  Py_DECREF(exec);
  return mod;
}

bool run_entry_and_start(const std::string& directory, const std::string& entry,
                         content::PluginHost* host) {
  namespace fs = std::filesystem;
  const fs::path dir(directory);
  const fs::path file = dir / entry;
  if (!fs::exists(file)) {
    return false;
  }

  PyGILState_STATE gil = PyGILState_Ensure();
  set_python_load_directory(directory);
  unload_plugin_module(directory);

  PyObject* sys_path = PySys_GetObject("path");
  if (sys_path) {
    PyObject* p = PyUnicode_FromString(directory.c_str());
    if (p) {
      PyList_Insert(sys_path, 0, p);
      Py_DECREF(p);
    }
  }

  PyObject* mod = import_entry_module(directory, file.string());
  if (!mod) {
    set_python_load_directory({});
    PyGILState_Release(gil);
    return false;
  }
  g_plugin_modules[directory] = mod;

  bind_python_host(host);
  if (!PyObject_HasAttrString(mod, "start")) {
    set_python_load_directory({});
    PyGILState_Release(gil);
    return false;
  }
  PyObject* start_fn = PyObject_GetAttrString(mod, "start");
  if (!start_fn) {
    set_python_load_directory({});
    PyGILState_Release(gil);
    return false;
  }
  PyObject* host_obj = make_python_host(host);
  if (!host_obj) {
    Py_DECREF(start_fn);
    set_python_load_directory({});
    PyGILState_Release(gil);
    return false;
  }
  PyObject* result = PyObject_CallFunctionObjArgs(start_fn, host_obj, nullptr);
  Py_DECREF(host_obj);
  Py_DECREF(start_fn);
  set_python_load_directory({});
  if (!result) {
    PyErr_Print();
    unload_plugin_module(directory);
    PyGILState_Release(gil);
    return false;
  }
  Py_DECREF(result);
  PyGILState_Release(gil);
  return true;
}

void unload_all_plugin_modules() {
  std::vector<std::string> dirs;
  dirs.reserve(g_plugin_modules.size());
  for (const auto& kv : g_plugin_modules) {
    dirs.push_back(kv.first);
  }
  for (const std::string& d : dirs) {
    unload_plugin_module(d);
  }
}

#else  // !HAS_PYTHON

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
#if defined(HAS_PYTHON)
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
#if defined(HAS_PYTHON)
  if (Py_IsInitialized()) {
    PyGILState_STATE gil = PyGILState_Ensure();
    unload_all_plugin_modules();
    unbind_python_host();
    PyGILState_Release(gil);
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
#if defined(HAS_PYTHON)
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
#if defined(HAS_PYTHON)
  if (Py_IsInitialized()) {
    PyGILState_STATE gil = PyGILState_Ensure();
    unload_all_plugin_modules();
    PyGILState_Release(gil);
  }
#endif
}

void PythonRuntime::stop(std::string_view directory) {
#if defined(HAS_PYTHON)
  if (!Py_IsInitialized() || directory.empty()) {
    return;
  }
  PyGILState_STATE gil = PyGILState_Ensure();
  unload_plugin_module(std::string(directory));
  PyGILState_Release(gil);
#else
  (void)directory;
#endif
}

bool PythonRuntime::is_ready() const {
  return ready_;
}

std::string PythonRuntime::eval(std::string_view code) {
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
#if defined(HAS_PYTHON)
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
      [runtime](const PluginRecord& rec) { runtime->stop(rec.directory); });
}

}  // namespace plugin
