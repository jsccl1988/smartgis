// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/gate.h"

#include <string_view>

namespace plugin {
namespace {

// Showcase mode names. "none" is the unknown mode, not a look preset.
const char* showcase_mode_name(int mode) {
  switch (mode) {
    case 1:
      return "land";
    case 2:
      return "ocean";
    case 3:
      return "full";
    case 4:
      return "coast";
    case 5:
      return "legacy";
    case 6:
      return "globe";
    default:
      return "none";
  }
}

bool name_is(int mode, const char* expected) {
  return std::string_view(showcase_mode_name(mode)) == expected;
}

}  // namespace

bool world3d_gate_contract(void*) {
  return name_is(1, "land") && name_is(2, "ocean") && name_is(3, "full") &&
         name_is(4, "coast") && name_is(5, "legacy") && name_is(6, "globe") &&
         name_is(0, "none");
}

bool world3d_gate_compute(void*) {
  return std::string_view(showcase_mode_name(1)) != showcase_mode_name(2) &&
         std::string_view(showcase_mode_name(6)) != showcase_mode_name(0);
}

}  // namespace plugin
