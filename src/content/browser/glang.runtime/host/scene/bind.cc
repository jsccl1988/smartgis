// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/scene/bind.h"

#include <string>

#include "content/browser/glang.runtime/host/bind/slots.h"
#include "content/browser/glang.runtime/host/document/document.h"
#include "content/browser/glang.runtime/host/horizon/sema/ui.h"
#include "content/browser/glang.runtime/host/scene/browse/dispatch.h"
#include "content/browser/glang.runtime/host/scene/verbs.h"
#include "content/browser/debug/browse/still.h"
#include "content/browser/debug/browse/stress.h"
#include "content/browser/camera/camera_fly.h"
#include "content/browser/camera/fit_scene_box.h"

namespace content {
namespace detail {

void register_scene_slots(content::Shell& browser,
                          content::CapabilityHost* out,
                          const wchar_t* mark_leaf) {
  code::install_scene_verb_table();
  register_browse_still_faces();
  content::Shell* b = &browser;
  const wchar_t* leaf = mark_leaf;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_invalidate_map2d> =
                   [b]() { return invalidate_map2d_frame(*b); },
               base::tag_resolver<slot_browse_stress> =
                   [b, leaf](int count) { return browse_stress(*b, leaf, count); },
               base::tag_resolver<slot_capture_browse_still> =
                   [b, leaf](const std::string& face) {
                     return capture_still_face(*b, leaf, face);
                   },
               base::tag_resolver<slot_fps_bench> =
                   [b]() {
                     run_horizon_map2d_fps_bench(*b);
                     return true;
                   },
               base::tag_resolver<slot_fit_scene_box> =
                   [b]() { return fit_scene_box(*b); },
               base::tag_resolver<slot_camera_fly> =
                   [b](const std::string& mode, int ms, int steps) {
                     return camera_fly(*b, mode, ms, steps);
                   },
           });
}

}  // namespace detail
}  // namespace content
