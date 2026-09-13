// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/orthogrid/commands.h"

#include <cstdint>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "content/public/plugin_host.h"
#include "plugin/orthogrid/detail/laplace_solver.h"
#include "tool/command.h"
#include "ui/views/file_picker.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.baogrid";
constexpr const char* kMenuId = "tools.baogrid";
constexpr const char* kEditAppendLinestring = "edit.append.linestring";

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

bool boundary_file_looks_valid(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  std::ifstream fin(path);
  if (!fin) {
    return false;
  }
  std::string header;
  std::getline(fin, header);
  // Strip trailing CR from Windows text files.
  if (!header.empty() && header.back() == '\r') {
    header.pop_back();
  }
  return header == "gridbnd:";
}

// Tiny Dirichlet Laplace smoke proving the Eigen kernel without linking
// Orthogrid (which pulls GDAL/geo into the Views graph).
bool run_laplace_smoke() {
  constexpr int kNx = 5;
  constexpr int kNy = 5;
  std::vector<double> xs(static_cast<size_t>(kNx * kNy), 0.0);
  std::vector<double> ys(static_cast<size_t>(kNx * kNy), 0.0);
  std::vector<std::uint8_t> unknown(static_cast<size_t>(kNx * kNy), 0);
  for (int j = 0; j < kNy; ++j) {
    for (int i = 0; i < kNx; ++i) {
      const size_t k = static_cast<size_t>(j * kNx + i);
      xs[k] = static_cast<double>(i);
      ys[k] = static_cast<double>(j);
      if (i > 0 && i < kNx - 1 && j > 0 && j < kNy - 1) {
        unknown[k] = 1;
        xs[k] = 0.0;
        ys[k] = 0.0;
      }
    }
  }
  orthogrid::GridField field;
  field.nx = kNx;
  field.ny = kNy;
  field.x = xs.data();
  field.y = ys.data();
  return orthogrid::solve_laplace(field, unknown.data());
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
  // Leftover Notify digitizes via GTT_InputLine. New chrome without the
  // leftover DLL attached uses the workspace linestring command.
  if (!workspace_has_linestring_tool(host)) {
    return false;
  }
  tool::CommandArgs line_args = args;
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
  (void)picked;
  return false;
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

// Validate boundary file header, then run the Laplace kernel smoke. Full
// Orthogrid::CreateOrthGrid + map write stay on the MFC DLL until EditSession
// is wired for Views.
bool create_orth_grid_processing(content::PluginHost*,
                                 std::string_view args_json) {
  std::string path;
  if (!json_get_string(args_json, "path", &path)) {
    return false;
  }
  if (!boundary_file_looks_valid(path)) {
    return false;
  }
  return run_laplace_smoke();
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
