// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Shared CPython helpers for surface and gis modules.

#ifndef PLUGIN_RUNTIME_PYTHON_KERNEL_PY_UTIL_H_
#define PLUGIN_RUNTIME_PYTHON_KERNEL_PY_UTIL_H_

#include "content/browser/glang.runtime/exec/py/embed/kernel/py_compat.h"
#include "content/browser/glang.runtime/exec/py/embed/kernel/session.h"

#include <cstdint>

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)

inline PyObject* py_bool(bool ok) {
  if (ok) {
    Py_RETURN_TRUE;
  }
  Py_RETURN_FALSE;
}

bool retain_callback(PyObject* callable, std::uint64_t* id);

// Steal |child| into |parent| as |name|. On failure DECREFs |child| and
// returns false (Python error set). On success ownership of |child| is taken
// by the module and |*child_slot| is cleared when provided.
bool add_module_object(PyObject* parent,
                       const char* name,
                       PyObject* child,
                       PyObject** child_slot = nullptr);

#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_KERNEL_PY_UTIL_H_
