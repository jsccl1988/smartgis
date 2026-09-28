// Copyright (c) 2024 The Mogu Authors.
// All rights reserved.

#ifndef BASE_CONTAINER_CONCEPT_H
#define BASE_CONTAINER_CONCEPT_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <string>
#include <type_traits>
#include <utility>

#include "base/traits/concept.h"
#include "base/traits/is_detected.h"
#include "base/traits/type_traits.h"
#include "base/util/type_utils.h"

namespace base {
template <typename T>
concept Container = is_iterable_v<T>;
}  // namespace base
#endif  // BASE_CONTAINER_CONCEPT_H
