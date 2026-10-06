// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/grid/orthogrid/register.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "content/public/plugin_host.h"
#include "plugin/product/world3d/detail/contribute.h"
#include "plugin/product/world3d/grid/orthogrid/session/session.h"
#include "plugin/product/world3d/grid/orthogrid/solve/boundary_solve.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "ui/views/dialogs/message_box.h"

#include <rapidjson/document.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

namespace plugin {
namespace {

constexpr const char* kMenuId = "tools.baogrid";
constexpr const char* kIdPrefixes[] = {"baogrid", "orthogrid"};
constexpr const char* kEditAppendLinestring = "edit.append.linestring";

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
  arm_grid_boundary(flag);
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
  if (!detail::write_orthogrid_gridbnd(picked.path)) {
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
  w.Int(detail::session_elliptic_iters());
  w.EndObject();
  return host->run_processing("baogrid.create_orth_grid",
                              std::string(buf.GetString(), buf.GetSize()));
}

bool handle_generate(content::PluginHost*, const tool::CommandArgs&) {
  if (!detail::orthogrid_edges_complete()) {
    set_operation_result("{\"error\":\"boundary_incomplete\"}");
    ui::views::show_message_box(
        ui::views::MessageBoxKind::kError,
        "Digitize all four boundaries (0..3) before generating.");
    return false;
  }
  const detail::BoundarySolve solved = detail::solve_orthogrid_session();
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  if (!detail::commit_orthogrid_solved(solved)) {
    set_operation_result("{\"error\":\"mesh_commit_failed\"}");
    return false;
  }
  set_operation_result(solved.message);
  return true;
}

bool create_orth_grid_processing(content::PluginHost* host,
                                 std::string_view args_json) {
  const int iters =
      json_get_int(args_json, "elliptic_iters", detail::session_elliptic_iters());
  detail::set_session_elliptic_iters(iters);

  detail::BoundarySolve solved;
  if (json_get_bool(args_json, "from_session", false)) {
    if (!detail::orthogrid_edges_complete()) {
      set_operation_result("{\"error\":\"boundary_incomplete\"}");
      return false;
    }
    solved = detail::solve_orthogrid_session();
  } else {
    std::string path;
    if (!json_get_string(args_json, "path", &path)) {
      set_operation_result("{\"error\":\"bad_args\"}");
      return false;
    }
    solved = detail::solve_grid_boundary_file(path, detail::session_elliptic_iters());
  }
  if (!solved.ok) {
    set_operation_result(solved.message);
    return false;
  }
  if (!host) {
    set_operation_result(solved.message);
    return true;
  }
  if (!detail::commit_orthogrid_solved(solved)) {
    set_operation_result("{\"error\":\"mesh_commit_failed\"}");
    return false;
  }
  set_operation_result(solved.message);
  return true;
}

}  // namespace

namespace detail {

bool register_world3d_orthogrid(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  bind_orthogrid_present_host(host);

  auto prefixes = {kIdPrefixes[0], kIdPrefixes[1]};
  for (int flag = 0; flag < 4; ++flag) {
    const std::string stem = "input_boundary_" + std::to_string(flag);
    const std::string title = "Input boundary " + std::to_string(flag);
    if (!contribute_prefixed_commands(
            host, prefixes, stem, title, kMenuId,
            [host, flag](const tool::CommandArgs& args) {
              return input_boundary(host, args, flag);
            })) {
      return false;
    }
  }
  if (!contribute_prefixed_commands(
          host, prefixes, "save_boundary", "Save grid boundary", kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_save_boundary(host, args);
          })) {
    return false;
  }
  if (!contribute_prefixed_commands(
          host, prefixes, "load_boundary", "Load grid boundary", kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_load_boundary(host, args);
          })) {
    return false;
  }
  if (!contribute_prefixed_commands(
          host, prefixes, "generate", "Generate orth grid", kMenuId,
          [host](const tool::CommandArgs& args) {
            return handle_generate(host, args);
          })) {
    return false;
  }
  if (!contribute_prefixed_commands(
          host, prefixes, "create_orth_grid", "Create orth grid", kMenuId,
          [host](const tool::CommandArgs& args) {
            return create_orth_grid_processing(host, args.payload);
          })) {
    return false;
  }
  if (!contribute_prefixed_processing(
          host, prefixes, "create_orth_grid", "Create orth grid",
          create_orth_grid_processing)) {
    return false;
  }
  return host->contribute_processing(
      detail::kWorld3dPluginId,
      {"orthogrid.present_frame", "Re-present orthogrid frame"},
      orthogrid_present_frame);
}

}  // namespace detail
}  // namespace plugin
