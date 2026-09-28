// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_EXPORT_H_
#define BASE_CORE_EXPORT_H_

#include "base/core/build_config.h"

// Single export for //src/base:base (dll_stem = base).
// GN sets BASE_EXPORTS on the source_sets compiled into that DLL.
// Foundation (:foundation) does not set BASE_EXPORTS; do not annotate
// header-only foundation symbols with BASE_EXPORT.

#if defined(BASE_EXPORTS)
#if defined(COMPILER_MSVC) || defined(_WIN32)
#define BASE_EXPORT __declspec(dllexport)
#else
#define BASE_EXPORT __attribute__((visibility("default")))
#endif
#elif defined(COMPILER_MSVC) || defined(_WIN32)
#define BASE_EXPORT __declspec(dllimport)
#else
#define BASE_EXPORT
#endif

#endif  // BASE_CORE_EXPORT_H_
