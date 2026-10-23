// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/tile/verbs.h"

#include "content/browser/glang.runtime/exec/py/plugin_fn.h"
#include "content/browser/glang.runtime/language/code/op.h"

namespace content {
namespace tile {
namespace {

bool eval_registered(content::CapabilityHost&, const content::code::Module&,
                     const content::code::Inst&, content::code::SlotTable&,
                     std::string*) {
  // Script calls are handled by try_script inside eval_script_call. This
  // entry only publishes the name on the dynamic verb table.
  return true;
}

}  // namespace

void install_script_verbs() {
  const char* names[] = {"tile_xyz", "tile_bbox", "tile_resolution", "tile_url",
                         "tile_fetch"};
  for (const char* name : names) {
    content::py::register_fn(name, nullptr, 0, &eval_registered);
  }
}

}  // namespace tile
}  // namespace content
