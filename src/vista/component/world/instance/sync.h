// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// World node ->Instance CPU copy (no RHI).

#ifndef VISTA_COMPONENT_WORLD_INSTANCE_SYNC_H_
#define VISTA_COMPONENT_WORLD_INSTANCE_SYNC_H_

#include <vector>

#include "vista/component/world/instance.h"
#include "vista/component/world/world.h"

namespace vista {
namespace detail {

void copy_world_instances(const vista::World& world,
                          std::vector<Instance>* out);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_INSTANCE_SYNC_H_
