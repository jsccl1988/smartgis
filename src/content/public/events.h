// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_EVENTS_H_
#define CONTENT_PUBLIC_EVENTS_H_

#include <cstdint>
#include <vector>

#include "content/public/map_types.h"

// Domain facts that already happened. Not RPC; not pointer routing.
namespace content {

struct SelectionChanged {
  uint32_t view_id = 0;
  std::vector<FeatureId> ids;
};

struct ExtentChanged {
  uint32_t view_id = 0;
  Extent2 extent{};
};

// Chrome / Workspace asked the GPU process to switch map paint.
// kind: 0 = Track B RHI / GpuScene, 1 = Track A MapLibre.
struct RenderBackendChanged {
  uint32_t view_id = 0;
  uint32_t kind = 0;
};

// Fired after EditSession::commit succeeds (e.g. draw.* draft → append).
// Chrome status / inspectors subscribe; widgets never hold SmtFeature*.
struct EditCommitted {
  uint32_t view_id = 0;
  FeatureId id{};
  // Mirrors gis::EditOp without pulling sdb into the event header.
  enum class Op { kAppend = 0, kDelete = 1, kModify = 2 };
  Op op = Op::kAppend;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_EVENTS_H_
