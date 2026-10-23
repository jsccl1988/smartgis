// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// One place for the CPython debug-macro dance. Each TU includes this once.

#ifndef PLUGIN_RUNTIME_PYTHON_KERNEL_PY_COMPAT_H_
#define PLUGIN_RUNTIME_PYTHON_KERNEL_PY_COMPAT_H_

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

#endif  // PLUGIN_RUNTIME_PYTHON_KERNEL_PY_COMPAT_H_
