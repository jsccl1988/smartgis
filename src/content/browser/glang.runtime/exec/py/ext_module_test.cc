// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/ext_module.h"

#include "content/browser/glang.runtime/exec/py/cpython.h"

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

#ifndef PLUGIN_HELLO_EXT_DIR
#define PLUGIN_HELLO_EXT_DIR ""
#endif

namespace {

#if defined(HAS_PYTHON)

bool expect_ping() {
  const char* root = PLUGIN_HELLO_EXT_DIR;
  if (!root || !root[0]) {
    std::fprintf(stderr, "ext_module_test: PLUGIN_HELLO_EXT_DIR empty\n");
    return false;
  }
  if (!content::py::interpreter().init()) {
    std::fprintf(stderr, "ext_module_test: interpreter init failed\n");
    return false;
  }
  if (!content::py::mount_extension("smartgis.sample_hello_ext", root)) {
    std::fprintf(stderr, "ext_module_test: mount failed\n");
    return false;
  }
  PyGILState_STATE gil = PyGILState_Ensure();
  PyObject* mod = PyImport_ImportModule("glang.ext.smartgis.sample_hello_ext");
  if (!mod) {
    PyErr_Print();
    PyGILState_Release(gil);
    std::fprintf(stderr, "ext_module_test: import failed\n");
    return false;
  }
  PyObject* ping = PyObject_GetAttrString(mod, "ping");
  Py_DECREF(mod);
  if (!ping || !PyCallable_Check(ping)) {
    Py_XDECREF(ping);
    PyGILState_Release(gil);
    std::fprintf(stderr, "ext_module_test: ping missing\n");
    return false;
  }
  PyObject* r = PyObject_CallFunction(ping, nullptr);
  Py_DECREF(ping);
  if (!r) {
    PyErr_Print();
    PyGILState_Release(gil);
    return false;
  }
  const char* text = PyUnicode_AsUTF8(r);
  const bool ok = text && std::string(text) == "pong";
  Py_DECREF(r);
  content::py::unmount_extension("smartgis.sample_hello_ext");
  PyGILState_Release(gil);
  content::py::interpreter().shutdown();
  if (!ok) {
    std::fprintf(stderr, "ext_module_test: ping != pong\n");
  }
  return ok;
}

#endif

}  // namespace

int main() {
#if defined(HAS_PYTHON)
  if (!expect_ping()) {
    return 1;
  }
  std::printf("ext_module_test: ok\n");
  return 0;
#else
  // Debug / stub: mount must fail closed without an interpreter.
  if (content::py::mount_extension("smartgis.sample_hello_ext", ".")) {
    std::fprintf(stderr, "ext_module_test: mount unexpectedly ok on stub\n");
    return 1;
  }
  std::printf("ext_module_test: ok\n");
  return 0;
#endif
}
