// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_STAT_EVALUATE_H_
#define ALGORITHM_STAT_EVALUATE_H_

#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#include "base/core/core.h"
#include "algorithm/stat/value_set.h"

#include <span>
#include <string_view>

#if defined(STAT_EXPORTS)
#define STAT_EXPORT_API __declspec(dllexport)
#else
#define STAT_EXPORT_API __declspec(dllimport)
#endif

namespace stat {

using UnaryFn = void (*)(std::span<double> values);

// Evaluate an assignment such as `[C]=([A]/8+[B])*9` or a bare expression
// written into `out` when `out` is non-empty. Field names use `[name]`.
STAT_EXPORT_API long evaluate(std::string_view expression, ValueSet& values);

// Register a unary function visible as `name(...)`. Built-in names win.
STAT_EXPORT_API long register_function(std::string_view name, UnaryFn fn);

}  // namespace stat

#if !defined(STAT_EXPORTS)
#if defined(_DEBUG)
#pragma comment(lib, "algorithm_d.lib")
#else
#pragma comment(lib, "algorithm.lib")
#endif
#endif

#endif  // ALGORITHM_STAT_EVALUATE_H_
