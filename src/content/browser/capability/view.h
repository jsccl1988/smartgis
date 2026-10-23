// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_VIEW_H_
#define CONTENT_BROWSER_CAPABILITY_VIEW_H_

#include <concepts>
#include <cstdint>
#include <string>

#include "content/browser/capability/op.h"

#include "content/public/types.h"

namespace content {

// Snapshot of one view face (edit, data, scene, or a future world view).
// Facts only: the IL runtime decides whether this counts as loaded.
struct ViewLoadStatus {
  std::string face;
  int pane_present = 0;
  int content_view = 0;
  int hwnd_alive = 0;
  int visible = 0;
  int frame_ready = 0;
  int layer_count = 0;
  int orbit_moved = 0;
  int view_x = 0;
  int view_y = 0;
  int view_w = 0;
  int view_h = 0;
  int hwnd_x = 0;
  int hwnd_y = 0;
  int hwnd_w = 0;
  int hwnd_h = 0;
  int menu_h = 0;
  int menu_min_px = 0;
};

// Last committed digitize geometry. Empty |kind| means there is no append.
struct GeomStatus {
  std::string kind;
  int point_count = 0;
  int is_append = 0;
  int committed = 0;
};

// Edit-view scale and camera kind. Facts only.
struct ViewScaleStatus {
  double scale = 0;
  int has_frame = 0;
  int ortho = 0;
};

// Map2d layout readiness. Facts only: IL decides wait_ready pass/fail.
struct MapReadyStatus {
  int map_present = 0;
  int layout_built = 0;
  int layout_build_count = 0;
};

// Edit ToolSession + MemoryEditSession presence. Facts only.
struct EditToolSessionStatus {
  int session_present = 0;
  int workspace = 0;
  int edits = 0;
  int memory_session = 0;
};

// Active tool on the tool stack. Empty |id| when none.
struct ToolStatus {
  std::string id;
};

// View lane. Mirrors GisContents (present) and ToolSession (tool stack, edit
// input). A view is any ViewKind, including a future world viewport.
// vista::World stays the 3D scene the view presents. Load, geometry, scale,
// map-ready, edit-host, and tool checks are not gates: IL registers callbacks
// and reads the snapshots.
struct ViewCapability {
  detail::Op<void()> detach_views;
  detail::Op<void()> stop_present_timers;
  detail::Op<void()> resume_present_timers;
  detail::Op<bool()> invalidate_map2d;

  detail::Op<bool(const InputEvent&)> dispatch_edit_input;
  // ToolSession::release_exclusive. Drops the active exclusive interaction.
  detail::Op<bool()> release_exclusive;
  detail::Op<bool(const std::string& command_id)> run_tool;
  detail::Op<int(const std::string& id)> activate_tool;
  // Fills the last committed geometry. Does not decide pass or fail.
  detail::Op<bool(GeomStatus* out)> last_geom;
  // Fills scale and ortho. Does not dispatch input and does not decide pass
  // or fail. A missing frame is |has_frame| == 0 with a true return.
  detail::Op<bool(ViewScaleStatus* out)> view_scale;
  // |timeout_ms| 0 is a snapshot. A positive value waits for layout_build,
  // then fills |out|. Does not decide pass or fail.
  detail::Op<bool(int timeout_ms, MapReadyStatus* out)> map_ready_status;
  // Fills edit-host presence flags. Does not decide pass or fail.
  detail::Op<bool(EditToolSessionStatus* out)> edit_tool_session_status;
  // Fills the current tool id. Empty id when none. Does not decide pass/fail.
  detail::Op<bool(ToolStatus* out)> tool_status;

  // IL installs these. Bind must not overwrite them. Each matching read
  // notifies the callback with the snapshot.
  detail::Op<void(const std::string& name, const ViewLoadStatus& status)>
      on_app_event;
  detail::Op<void(const std::string& name, const GeomStatus& status)>
      on_geom_event;
  detail::Op<void(const std::string& name, const ViewScaleStatus& status)>
      on_scale_event;
  detail::Op<void(const std::string& name, const MapReadyStatus& status)>
      on_map_ready_event;
  detail::Op<void(const std::string& name, const EditToolSessionStatus& status)>
      on_edit_tool_session_event;
  detail::Op<void(const std::string& name, const ToolStatus& status)>
      on_tool_event;
  // |timeout_ms| 0 is a snapshot. A positive value waits for a presented
  // frame first, then fills |out|. Does not decide pass or fail.
  detail::Op<bool(const std::string& face, int timeout_ms, ViewLoadStatus* out)>
      load_status;

  detail::Op<bool(int count)> browse_stress;
  detail::Op<bool()> fps_bench;
  // Browse still face: "map2d" (software export) or "scene3d" (hypsometric).
  // Known faces return true after the attempt; the still writer records a miss.
  detail::Op<bool(const std::string& face)> capture_browse_still;

  // Scene3D: frame orbit from world-component AABB (best viewport).
  detail::Op<bool()> fit_scene_box;
  // Scene3D camera path: mode "orbit" | "spherical"; |ms| wall; |steps| keys.
  detail::Op<bool(const std::string& mode, int ms, int steps)> camera_fly;

  // Map / Data / 3D tab id ("map2d", "data", "scene3d"). Empty until bound.
  detail::Op<std::string()> present_mode;
  detail::Op<bool(const std::string& mode)> set_present_mode;

  // Console digitize: activate tool on a view id. Unset → try_activate_tool false.
  detail::Op<bool(uint32_t view_id, const std::string& tool_id)> activate_tool_at;
};

template <typename T>
concept view_capability = requires(T& t,
                                   const InputEvent& event,
                                   const std::string& s,
                                   int n,
                                   GeomStatus* geom,
                                   ViewScaleStatus* scale,
                                   MapReadyStatus* ready,
                                   EditToolSessionStatus* edit,
                                   ToolStatus* tool,
                                   ViewLoadStatus* load) {
  { t.detach_views() };
  { t.stop_present_timers() };
  { t.resume_present_timers() };
  { t.invalidate_map2d() } -> std::convertible_to<bool>;
  { t.dispatch_edit_input(event) } -> std::convertible_to<bool>;
  { t.release_exclusive() } -> std::convertible_to<bool>;
  { t.run_tool(s) } -> std::convertible_to<bool>;
  { t.activate_tool(s) } -> std::convertible_to<int>;
  { t.last_geom(geom) } -> std::convertible_to<bool>;
  { t.view_scale(scale) } -> std::convertible_to<bool>;
  { t.map_ready_status(n, ready) } -> std::convertible_to<bool>;
  { t.edit_tool_session_status(edit) } -> std::convertible_to<bool>;
  { t.tool_status(tool) } -> std::convertible_to<bool>;
  { t.load_status(s, n, load) } -> std::convertible_to<bool>;
  { t.browse_stress(n) } -> std::convertible_to<bool>;
  { t.fps_bench() } -> std::convertible_to<bool>;
  { t.capture_browse_still(s) } -> std::convertible_to<bool>;
  { t.fit_scene_box() } -> std::convertible_to<bool>;
  { t.camera_fly(s, n, n) } -> std::convertible_to<bool>;
  { t.present_mode() } -> std::convertible_to<std::string>;
  { t.set_present_mode(s) } -> std::convertible_to<bool>;
  { t.activate_tool_at(static_cast<uint32_t>(n), s) } -> std::convertible_to<bool>;
};

static_assert(view_capability<ViewCapability>);

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_VIEW_H_
