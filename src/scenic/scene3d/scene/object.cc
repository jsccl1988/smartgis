// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// prefers_immediate_context default is inline on Object3d (object.h).
// This TU stays in the source_set so GN paths / future out-of-line helpers
// have a home without reintroducing a DLL-export gap for ui_legacy.

#include "scenic/scene3d/scene/object.h"

namespace scenic {
namespace detail {
namespace {
[[maybe_unused]] constexpr int kObjectCcKeep = 1;
}
}  // namespace detail
}  // namespace scenic
