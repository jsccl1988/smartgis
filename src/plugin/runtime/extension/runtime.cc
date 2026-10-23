// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/runtime/extension/runtime.h"

#include "content/browser/glang.runtime/exec/py/cpython.h"
#include "content/public/plugin_host.h"
#include "content/browser/glang.runtime/exec/py/embed/assemble.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace plugin {
namespace {

// Debug builds do not link CPython. This scan is only the stub gate.
// The embedded interpreter checks that module.main is callable after load.
bool module_source_defines_main(std::string_view source) {
  bool in_triple = false;
  char quote = '\0';
  size_t i = 0;
  while (i < source.size()) {
    const size_t eol = source.find('\n', i);
    const size_t end = eol == std::string_view::npos ? source.size() : eol;
    std::string_view line = source.substr(i, end - i);
    if (!line.empty() && line.back() == '\r') {
      line.remove_suffix(1);
    }
    i = end < source.size() ? end + 1 : source.size();
    if (in_triple) {
      const std::string closer(3, quote);
      if (line.find(closer) != std::string_view::npos) {
        in_triple = false;
      }
      continue;
    }
    size_t col = 0;
    while (col < line.size() && (line[col] == ' ' || line[col] == '\t')) {
      ++col;
    }
    if (col != 0 || col >= line.size()) {
      continue;
    }
    const std::string_view rest = line.substr(col);
    if (!rest.empty() && rest[0] == '#') {
      continue;
    }
    if (rest.starts_with("\"\"\"") || rest.starts_with("'''")) {
      quote = rest[0];
      const std::string marker(3, quote);
      if (rest.find(marker, 3) == std::string_view::npos) {
        in_triple = true;
      }
      continue;
    }
    auto is_main_def = [](std::string_view text) {
      if (!text.starts_with("def main")) {
        return false;
      }
      const std::string_view tail = text.substr(8);
      return !tail.empty() && (tail[0] == '(' || tail[0] == ' ' || tail[0] == '\t');
    };
    if (is_main_def(rest)) {
      return true;
    }
    if (rest.starts_with("async ") && is_main_def(rest.substr(6))) {
      return true;
    }
  }
  return false;
}

bool read_script_file(std::string_view path, std::string* out) {
  if (!out || path.empty()) {
    return false;
  }
  std::ifstream in(std::filesystem::path(std::string(path)), std::ios::binary);
  if (!in) {
    return false;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  *out = ss.str();
  return true;
}

#if defined(HAS_PYTHON)

void attach_embed_after_init() {
  if (!attach_plugin_embed()) {
    // Interpreter stays up; plugin start fails later if Host is missing.
  }
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

void forget_sys_module(PyObject* mod) {
  if (!mod) {
    return;
  }
  PyObject* modules = PyImport_GetModuleDict();
  PyObject* name = PyObject_GetAttrString(mod, "__name__");
  if (!name) {
    PyErr_Clear();
    return;
  }
  if (modules && name) {
    if (PyDict_DelItem(modules, name) < 0) {
      PyErr_Clear();
    }
  }
  Py_XDECREF(name);
}

void unload_plugin_module(const std::string& directory) {
  auto it = g_plugin_modules.find(directory);
  if (it == g_plugin_modules.end()) {
    return;
  }
  PyObject* mod = it->second;
  // Drop callbacks before the module object goes away.
  drop_python_plugin_holds(directory);
  call_module_stop(mod);
  forget_sys_module(mod);
  Py_XDECREF(mod);
  g_plugin_modules.erase(it);
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

#endif

}  // namespace

bool PythonRuntime::init() {
  if (ready_) {
    return true;
  }
#if defined(HAS_PYTHON)
  content::py::InitHooks hooks;
  hooks.after_initialize = &attach_embed_after_init;
  ready_ = content::py::interpreter().init(hooks);
#else
  ready_ = content::py::interpreter().init();
#endif
  return ready_;
}

void PythonRuntime::shutdown() {
#if defined(HAS_PYTHON)
  if (Py_IsInitialized()) {
    PyGILState_STATE gil = PyGILState_Ensure();
    unload_all_plugin_modules();
    unbind_python_host();
    PyGILState_Release(gil);
  }
#endif
  content::py::interpreter().shutdown();
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
  if (!ready_) {
    return "error: python not ready";
  }
  return content::py::interpreter().eval(code);
#else
  (void)code;
  return "error: python not compiled in";
#endif
}

bool PythonRuntime::exec_source(std::string_view code, std::string* error) {
#if defined(HAS_PYTHON)
  if (!ready_) {
    if (error) {
      *error = "error: python not ready";
    }
    return false;
  }
  return content::py::interpreter().exec_source(code, error);
#else
  (void)code;
  if (error) {
    *error = "error: python not compiled in";
  }
  return false;
#endif
}

bool PythonRuntime::exec_file(std::string_view path, std::string* error) {
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
  if (!ready_) {
    return fail("error: python not ready");
  }
  return content::py::interpreter().exec_file(path, error);
#else
  std::string source;
  if (!read_script_file(path, &source)) {
    return fail("error: cannot read script");
  }
  if (!module_source_defines_main(source)) {
    return fail("error: script missing callable main");
  }
  return fail("error: python not compiled in");
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

}  // namespace plugin
