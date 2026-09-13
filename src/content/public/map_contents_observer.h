// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_MAP_CONTENTS_OBSERVER_H
#define CONTENT_PUBLIC_MAP_CONTENTS_OBSERVER_H

#include <cstdint>

#include "content/public/map_types.h"

namespace content {

// Chrome implements this. MapContents does not include mojo.
class MapContentsObserver {
 public:
  virtual ~MapContentsObserver() = default;
  virtual void OnFrameReady(uint32_t view_id, uint32_t generation) {}
  virtual void OnExtentChanged(uint32_t view_id, const Extent2& e) {}
  virtual void OnRenderDied() {}
};

}  // namespace content

#endif  // CONTENT_PUBLIC_MAP_CONTENTS_OBSERVER_H
