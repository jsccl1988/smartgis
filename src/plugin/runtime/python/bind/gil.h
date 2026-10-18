// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Drop the GIL around GIS kernel and capture calls. The caller must already
// hold the GIL. Python callbacks re-acquire it with PyGILState_Ensure.

#ifndef PLUGIN_RUNTIME_PYTHON_BIND_GIL_H_
#define PLUGIN_RUNTIME_PYTHON_BIND_GIL_H_

#include "plugin/runtime/python/bind/py_compat.h"

namespace plugin {
namespace detail {

#if defined(HAS_PYTHON)

struct ReleaseGil {
  PyThreadState* state = nullptr;

  ReleaseGil() : state(PyEval_SaveThread()) {}
  ~ReleaseGil() {
    if (state) {
      PyEval_RestoreThread(state);
    }
  }

  ReleaseGil(const ReleaseGil&) = delete;
  ReleaseGil& operator=(const ReleaseGil&) = delete;
};

#endif

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_RUNTIME_PYTHON_BIND_GIL_H_
