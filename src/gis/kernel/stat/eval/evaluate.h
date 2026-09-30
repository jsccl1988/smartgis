// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef ALGORITHM_STAT_EVALUATE_H_
#define ALGORITHM_STAT_EVALUATE_H_


#include "gis/gis_export.h"
#ifndef _CRT_DECLARE_NONSTDC_NAMES
#define _CRT_DECLARE_NONSTDC_NAMES 0
#endif

#include "legacy/core/macros/macros.h"
#include "gis/kernel/stat/value/value_set.h"

#include <span>
#include <string_view>

namespace stat {

using UnaryFn = void (*)(std::span<double> values);

// Evaluate an assignment such as `[C]=([A]/8+[B])*9` or a bare expression
// written into `out` when `out` is non-empty. Field names use `[name]`.
GIS_EXPORT long evaluate(std::string_view expression, ValueSet& values);

// Register a unary function visible as `name(...)`. Built-in names win.
GIS_EXPORT long register_function(std::string_view name, UnaryFn fn);

}  // namespace stat


#endif  // ALGORITHM_STAT_EVALUATE_H_
