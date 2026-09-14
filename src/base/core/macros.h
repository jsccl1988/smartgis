// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CORE_MACROS_H_
#define BASE_CORE_MACROS_H_

#include "base/core/build_config.h"

#include <cassert>
#include <utility>

// Annotate a function indicating the caller must examine the return value.
#undef WARN_UNUSED_RESULT
#if defined(COMPILER_GCC) || defined(__clang__)
#define WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#elif defined(COMPILER_MSVC)
#define WARN_UNUSED_RESULT _Check_return_
#else
#define WARN_UNUSED_RESULT
#endif

#define STRINGIFY_HELPER(X) #X
#define STRINGIFY(X) STRINGIFY_HELPER(X)

#ifndef DISALLOW_COPY_AND_ASSIGN
#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&) = delete;      \
  void operator=(const TypeName&) = delete
#endif  // DISALLOW_COPY_AND_ASSIGN

#define CHECK_SHOULD_NOT_HAVE_EXTRA_MEMBER_VARIABLES(BASE, DERIVE) \
  static_assert(sizeof(DERIVE) == sizeof(BASE),                    \
                #DERIVE " should not have extra member variables")

#define ANONYMOUS_VAR_(var, line) var##line
#define ANONYMOUS_VAR__(var, line) ANONYMOUS_VAR_(var, line)
#if defined(__COUNTER__)
#define ANONYMOUS_VAR(var) ANONYMOUS_VAR__(var, __COUNTER__)
#else
#define ANONYMOUS_VAR(var) ANONYMOUS_VAR__(var, __LINE__)
#endif

#define ALLOW_UNUSED_LOCAL(x) (void)x

#if defined(COMPILER_GCC) || defined(__clang__)
#define LIKELY(x) __builtin_expect(!!(x), 1)
#define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define LIKELY(x) (x)
#define UNLIKELY(x) (x)
#endif

#define UNREACHABLE(x)           \
  do {                           \
    assert(0 && "Unreachable!"); \
    x;                           \
  } while (0)

#define FWD(...) ::std::forward<decltype(__VA_ARGS__)>(__VA_ARGS__)

#if defined(COMPILER_MSVC)
#define FORCE_INLINE __forceinline
#elif defined(COMPILER_GCC) || defined(__clang__)
#define FORCE_INLINE inline __attribute__((always_inline))
#else
#define FORCE_INLINE inline
#endif

#endif  // BASE_CORE_MACROS_H_
