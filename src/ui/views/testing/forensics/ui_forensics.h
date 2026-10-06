// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TESTING_FORENSICS_UI_FORENSICS_H_
#define UI_VIEWS_TESTING_FORENSICS_UI_FORENSICS_H_

#include <filesystem>
#include <string>
#include <vector>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Failure / optional record dump for UI visual forensics (Mode A).
// Default output root: out/ui_forensics/<run_id>/ (repo-relative when cwd is
// repo root). PNG frames are optional (requires WIC via pixel harness).

struct ForensicsDumpOptions {
  // When empty, use out/ui_forensics/<timestamp_or_label>.
  std::filesystem::path root;
  std::string run_id;
  bool write_png = true;
  int frame_width = 0;   // 0 → use root->bounds().width
  int frame_height = 0;  // 0 → use root->bounds().height
};

struct ForensicsDumpResult {
  bool ok = false;
  std::filesystem::path dir;
  std::string error;
};

// Create run dir, write layout_issues.txt + manifest.json (+ optional PNG).
// |layout_issues| may include sibling-overlap lines. Does not assert.
ForensicsDumpResult dump_ui_forensics(View* root,
                                      const std::vector<std::string>& layout_issues,
                                      const ForensicsDumpOptions& options = {});

// True when UI_FORENSICS is set to a non-empty / non-0 value.
bool ui_forensics_env_forced();

// Resolve default forensics root (out/ui_forensics).
std::filesystem::path ui_forensics_root_dir();

// Pure geometry helper for Perf Gantt lane rows (unit-tested).
// Returns per-lane top y (absolute) and lane height; empty if no room.
struct GanttLaneGeom {
  int lane_top = 0;
  int lane_h = 0;
  int label_left = 0;
  int bar_left = 0;
};
bool compute_gantt_lane_geom(int panel_y,
                             int panel_bottom,
                             int horizon_bottom,
                             int lane_count,
                             GanttLaneGeom* out);

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TESTING_FORENSICS_UI_FORENSICS_H_
