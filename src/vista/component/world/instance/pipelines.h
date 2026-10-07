// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Which Instance kinds need lit PSOs (CPU, no Device).

#ifndef VISTA_COMPONENT_WORLD_INSTANCE_PIPELINES_H_
#define VISTA_COMPONENT_WORLD_INSTANCE_PIPELINES_H_

#include <vector>

#include "vista/component/world/instance.h"

namespace vista {
namespace detail {

bool want_model_lit(const std::vector<Instance>& instances);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_COMPONENT_WORLD_INSTANCE_PIPELINES_H_
