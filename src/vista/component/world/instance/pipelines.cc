// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/instance/pipelines.h"

namespace vista {
namespace detail {

bool want_model_lit(const std::vector<Instance>& instances) {
  for (const Instance& inst : instances) {
    if (inst.kind == vista::NodeKind::kModel ||
        inst.kind == vista::NodeKind::kTileset) {
      return true;
    }
  }
  return false;
}

}  // namespace detail
}  // namespace vista
