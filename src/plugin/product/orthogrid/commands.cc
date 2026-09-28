// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid/commands.h"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/public/plugin_host.h"
#include "plugin/product/orthogrid/detail/boundary_solve.h"
#include "plugin/runtime/host/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/shell/file_picker.h"
#include "ui/views/dialogs/shell/message_box.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.baogrid";
constexpr const char* kMenuId = "tools.baogrid";
constexpr const char* kEditAppendLinestring = "edit.append.linestring";

struct StoredEdge {
  int flag = 0;
  std::vector<std::pair<double, double>> pts;
};

std::vector<StoredEdge> g_edges;
int g_armed_flag = -1;

void arm_grid_boundary_store(int flag) { g_armed_flag = flag; }

bool grid_boundary_armed_store() { return g_armed_flag >= 0; }

bool note_grid_boundary_store(const double* xy, size_t count) {
  if (g_armed_flag < 0 || !xy || count < 2) {
    return false;
  }
  StoredEdge edge;
  edge.flag = g_armed_flag;
  edge.pts.reserve(count);
  for (size_t i = 0; i < count; ++i) {
    edge.pts.push_back({xy[i * 2], xy[i * 2 + 1]});
  }
  g_armed_flag = -1;
  for (StoredEdge& existing : g_edges) {
    if (existing.flag == edge.flag) {
      existing = std::move(edge);
      return true;
    }
  }
  g_edges.push_back(std::move(edge));
  return true;
}

bool write_gridbnd(const std::string& path) {
  if (path.empty() || g_edges.empty()) {
    return false;
  }
  int nx = 3;
  int ny = 3;
  for (const StoredEdge& edge : g_edges) {
    const int n = static_cast<int>(edge.pts.size());
    if (edge.flag == 0 || edge.flag == 2) {
      nx = std::max(nx, std::max(n, 3));
    } else {
      ny = std::max(ny, std::max(n, 3));
    }
  }
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    return false;
  }
  out << "gridbnd:\n" << nx << " " << ny << "\nbegin\nmain_begin\n";
  for (const StoredEdge& edge : g_edges) {
    const int n = static_cast<int>(edge.pts.size());
    out << n << "\n";
    out << "0 " << (n - 1) << " 0 " << edge.flag << " 1 1\n";
    for (const auto& pt : edge.pts) {
      out << pt.first << "," << pt.second << "\n";
    }
  }
  out << "main_end\nend\n";
  return static_cast<bool>(out);
}

bool json_get_string(std::string_view json,
                     const char* key,
                     std::string* out) {
  if (!out || !key || json.empty()) {
    return false;
  }
  const std::string needle = std::string("\"") + key + "\":\"";
  const size_t pos = json.find(needle);
  if (pos == std::string_view::npos) {
    return false;
  }
  const size_t start = pos + needle.size();
  const size_t end = json.find('"', start);
  if (end == std::string_view::npos || end < start) {
    return false;
  }
  *out = std::string(json.substr(start, end - start));
  return !out->empty();
}


bool workspace_has_linestring_tool(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  tool::CommandCatalog* catalog = host->commands();
  return catalog && catalog->contains(kEditAppendLinestring);
}

bool input_boundary(content::PluginHost* host,
                    const tool::CommandArgs& args,
                    std::string_view boundary_tag) {
  if (!host) {
    return false;
  }
  // Leftover Notify digitizes via GTT_InputLine. New shell without the
  // leftover DLL attached uses the workspace linestring command.
  if (!workspace_has_linestring_tool(host)) {
    return false;
  }
  tool::CommandArgs line_args = args;
  if (boundary_tag == "boundary_0") {
    arm_grid_boundary_store(0);
  } else if (boundary_tag == "boundary_2") {
    arm_grid_boundary_store(2);
  }
  if (boundary_tag.empty()) {
    return host->execute(kEditAppendLinestring, line_args);
  }
  const std::string payload(boundary_tag);
  line_args.payload = payload;
  return host->execute(kEditAppendLinestring, line_args);
}

bool handle_input_boundary_0(content::PluginHost* host,
                             const tool::CommandArgs& args) {
  return input_boundary(host, args, "boundary_0");
}

bool handle_input_boundary_2(content::PluginHost* host,
                             const tool::CommandArgs& args) {
  return input_boundary(host, args, "boundary_2");
}

