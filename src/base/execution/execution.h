// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// L5 full-module umbrella: L1 executor through L4 adapters only.
// Does NOT auto-include pipeline/, map_reduce/, or parallel/ capability layer.

#ifndef BASE_EXECUTION_EXECUTION_H
#define BASE_EXECUTION_EXECUTION_H

#include "base/execution/execution_executor.h"
#include "base/execution/futures/combinators/continuation.h"
#include "base/execution/async/sender_receiver.h"
#include "base/execution/execution_adapters.h"

namespace base {
// Lifetime is a placeholder type used in tests to verify executor lifetime
// management.
template <typename Tag>
struct Lifetime {
  Lifetime() = default;
  Lifetime(const Lifetime&) = default;
  Lifetime(Lifetime&&) = default;
  Lifetime& operator=(const Lifetime&) = default;
  Lifetime& operator=(Lifetime&&) = default;
};
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTION_H
