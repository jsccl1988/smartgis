// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid/commands.h"

#include <algorithm>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "content/public/plugin_host.h"
#include "plugin/product/orthogrid/detail/boundary_solve.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.baogrid";
constexpr const char* kMenuId = "tools.baogrid";
constexpr const char* kEditAppendLinestring = "edit.append.linestring";
constexpr int kAutoMaxNodes = 64 * 64;

OrthogridMeshWriter g_mesh_writer;
int g_elliptic_iters = 0;

struct StoredEdge {
  int flag = 0;
  std::vector<std::pair<double, double>> pts;
};

std::vector<StoredEdge> g_edges;
int g_armed_flag = -1;

void arm_grid_boundary_store(int flag) { g_armed_flag = flag; }

bool grid_boundary_armed_store() { return g_armed_flag >= 0; }

bool edges_complete() {
  bool seen[4] = {};
  for (const StoredEdge& e : g_edges) {
    if (e.flag >= 0 && e.flag <= 3) {
      seen[e.flag] = true;
    }
  }
  return seen[0] && seen[1] && seen[2] && seen[3];
}

int estimate_nodes() {
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
  return nx * ny;
}

bool commit_solved(const detail::BoundarySolve& solved) {
  if (!solved.ok) {
    return false;
  }
  if (!g_mesh_writer) {
    return true;
  }
  OrthogridMeshCommit commit;
  commit.nx = solved.nx;
  commit.ny = solved.ny;
  commit.xs = solved.xs.data();
  commit.ys = solved.ys.data();
  if (!solved.cell_orth.empty() &&
      solved.cell_orth.size() ==
          static_cast<size_t>((solved.nx - 1) * (solved.ny - 1))) {
    commit.cell_orth = solved.cell_orth.data();
  }
  if (!solved.raster_orth.empty() && solved.raster_w > 0 &&
      solved.raster_h > 0 &&
      solved.raster_orth.size() ==
          static_cast<size_t>(solved.raster_w * solved.raster_h)) {
    commit.raster_w = solved.raster_w;
    commit.raster_h = solved.raster_h;
    commit.raster_min_x = solved.raster_min_x;
    commit.raster_min_y = solved.raster_min_y;
    commit.raster_max_x = solved.raster_max_x;
    commit.raster_max_y = solved.raster_max_y;
    commit.raster_orth = solved.raster_orth.data();
  }
  // Intermediate elliptic frames for AnalysisPlayback (final mesh still last).
  commit.frame_count = static_cast<int>(solved.frame_xs.size());
  commit.frame_xs = solved.frame_xs.empty() ? nullptr : solved.frame_xs.data();
  commit.frame_ys = solved.frame_ys.empty() ? nullptr : solved.frame_ys.data();
  return g_mesh_writer(commit);
}

detail::BoundarySolve solve_session() {
  std::vector<detail::BoundaryEdge> edges;
  edges.reserve(g_edges.size());
  for (const StoredEdge& e : g_edges) {
    detail::BoundaryEdge be;
    be.flag = e.flag;
    be.pts = e.pts;
    edges.push_back(std::move(be));
  }
  return detail::solve_grid_boundary(edges, 0, 0, g_elliptic_iters);
}

bool maybe_auto_generate() {
  if (!edges_complete() || g_elliptic_iters != 0 ||
      estimate_nodes() > kAutoMaxNodes) {
    return false;
  }
  const detail::BoundarySolve solved = solve_session();
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  if (!commit_solved(solved)) {
    set_operation_result("{\"error\":\"mesh_commit_failed\"}");
    return false;
  }
  set_operation_result(solved.message);
  return true;
}

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
      (void)maybe_auto_generate();
      return true;
    }
  }
  g_edges.push_back(std::move(edge));
  (void)maybe_auto_generate();
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
    int start = 0;
    int end = 0;
    int index = 0;
    if (edge.flag == 0) {
      start = 0;
      end = nx - 1;
      index = 0;
    } else if (edge.flag == 1) {
      start = 0;
      end = ny - 1;
      index = nx - 1;
    } else if (edge.flag == 2) {
      start = nx - 1;
      end = 0;
      index = ny - 1;
    } else {
      start = ny - 1;
      end = 0;
      index = 0;
    }
    out << start << " " << end << " " << index << " " << edge.flag
        << " 1 1\n";
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
  rapidjson::Document doc;
  doc.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

int json_get_int(std::string_view json, const char* key, int fallback) {
  if (!key || json.empty()) {
    return fallback;
  }
  rapidjson::Document doc;
  doc.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (doc.HasParseError() || !doc.IsObject()) {
    return fallback;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsInt()) {
    return fallback;
  }
  return it->value.GetInt();
}

