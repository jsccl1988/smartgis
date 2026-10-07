// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/il.runtime/backend/view/view_bind.h"

#include "app/views/browser/browser.h"
#include "app/views/il.runtime/backend/view/camera/camera_fly.h"
#include "app/views/il.runtime/backend/view/camera/fit_scene_box.h"
#include "app/views/il.runtime/backend/view/host/capture_host.h"
#include "app/views/il.runtime/backend/document/document.h"
#include "app/views/il.runtime/backend/view/probe.h"
#include "app/views/il.runtime/backend/horizon/sema/expect.h"
#include "app/views/il.runtime/backend/view/browse/still.h"
#include "app/views/il.runtime/backend/view/browse/stress.h"
#include "app/views/il.runtime/backend/horizon/sema/ui.h"
#include "app/views/il.runtime/bind/slots.h"

namespace app {
namespace detail {

void register_view(Browser& browser,
                   content::CapabilityHost* out,
                   const wchar_t* mark_leaf) {
  Browser* b = &browser;
  const wchar_t* leaf = mark_leaf;
  bind_tagged_slots(
      out, base::tagged_tuple{
               base::tag_resolver<slot_dispatch_edit_input> =
                   [b](const content::InputEvent& e) {
                     return dispatch_edit_input(*b, e);
                   },
               base::tag_resolver<slot_detach_views> =
                   [b]() { detach_views(*b); },
               base::tag_resolver<slot_stop_present_timers> =
                   [b]() { stop_present_timers(*b); },
               base::tag_resolver<slot_resume_present_timers> =
                   [b]() { resume_present_timers(*b); },
               base::tag_resolver<slot_run_tool> =
                   [b](const std::string& command_id) {
                     return b->run_tool_command(command_id);
                   },
               base::tag_resolver<slot_invalidate_map2d> =
                   [b]() { return invalidate_map2d_frame(*b); },
               base::tag_resolver<slot_browse_stress> =
                   [b, leaf](int count) { return browse_stress(*b, leaf, count); },
               base::tag_resolver<slot_capture_browse_still> =
                   [b, leaf](const std::string& face) {
                     if (face == "scene3d") {
                       capture_browse_scene3d(*b);
                       return true;
                     }
                     if (face == "map2d") {
                       (void)capture_browse_map2d(*b, leaf);
                       return true;
                     }
                     return false;
                   },
               base::tag_resolver<slot_fps_bench> =
                   [b]() {
                     run_horizon_map2d_fps_bench(*b);
                     return true;
                   },
               base::tag_resolver<slot_load_status> =
                   [b](const std::string& face, int timeout_ms,
                       content::ViewLoadStatus* status) {
                     return fill_map_load_status(*b, face, timeout_ms, status);
                   },
               base::tag_resolver<slot_map_ready_status> =
                   [b](int timeout_ms, content::MapReadyStatus* status) {
                     return fill_map_ready_status(*b, timeout_ms, status);
                   },
               base::tag_resolver<slot_edit_tool_session_status> =
                   [b](content::EditToolSessionStatus* status) {
                     return fill_edit_tool_session_status(*b, status);
                   },
               base::tag_resolver<slot_tool_status> =
                   [b](content::ToolStatus* status) {
                     return fill_tool_status(*b, status);
                   },
               base::tag_resolver<slot_last_geom> =
                   [b](content::GeomStatus* status) {
                     return fill_last_geom(*b, status);
                   },
               base::tag_resolver<slot_view_scale> =
                   [b](content::ViewScaleStatus* status) {
                     return fill_view_scale(*b, status);
                   },
               base::tag_resolver<slot_activate_tool> =
                   [b](const std::string& id) { return activate_view_tool(*b, id); },
               base::tag_resolver<slot_fit_scene_box> =
                   [b]() { return fit_scene_box(*b); },
               base::tag_resolver<slot_camera_fly> =
                   [b](const std::string& mode, int ms, int steps) {
                     return camera_fly(*b, mode, ms, steps);
                   },
           });
}

}  // namespace detail
}  // namespace app
