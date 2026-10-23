// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_util.h"

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)

bool retain_callback(PyObject* callable, std::uint64_t* id) {
  if (!id) {
    return false;
  }
  *id = hold_callable(callable);
  if (*id == 0) {
    PyErr_SetString(PyExc_RuntimeError, "glang.plugin could not hold callback");
    return false;
  }
  return true;
}

bool add_module_object(PyObject* parent,
                       const char* name,
                       PyObject* child,
                       PyObject** child_slot) {
  if (!parent || !name || !child) {
    Py_XDECREF(child);
    if (child_slot) {
      *child_slot = nullptr;
    }
    return false;
  }
  if (PyModule_AddObject(parent, name, child) < 0) {
    Py_DECREF(child);
    if (child_slot) {
      *child_slot = nullptr;
    }
    return false;
  }
  if (child_slot) {
    *child_slot = nullptr;
  }
  return true;
}

#endif

}  // namespace detail
}  // namespace plugin
