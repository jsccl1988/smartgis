// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_COMPONENT_EXPORT_H_
#define BASE_CORE_COMPONENT_EXPORT_H_

#include "base/core/build_config.h"

// Chromium-style component export (see docs/superpowers/specs/
// 2026-09-14-base-root-hybrid-design.md §DLL / STL trust model).
//
// Annotate symbols owned by a product DLL / static component:
//
//   #include "base/core/component_export.h"
//   class COMPONENT_EXPORT(GIS) Layer { ... };
//
// The corresponding GN target must define IS_<NAME>_IMPL=1 (e.g.
// IS_GIS_IMPL) on sources compiled into that component. Prefer this for
// new modules; existing *_EXPORT macros remain valid.
//
// When is_component_build=false, COMPONENT_BUILD is unset and these
// macros expand to nothing (static link into the exe).

#if defined(COMPONENT_BUILD)
#if defined(COMPILER_MSVC) || defined(_WIN32)
#define COMPONENT_EXPORT_ANNOTATION __declspec(dllexport)
#define COMPONENT_IMPORT_ANNOTATION __declspec(dllimport)
#else
#define COMPONENT_EXPORT_ANNOTATION __attribute__((visibility("default")))
#define COMPONENT_IMPORT_ANNOTATION __attribute__((visibility("default")))
#endif
#else
#define COMPONENT_EXPORT_ANNOTATION
#define COMPONENT_IMPORT_ANNOTATION
#endif

#define COMPONENT_MACRO_CONDITIONAL_COMMA_IMPL_(x, ...) \
  COMPONENT_MACRO_CONDITIONAL_COMMA_##x##_
#define COMPONENT_MACRO_CONDITIONAL_COMMA_(...) \
  COMPONENT_MACRO_CONDITIONAL_COMMA_IMPL_(__VA_ARGS__, )
#define COMPONENT_MACRO_CONDITIONAL_COMMA_1_ ,

#define COMPONENT_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(a, b, c, ...) c
#define COMPONENT_MACRO_SELECT_THIRD_ARGUMENT_(...) \
  COMPONENT_MACRO_SELECT_THIRD_ARGUMENT_IMPL_(__VA_ARGS__)

#define COMPONENT_MACRO_CONDITIONAL_(condition, consequent, alternate) \
  COMPONENT_MACRO_SELECT_THIRD_ARGUMENT_(                              \
      COMPONENT_MACRO_CONDITIONAL_COMMA_(condition), consequent, alternate)

#define COMPONENT_EXPORT(component)                 \
  COMPONENT_MACRO_CONDITIONAL_(IS_##component##_IMPL, \
                               COMPONENT_EXPORT_ANNOTATION, \
                               COMPONENT_IMPORT_ANNOTATION)

#define INSIDE_COMPONENT_IMPL(component) \
  COMPONENT_MACRO_CONDITIONAL_(IS_##component##_IMPL, 1, 0)

#endif  // BASE_CORE_COMPONENT_EXPORT_H_
