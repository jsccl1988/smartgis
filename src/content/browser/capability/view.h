// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_CAPABILITY_VIEW_H_
#define CONTENT_BROWSER_CAPABILITY_VIEW_H_

#include <functional>
#include <string>

#include "content/public/map_layer_types.h"

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

// Edit ViewHost + MemoryEditSession presence. Facts only.
struct EditHostStatus {
  int host_present = 0;
  int workspace = 0;
  int edits = 0;
  int memory_session = 0;
};

// Active tool on the tool stack. Empty |id| when none.
struct ToolStatus {
  std::string id;
};

// View lane. Mirrors MapContents (present) and ViewHost (tool stack, edit
// input). A view is any ViewKind, including a future world viewport.
// vista::World stays the 3D scene the view presents. Load, geometry, scale,
// map-ready, edit-host, and tool checks are not gates: IL registers callbacks
// and reads the snapshots.
struct ViewCapability {
  std::function<void()> detach_maps;
  std::function<void()> stop_map_present_timers;
  std::function<void()> resume_map_present_timers;
  std::function<bool()> invalidate_map2d;

  std::function<bool(const InputEvent&)> dispatch_edit_input;
  // ViewHost::release_exclusive. Drops the active exclusive interaction.
  std::function<bool()> release_exclusive;
  std::function<bool(const std::string& command_id)> run_tool;
  std::function<int(const std::string& id)> activate_tool;
  // Fills the last committed geometry. Does not decide pass or fail.
  std::function<bool(GeomStatus* out)> last_geom;
  // Fills scale and ortho. Does not dispatch input and does not decide pass
  // or fail. A missing frame is |has_frame| == 0 with a true return.
  std::function<bool(ViewScaleStatus* out)> view_scale;
  // |timeout_ms| 0 is a snapshot. A positive value waits for layout_build,
  // then fills |out|. Does not decide pass or fail.
  std::function<bool(int timeout_ms, MapReadyStatus* out)> map_ready_status;
  // Fills edit-host presence flags. Does not decide pass or fail.
  std::function<bool(EditHostStatus* out)> edit_host_status;
  // Fills the current tool id. Empty id when none. Does not decide pass/fail.
  std::function<bool(ToolStatus* out)> tool_status;

  // IL installs these. Bind must not overwrite them. Each matching read
  // notifies the callback with the snapshot.
  std::function<void(const std::string& name, const ViewLoadStatus& status)>
      on_app_event;
  std::function<void(const std::string& name, const GeomStatus& status)>
      on_geom_event;
  std::function<void(const std::string& name, const ViewScaleStatus& status)>
      on_scale_event;
  std::function<void(const std::string& name, const MapReadyStatus& status)>
      on_map_ready_event;
  std::function<void(const std::string& name, const EditHostStatus& status)>
      on_edit_host_event;
  std::function<void(const std::string& name, const ToolStatus& status)>
      on_tool_event;
  // |timeout_ms| 0 is a snapshot. A positive value waits for a presented
  // frame first, then fills |out|. Does not decide pass or fail.
  std::function<bool(const std::string& face, int timeout_ms, ViewLoadStatus* out)>
      load_status;

  std::function<bool(int count)> browse_stress;
  std::function<bool()> fps_bench;
  // Browse still face: "map2d" (software export) or "scene3d" (hypsometric).
  // Known faces return true after the attempt; the still writer records a miss.
  std::function<bool(const std::string& face)> capture_browse_still;

  // Scene3D: frame orbit from world-component AABB (best viewport).
  std::function<bool()> fit_scene_box;
  // Scene3D camera path: mode "orbit" | "spherical"; |ms| wall; |steps| keys.
  std::function<bool(const std::string& mode, int ms, int steps)> camera_fly;
};

}  // namespace content

#endif  // CONTENT_BROWSER_CAPABILITY_VIEW_H_
