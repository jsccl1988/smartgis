// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/orthogrid3d/commands.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/product/orthogrid3d/detail/boundary_solve.h"
#include "plugin/product/orthogrid3d/detail/vtk_structured.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.orthogrid3d";
constexpr const char* kMenuId = "tools.orthogrid3d";

HexGridWriter g_hex_writer;

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

bool parse_corners(std::string_view json, geo::Raw3DPoint corners[8]) {
  rapidjson::Document doc;
  doc.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember("corners");
  if (it == doc.MemberEnd() || !it->value.IsArray() ||
      it->value.Size() != 8) {
    return false;
  }
  for (rapidjson::SizeType n = 0; n < 8; ++n) {
    const auto& c = it->value[n];
    if (!c.IsArray() || c.Size() != 3 || !c[0].IsNumber() || !c[1].IsNumber() ||
        !c[2].IsNumber()) {
      return false;
    }
    corners[n] = geo::Raw3DPoint(c[0].GetDouble(), c[1].GetDouble(),
                                 c[2].GetDouble());
  }
  return true;
}

void fill_unit_box_corners(geo::Raw3DPoint corners[8]) {
  // (0,0,0)(1,0,0)(1,1,0)(0,1,0)(0,0,1)(1,0,1)(1,1,1)(0,1,1)
  corners[0] = geo::Raw3DPoint(0, 0, 0);
  corners[1] = geo::Raw3DPoint(1, 0, 0);
  corners[2] = geo::Raw3DPoint(1, 1, 0);
  corners[3] = geo::Raw3DPoint(0, 1, 0);
  corners[4] = geo::Raw3DPoint(0, 0, 1);
  corners[5] = geo::Raw3DPoint(1, 0, 1);
  corners[6] = geo::Raw3DPoint(1, 1, 1);
  corners[7] = geo::Raw3DPoint(0, 1, 1);
}

bool commit_solved(const detail::HexCornerSolve& solved,
                   const std::string& vts_path) {
  if (!solved.ok) {
    return false;
  }
  if (!vts_path.empty()) {
    if (!orthogrid3d::write_vtk_structured(
            solved.grid, vts_path, solved.cell_orth.data(),
            solved.cell_orth.size())) {
      set_operation_result("{\"error\":\"vtk_write_failed\"}");
      return false;
    }
  }
  HexGridCommit commit;
  commit.grid = &solved.grid;
  commit.cell_orth =
      solved.cell_orth.empty() ? nullptr : solved.cell_orth.data();
  commit.cell_orth_count = static_cast<int>(solved.cell_orth.size());
  if (g_hex_writer) {
    if (!g_hex_writer(commit)) {
      set_operation_result("{\"error\":\"mesh_commit_failed\"}");
      return false;
    }
  }
  set_operation_result(solved.message);
  return true;
}

bool create_hex_grid_processing(content::PluginHost*,
                                std::string_view args_json) {
  const int nx = json_get_int(args_json, "nx", 8);
  const int ny = json_get_int(args_json, "ny", 8);
  const int nz = json_get_int(args_json, "nz", 8);
  const int nx_use = nx < 3 ? 3 : nx;
  const int ny_use = ny < 3 ? 3 : ny;
  const int nz_use = nz < 3 ? 3 : nz;
  if (nx_use * ny_use * nz_use > 200000) {
    set_operation_result("{\"error\":\"too_large\"}");
    return false;
  }

  geo::Raw3DPoint corners[8];
  if (!parse_corners(args_json, corners)) {
    fill_unit_box_corners(corners);
  }

  std::string vts_path;
  json_get_string(args_json, "vts_path", &vts_path);

  const detail::HexCornerSolve solved =
      detail::solve_hex_from_corners(corners, nx_use, ny_use, nz_use);
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  return commit_solved(solved, vts_path);
}

bool handle_generate(content::PluginHost* host, const tool::CommandArgs&) {
  if (!host) {
    return false;
  }
  return host->run_processing("orthogrid3d.create_hex_grid",
                              "{\"nx\":8,\"ny\":8,\"nz\":8}");
}

bool handle_export_vts(content::PluginHost* host, const tool::CommandArgs&) {
  if (!host) {
    return false;
  }
  const ui::views::FilePickerResult picked = ui::views::pick_save_file(
      L"VTK StructuredGrid\0*.vts\0All Files\0*.*\0");
  if (!picked.accepted || picked.path.empty()) {
    return false;
  }
  const std::string args =
      std::string("{\"nx\":8,\"ny\":8,\"nz\":8,\"vts_path\":\"") +
      picked.path + "\"}";
  return host->run_processing("orthogrid3d.create_hex_grid", args);
}

}  // namespace

void set_hex_grid_writer(HexGridWriter writer) {
  g_hex_writer = std::move(writer);
}

bool publish_hex_grid(const HexGridCommit& commit) {
  if (!g_hex_writer) {
    return false;
  }
  return g_hex_writer(commit);
}

bool register_orthogrid3d(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "orthogrid3d.generate", "Generate 3D orth grid", kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_generate(host, args);
          })) {
    return false;
  }
  if (!host->contribute_command(
          kPluginId, "orthogrid3d.export_vts", "Export hex grid VTK", kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_export_vts(host, args);
          })) {
    return false;
  }
  const content::ProcessingContribution proc = {
      "orthogrid3d.create_hex_grid", "Create 3D orth hex grid"};
  if (!host->contribute_processing(kPluginId, proc,
                                   create_hex_grid_processing)) {
    return false;
  }
  return true;
}

}  // namespace plugin
