// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/atmosphere/common/progress.h"

#include "app/views/harness/common/mark/mark.h"

namespace app {
namespace detail {

void atmosphere_showcase_mark(const char* step) {
  write_mark(kAtmosphereShowcaseMarkLeaf, step, /*truncate=*/false);
}

}  // namespace detail
}  // namespace app
