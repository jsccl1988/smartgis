// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/hexgrid/register.h"

#include <string>
#include <string_view>

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/detail/contribute.h"
#include "plugin/product/world3d/grid/hexgrid/present/mesh.h"
#include "plugin/product/world3d/grid/hexgrid/solve/boundary_solve.h"
#include "plugin/product/world3d/grid/hexgrid/sample/sample_volume.h"
#include "plugin/product/world3d/grid/hexgrid/io/vtk_structured.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

constexpr const char* kMenuId = "tools.orthogrid3d";

content::PluginHost* g_present_host = nullptr;
detail::HexCornerSolve g_last_hex;

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

bool parse_corners(std::string_view json, detail::Xyz corners[8]) {
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
    corners[n] = detail::Xyz(c[0].GetDouble(), c[1].GetDouble(),
                                 c[2].GetDouble());
  }
  return true;
}

bool commit_solved(content::PluginHost* host,
                   const detail::HexCornerSolve& solved,
                   const std::string& vts_path) {
  if (!solved.ok) {
    return false;
  }
  if (!vts_path.empty()) {
    if (!detail::write_vtk_structured(
            solved.grid, vts_path, solved.cell_orth.data(),
            solved.cell_orth.size())) {
      set_operation_result("{\"error\":\"vtk_write_failed\"}");
      return false;
    }
  }
  HexGridCommit commit;
  commit.nodes = &solved.grid.nodes;
  commit.nx = solved.grid.nx;
  commit.ny = solved.grid.ny;
  commit.nz = solved.grid.nz;
  commit.cell_orth =
      solved.cell_orth.empty() ? nullptr : solved.cell_orth.data();
  commit.cell_orth_count = static_cast<int>(solved.cell_orth.size());
  g_last_hex = solved;
  if (host) {
    content::GisDocument* gis = host->gis_document();
    if (!gis) {
      set_operation_result("{\"error\":\"no_hexgrid_seam\"}");
      return false;
    }
    if (!present_hex_grid_mesh(gis, host->scene3d_sink(), nullptr, commit)) {
      set_operation_result("{\"error\":\"mesh_commit_failed\"}");
      return false;
    }
    if (content::PluginHost::Playback* pb = host->playback()) {
      pb->clear();
      pb->push_frame("{\"index\":0}");
      pb->set_index(0);
    }
    (void)host->present_dataset("smartgis.world3d", "", 1);
  }
  set_operation_result(solved.message);
  return true;
}

bool orthogrid3d_present_frame(content::PluginHost* host,
                               std::string_view args_json) {
  (void)args_json;
  if (!host || !g_last_hex.ok) {
    set_operation_result(
        "{\"error\":\"no_hexgrid_session\",\"op\":\"orthogrid3d.present_frame\"}");
    return false;
  }
  content::GisDocument* gis = host->gis_document();
  if (!gis) {
    set_operation_result(
        "{\"error\":\"no_hexgrid_seam\",\"op\":\"orthogrid3d.present_frame\"}");
    return false;
  }
  HexGridCommit commit;
  commit.nodes = &g_last_hex.grid.nodes;
  commit.nx = g_last_hex.grid.nx;
  commit.ny = g_last_hex.grid.ny;
  commit.nz = g_last_hex.grid.nz;
  commit.cell_orth =
      g_last_hex.cell_orth.empty() ? nullptr : g_last_hex.cell_orth.data();
  commit.cell_orth_count = static_cast<int>(g_last_hex.cell_orth.size());
  if (!present_hex_grid_mesh(gis, host->scene3d_sink(), nullptr, commit)) {
    set_operation_result(
        "{\"error\":\"present_failed\",\"op\":\"orthogrid3d.present_frame\"}");
    return false;
  }
  if (content::PluginHost::Playback* pb = host->playback()) {
    pb->set_index(0);
  }
  set_operation_result("{\"ok\":true,\"op\":\"orthogrid3d.present_frame\"}");
  return true;
}

bool create_hex_grid_processing(content::PluginHost* host,
                                std::string_view args_json) {
  const int nx = json_get_int(args_json, "nx", detail::k_demo_nx);
  const int ny = json_get_int(args_json, "ny", detail::k_demo_ny);
  const int nz = json_get_int(args_json, "nz", detail::k_demo_nz);
  const int nx_use = nx < 3 ? 3 : nx;
  const int ny_use = ny < 3 ? 3 : ny;
  const int nz_use = nz < 3 ? 3 : nz;
  if (nx_use * ny_use * nz_use > 200000) {
    set_operation_result("{\"error\":\"too_large\"}");
    return false;
  }

  std::string vts_path;
  json_get_string(args_json, "vts_path", &vts_path);

  detail::Xyz corners[8];
  const detail::HexCornerSolve solved =
      parse_corners(args_json, corners)
          ? detail::solve_hex_from_corners(corners, nx_use, ny_use, nz_use)
          : detail::solve_hex_from_quarry_sample(nx_use, ny_use, nz_use);
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  return commit_solved(host, solved, vts_path);
}

bool handle_generate(content::PluginHost* host, const tool::CommandArgs&) {
  if (!host) {
    return false;
  }
  return host->run_processing("orthogrid3d.create_hex_grid", "{}");
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
      std::string("{\"vts_path\":\"") + picked.path + "\"}";
  return host->run_processing("orthogrid3d.create_hex_grid", args);
}

}  // namespace

bool publish_hex_grid(const HexGridCommit& commit) {
  if (!g_present_host || !g_present_host->gis_document()) {
    return false;
  }
  return present_hex_grid_mesh(g_present_host->gis_document(),
                               g_present_host->scene3d_sink(), nullptr,
                               commit);
}

namespace detail {

bool register_world3d_hexgrid(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  g_present_host = host;
  if (!contribute_command_aliases(
          host, {{"orthogrid3d.generate", "Generate 3D orth grid"}}, kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_generate(host, args);
          })) {
    return false;
  }
  if (!contribute_command_aliases(
          host, {{"orthogrid3d.export_vts", "Export hex grid VTK"}}, kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_export_vts(host, args);
          })) {
    return false;
  }
  if (!contribute_processing_aliases(
          host, {{"orthogrid3d.create_hex_grid", "Create 3D orth hex grid"}},
          create_hex_grid_processing)) {
    return false;
  }
  return host->contribute_processing(
      detail::kWorld3dPluginId,
      {"orthogrid3d.present_frame", "Re-present hex grid frame"},
      orthogrid3d_present_frame);
}

}  // namespace detail
}  // namespace plugin
