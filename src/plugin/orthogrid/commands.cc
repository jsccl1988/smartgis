// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/orthogrid/commands.h"

#include <string>

#include "content/public/plugin_host.h"
#include "tool/command.h"
#include "ui/views/file_picker.h"

namespace plugin {
namespace {

constexpr const char* kPluginId = "smartgis.baogrid";
constexpr const char* kMenuId = "tools.baogrid";
constexpr const char* kEditAppendLinestring = "edit.append.linestring";

bool workspace_has_linestring_tool(content::PluginHost* host) {
  tool::CommandCatalog* catalog = host->commands();
  return catalog && catalog->contains(kEditAppendLinestring);
}

bool input_boundary(content::PluginHost* host,
                    const tool::CommandArgs& args,
                    std::string_view boundary_tag) {
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

bool handle_save_boundary(content::PluginHost*, const tool::CommandArgs&) {
  // Leftover handler body was empty; keep as a no-op failure.
  return false;
}

bool handle_load_boundary(content::PluginHost* host, const tool::CommandArgs&) {
  const ui::views::FilePickerResult picked = ui::views::pick_open_file(
      L"Text Files\0*.txt\0All Files\0*.*\0");
  if (!picked.accepted || picked.path.empty()) {
    return false;
  }
  const std::string json = std::string("{\"path\":\"") + picked.path + "\"}";
  return host->run_processing("baogrid.create_orth_grid", json);
}

bool create_orth_grid_processing(content::PluginHost*, std::string_view) {
  // Grid build runs in the MFC plugin DLL (SmtAMOrthogrid), not src_all.
  return false;
}

bool contribute(content::PluginHost* host,
                std::string_view command_id,
                std::string_view title,
                tool::CommandHandler handler) {
  return host->contribute_command(kPluginId, command_id, title, kMenuId,
                                  std::move(handler));
}

}  // namespace

bool register_orthogrid(content::PluginHost* host) {
  if (!host) {
    return false;
  }

  // Spec ids are baogrid.*; leftover AM / older Views still use orthogrid.*.
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
