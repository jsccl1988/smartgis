// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/memory/allocation_tracker.h"

namespace base {

AllocationTracker::State& AllocationTracker::state() {
  static State storage;
  return storage;
}

}  // namespace base