bool json_get_bool(std::string_view json, const char* key, bool fallback) {
  if (!key || json.empty()) {
    return fallback;
  }
  rapidjson::Document doc;
  doc.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (doc.HasParseError() || !doc.IsObject()) {
    return fallback;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsBool()) {
    return fallback;
  }
  return it->value.GetBool();
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
                    int flag) {
  if (!host || flag < 0 || flag > 3) {
    return false;
  }
  if (!workspace_has_linestring_tool(host)) {
    return false;
  }
  arm_grid_boundary_store(flag);
  tool::CommandArgs line_args = args;
  const std::string payload = "boundary_" + std::to_string(flag);
  line_args.payload = payload;
  return host->execute(kEditAppendLinestring, line_args);
}

bool handle_save_boundary(content::PluginHost* host, const tool::CommandArgs&) {
  if (!host) {
    return false;
  }
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
  const std::string path_u8 = picked.path;
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("path");
  w.String(path_u8.c_str(), static_cast<rapidjson::SizeType>(path_u8.size()));
  w.Key("elliptic_iters");
  w.Int(g_elliptic_iters);
  w.EndObject();
  return host->run_processing("baogrid.create_orth_grid",
                              std::string(buf.GetString(), buf.GetSize()));
}

bool handle_generate(content::PluginHost*, const tool::CommandArgs&) {
  if (!edges_complete()) {
    set_operation_result("{\"error\":\"boundary_incomplete\"}");
    ui::views::show_message_box(
        ui::views::MessageBoxKind::kError,
        "Digitize all four boundaries (0..3) before generating.");
    return false;
  }
  const detail::BoundarySolve solved = solve_session();
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  if (!commit_solved(solved)) {
    set_operation_result("{\"error\":\"mesh_commit_failed\"}");
    return false;
  }
  set_operation_result(solved.message);
  return true;
}

bool create_orth_grid_processing(content::PluginHost*,
                                 std::string_view args_json) {
  const int iters = json_get_int(args_json, "elliptic_iters", g_elliptic_iters);
  g_elliptic_iters = std::max(0, iters);

  detail::BoundarySolve solved;
  if (json_get_bool(args_json, "from_session", false)) {
    if (!edges_complete()) {
      set_operation_result("{\"error\":\"boundary_incomplete\"}");
      return false;
    }
    solved = solve_session();
  } else {
    std::string path;
    if (!json_get_string(args_json, "path", &path)) {
      set_operation_result("{\"error\":\"bad_args\"}");
      return false;
    }
    solved = detail::solve_grid_boundary_file(path, g_elliptic_iters);
  }
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  if (!commit_solved(solved)) {
    set_operation_result("{\"error\":\"mesh_commit_failed\"}");
    return false;
  }
  set_operation_result(solved.message);
  return true;
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

bool contribute_boundary(content::PluginHost* host,
                         std::string_view id,
                         std::string_view title,
                         int flag) {
  return contribute(host, id, title,
                    [host, flag](const tool::CommandArgs& args) {
                      return input_boundary(host, args, flag);
                    });
}

}  // namespace

void set_orthogrid_mesh_writer(OrthogridMeshWriter writer) {
  g_mesh_writer = std::move(writer);
}

bool publish_orthogrid_mesh(const OrthogridMeshCommit& commit) {
  if (!g_mesh_writer) {
    return false;
  }
  return g_mesh_writer(commit);
}

void set_orthogrid_elliptic_iters(int n) { g_elliptic_iters = std::max(0, n); }

int orthogrid_elliptic_iters() { return g_elliptic_iters; }

void arm_grid_boundary(int flag) { arm_grid_boundary_store(flag); }

bool grid_boundary_armed() { return grid_boundary_armed_store(); }

bool note_grid_boundary(const double* xy, size_t count) {
  return note_grid_boundary_store(xy, count);
}

bool register_orthogrid(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  for (const char* prefix : {"baogrid", "orthogrid"}) {
    if (!contribute_boundary(host,
                             std::string(prefix) + ".input_boundary_0",
                             "Input boundary 0", 0) ||
        !contribute_boundary(host,
                             std::string(prefix) + ".input_boundary_1",
                             "Input boundary 1", 1) ||
        !contribute_boundary(host,
                             std::string(prefix) + ".input_boundary_2",
                             "Input boundary 2", 2) ||
        !contribute_boundary(host,
                             std::string(prefix) + ".input_boundary_3",
                             "Input boundary 3", 3)) {
      return false;
    }
    if (!contribute(host, std::string(prefix) + ".save_boundary",
                    "Save grid boundary",
                    [host](const tool::CommandArgs& args) {
                      return handle_save_boundary(host, args);
                    })) {
      return false;
    }
    if (!contribute(host, std::string(prefix) + ".load_boundary",
                    "Load grid boundary",
                    [host](const tool::CommandArgs& args) {
                      return handle_load_boundary(host, args);
                    })) {
      return false;
    }
    if (!contribute(host, std::string(prefix) + ".generate", "Generate orth grid",
                    [host](const tool::CommandArgs& args) {
                      return handle_generate(host, args);
                    })) {
      return false;
    }
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
