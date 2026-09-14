// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_EXPORT_H_
#define BASE_CORE_EXPORT_H_

#include "base/core/build_config.h"

// //src/base:foundation is a source_set / static aggregate, not a product DLL.
// COMPONENT_BUILD may later export symbols; until then BASE_EXPORT is empty.

#if defined(COMPONENT_BUILD)
#if defined(COMPILER_MSVC)
#if defined(BASE_IMPLEMENTATION)
#define BASE_EXPORT __declspec(dllexport)
#else
#define BASE_EXPORT __declspec(dllimport)
#endif
#else
#define BASE_EXPORT __attribute__((visibility("default")))
#endif
#else
#define BASE_EXPORT
#endif  // defined(COMPONENT_BUILD)

#endif  // BASE_CORE_EXPORT_H_
