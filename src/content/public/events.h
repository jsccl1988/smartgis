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

// Fired after EditSession::commit succeeds (e.g. draw.* draft → append).
// Chrome status / inspectors subscribe; widgets never hold SmtFeature*.
struct EditCommitted {
  uint32_t view_id = 0;
  FeatureId id{};
  // Mirrors sdb::EditOp without pulling sdb into the event header.
  enum class Op { kAppend = 0, kDelete = 1, kModify = 2 };
  Op op = Op::kAppend;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_EVENTS_H_