bool handle_save_boundary(content::PluginHost* host, const tool::CommandArgs&) {
  if (!host) {
    return false;
  }
  // Views path has no persistent Orthogrid session yet; refuse without crash.
  const ui::views::FilePickerResult picked = ui::views::pick_save_file(
      L"Text Files\0*.txt\0All Files\0*.*\0");
  if (!picked.accepted || picked.path.empty()) {
    return false;
  }
  if (!write_gridbnd(picked.path)) {
    set_operation_result("{\"error\":\"no_boundary\"}");
    ui::views::show_message_box(
        ui::views::MessageBoxKind::kError,
        "No boundary has been digitized to save.");
    return false;
  }
  set_operation_result("{\"ok\":true,\"op\":\"baogrid.save_boundary\"}");
  return true;
}

bool handle_load_boundary(content::PluginHost* host, const tool::CommandArgs&) {
  if (!host) {
    return false;
  }
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Text Files\0*.txt\0All Files\0*.*\0");
  if (!picked.accepted || picked.path.empty()) {
    return false;
  }
  const std::string json = std::string("{\"path\":\"") + picked.path + "\"}";
  return host->run_processing("baogrid.create_orth_grid", json);
}

// Load gridbnd and solve Dirichlet Laplace on the file's boundary nodes.
// The 2010 Orthogrid class stays in the legacy DLL.
bool create_orth_grid_processing(content::PluginHost*,
                                 std::string_view args_json) {
  std::string path;
  if (!json_get_string(args_json, "path", &path)) {
    set_operation_result("{\"error\":\"bad_args\"}");
    return false;
  }
  const detail::BoundarySolve solved = detail::solve_grid_boundary_file(path);
  set_operation_result(solved.message);
  return solved.ok;
}

bool contribute(content::PluginHost* host,
                std::string_view command_id,
                std::string_view title,
                tool::CommandHandler handler) {
  if (!host || command_id.empty() || !handler) {
    return false;
  }
  return host->contribute_command(kPluginId, command_id, title, kMenuId,
                                  std::move(handler));
}

}  // namespace

void arm_grid_boundary(int flag) { arm_grid_boundary_store(flag); }

bool grid_boundary_armed() { return grid_boundary_armed_store(); }

bool note_grid_boundary(const double* xy, size_t count) {
  return note_grid_boundary_store(xy, count);
}

bool register_orthogrid(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  // Stable plugin id is smartgis.baogrid; command ids stay baogrid.* with
  // orthogrid.* aliases for leftover AM / older Views callers.
  if (!contribute(host, "baogrid.input_boundary_0", "Input boundary 0",
                  [host](const tool::CommandArgs& args) {
                    return handle_input_boundary_0(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "baogrid.input_boundary_2", "Input boundary 2",
                  [host](const tool::CommandArgs& args) {
                    return handle_input_boundary_2(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "baogrid.save_boundary", "Save grid boundary",
                  [host](const tool::CommandArgs& args) {
                    return handle_save_boundary(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "baogrid.load_boundary", "Load grid boundary",
                  [host](const tool::CommandArgs& args) {
                    return handle_load_boundary(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "orthogrid.input_boundary_0", "Input boundary 0",
                  [host](const tool::CommandArgs& args) {
                    return handle_input_boundary_0(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "orthogrid.input_boundary_2", "Input boundary 2",
                  [host](const tool::CommandArgs& args) {
                    return handle_input_boundary_2(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "orthogrid.save_boundary", "Save grid boundary",
                  [host](const tool::CommandArgs& args) {
                    return handle_save_boundary(host, args);
                  })) {
    return false;
  }
  if (!contribute(host, "orthogrid.load_boundary", "Load grid boundary",
                  [host](const tool::CommandArgs& args) {
                    return handle_load_boundary(host, args);
                  })) {
    return false;
  }

  const content::ProcessingContribution proc = {"baogrid.create_orth_grid",
                                                "Create orth grid"};
  if (!host->contribute_processing(kPluginId, proc,
                                   create_orth_grid_processing)) {
    return false;
  }
  const content::ProcessingContribution leftover_proc = {
      "orthogrid.create_orth_grid", "Create orth grid"};
  if (!host->contribute_processing(kPluginId, leftover_proc,
                                   create_orth_grid_processing)) {
    return false;
  }

  return true;
}

}  // namespace plugin
