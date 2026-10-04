// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/testing/forensics/ui_forensics.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <format>
#include <fstream>
#include <sstream>
#include <system_error>

#include "ui/views/kernel/layout/layout_check.h"
#include "ui/views/testing/pixel/pixel_harness.h"
#include "base/process/switches.h"

namespace ui {
namespace views {
namespace {

std::string make_run_id() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  const auto ms =
      std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
  return std::format("run_{}", ms);
}

void append_views_json(const View* v, int parent_id, int* next_id,
                       std::ostringstream* oss, bool* first) {
  if (!v || !v->is_visible() || !oss || !next_id) {
    return;
  }
  const int id = (*next_id)++;
  const Rect& b = v->bounds();
  if (!*first) {
    *oss << ',';
  }
  *first = false;
  *oss << "{\"id\":" << id << ",\"parent_id\":" << parent_id << ",\"role\":\""
       << v->paint_role() << "\",\"x\":" << b.x << ",\"y\":" << b.y
       << ",\"w\":" << b.width << ",\"h\":" << b.height << '}';
  for (size_t i = 0; i < v->child_count(); ++i) {
    append_views_json(v->child_at(i), id, next_id, oss, first);
  }
}

}  // namespace

bool ui_forensics_env_forced() {
  const char* v = base::switch_cstr("ui-forensics");
  if (!v || !*v) {
    return false;
  }
  return !(v[0] == '0' && v[1] == '\0');
}

std::filesystem::path ui_forensics_root_dir() {
  if (const char* env = base::switch_cstr("ui-forensics-dir")) {
    if (env[0]) {
      return std::filesystem::path(env);
    }
  }
  return std::filesystem::path("out") / "ui_forensics";
}

bool compute_gantt_lane_geom(int panel_y,
                             int panel_bottom,
                             int chrome_bottom,
                             int lane_count,
                             GanttLaneGeom* out) {
  if (!out || lane_count <= 0) {
    return false;
  }
  const int lane_top = (std::max)(panel_y, chrome_bottom);
  const int lane_bottom = panel_bottom - 6;
  if (lane_bottom <= lane_top + 8) {
    return false;
  }
  const int lane_h =
      (std::max)(14, (lane_bottom - lane_top) / (std::max)(1, lane_count));
  out->lane_top = lane_top;
  out->lane_h = lane_h;
  out->label_left = 6;
  out->bar_left = 88;
  return true;
}

ForensicsDumpResult dump_ui_forensics(
    View* root,
    const std::vector<std::string>& layout_issues,
    const ForensicsDumpOptions& options) {
  ForensicsDumpResult result;
  if (!root) {
    result.error = "null root";
    return result;
  }
  const std::string run_id =
      options.run_id.empty() ? make_run_id() : options.run_id;
  const std::filesystem::path root_dir =
      options.root.empty() ? ui_forensics_root_dir() : options.root;
  result.dir = root_dir / run_id;
  std::error_code ec;
  std::filesystem::create_directories(result.dir, ec);
  if (ec) {
    result.error = "mkdir failed";
    return result;
  }

  if (!write_layout_issues_file(result.dir / "layout_issues.txt",
                                layout_issues)) {
    result.error = "write layout_issues failed";
    return result;
  }

  int next_id = 1;
  bool first = true;
  std::ostringstream views_json;
  views_json << '[';
  append_views_json(root, 0, &next_id, &views_json, &first);
  views_json << ']';

  {
    std::ofstream man(result.dir / "manifest.json", std::ios::binary);
    if (!man) {
      result.error = "write manifest failed";
      return result;
    }
    man << "{\n  \"run_id\": \"" << run_id << "\",\n  \"issue_count\": "
        << layout_issues.size() << ",\n  \"views\": " << views_json.str()
        << "\n}\n";
  }

  if (options.write_png) {
    int w = options.frame_width > 0 ? options.frame_width : root->bounds().width;
    int h =
        options.frame_height > 0 ? options.frame_height : root->bounds().height;
    if (w <= 0) {
      w = 800;
    }
    if (h <= 0) {
      h = 600;
    }
    PixelBuffer shot = capture_view(root, w, h);
    if (!shot.bgra.empty()) {
      std::string err;
      if (!write_png_bgra(result.dir / "frame_0000.png", shot, &err)) {
        std::ofstream note(result.dir / "frame_0000.note.txt",
                           std::ios::binary);
        if (note) {
          note << "png_write_failed: " << err << '\n';
        }
      }
    }
  }

  result.ok = true;
  return result;
}

}  // namespace views
}  // namespace ui
