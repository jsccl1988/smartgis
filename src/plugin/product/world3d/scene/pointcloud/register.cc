// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/pointcloud/register.h"

#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/detail/contribute.h"
#include "plugin/product/world3d/scene/detail/host.h"
#include "plugin/product/world3d/scene/detail/json.h"
#include "plugin/product/world3d/scene/pointcloud/present.h"
#include "plugin/product/world3d/scene/present/style.h"
#include "plugin/runtime/host/processing/operation_result.h"
#include "tool/command/command.h"
#include "ui/views/dialogs/file_picker.h"
#include "vista/assets/pointcloud/pdal_io.h"

#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <cstdint>
#include <string>
#include <string_view>

namespace plugin {
namespace {

content::PluginHost* g_host = nullptr;

bool commit_pointcloud(content::PluginHost* host, const float* xyz,
                       int point_count, const uint8_t* rgba, const char* op) {
  if (!host || !host->gis_document()) {
    return detail::fail_no_scene_processing(op);
  }
  if (!present_world3d_pointcloud_xyz(host->gis_document(), xyz, point_count,
                                      rgba)) {
    set_operation_result(std::string("{\"error\":\"add_failed\",\"op\":\"") +
                         op + "\"}");
    return false;
  }
  (void)apply_world3d_mesh_style(host->gis_document());
  (void)host->present_dataset(detail::kWorld3dPluginId, "", 1);
  return true;
}

bool process_add_pointcloud(content::PluginHost* host,
                            std::string_view args_json) {
  rapidjson::Document args;
  if (!detail::parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  std::string path;
  if (!detail::scene_json_get_string(args, "path", &path) || path.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  if (!host || !host->gis_document()) {
    return detail::fail_no_scene_processing("model3d.add_pointcloud");
  }
  if (!present_world3d_pointcloud(host->gis_document(), path)) {
    set_operation_result(
        "{\"error\":\"add_failed\",\"op\":\"model3d.add_pointcloud\"}");
    return false;
  }
  (void)apply_world3d_mesh_style(host->gis_document());
  (void)host->present_dataset(detail::kWorld3dPluginId, "", 1);
  set_operation_result("{\"ok\":true,\"op\":\"model3d.add_pointcloud\"}");
  return true;
}

bool attach_pointcloud_result(content::PluginHost* host,
                              const vista::PointCloud& cloud, const char* op) {
  const uint8_t* rgba = cloud.has_color() ? cloud.rgba.data() : nullptr;
  if (!commit_pointcloud(host, cloud.xyz.data(),
                         static_cast<int>(cloud.point_count()), rgba, op)) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("ok");
  w.Bool(true);
  w.Key("op");
  w.String(op);
  w.Key("point_count");
  w.Uint64(cloud.point_count());
  w.Key("pdal");
  w.Bool(vista::pdal_is_available());
  w.EndObject();
  set_operation_result(buf.GetString());
  return true;
}

bool process_pdal_read(content::PluginHost* host, std::string_view args_json) {
  rapidjson::Document args;
  if (!detail::parse_scene_args(args_json, &args)) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.pdal_read\"}");
    return false;
  }
  std::string path;
  if (!detail::scene_json_get_string(args, "path", &path) || path.empty()) {
    set_operation_result("{\"error\":\"bad_args\",\"op\":\"world3d.pdal_read\"}");
    return false;
  }
  vista::PdalReadOptions opts;
  if (args.HasMember("z_min") && args["z_min"].IsNumber() &&
      args.HasMember("z_max") && args["z_max"].IsNumber()) {
    opts.has_z_range = true;
    opts.z_min = args["z_min"].GetDouble();
    opts.z_max = args["z_max"].GetDouble();
  }
  if (args.HasMember("max_points") && args["max_points"].IsUint64()) {
    opts.max_points = static_cast<size_t>(args["max_points"].GetUint64());
  }
  vista::PointCloud cloud;
  if (!vista::run_pdal_read(path.c_str(), opts, &cloud)) {
    const char* err =
        cloud.error.empty() ? "pdal_read_failed" : cloud.error.c_str();
    set_operation_result(std::string("{\"error\":\"") + err +
                         "\",\"op\":\"world3d.pdal_read\"}");
    return false;
  }
  bool attach = true;
  if (args.HasMember("attach") && args["attach"].IsBool()) {
    attach = args["attach"].GetBool();
  }
  if (!attach) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("ok");
    w.Bool(true);
    w.Key("op");
    w.String("world3d.pdal_read");
    w.Key("point_count");
    w.Uint64(cloud.point_count());
    w.Key("pdal");
    w.Bool(vista::pdal_is_available());
    w.EndObject();
    set_operation_result(buf.GetString());
    return true;
  }
  return attach_pointcloud_result(host, cloud, "world3d.pdal_read");
}

bool process_pdal_pipeline(content::PluginHost* host,
                           std::string_view args_json) {
  rapidjson::Document args;
  if (!detail::parse_scene_args(args_json, &args)) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.pdal_pipeline\"}");
    return false;
  }
  std::string pipeline_json;
  if (args.HasMember("pipeline") && args["pipeline"].IsArray()) {
    rapidjson::Document wrap;
    wrap.SetObject();
    rapidjson::Value pipe(args["pipeline"], wrap.GetAllocator());
    wrap.AddMember("pipeline", pipe, wrap.GetAllocator());
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    wrap.Accept(w);
    pipeline_json = buf.GetString();
  } else if (!detail::scene_json_get_string(args, "pipeline_json",
                                            &pipeline_json) ||
             pipeline_json.empty()) {
    set_operation_result(
        "{\"error\":\"bad_args\",\"op\":\"world3d.pdal_pipeline\"}");
    return false;
  }
  vista::PointCloud cloud;
  if (!vista::run_pdal_pipeline_json(pipeline_json, &cloud)) {
    const char* err =
        cloud.error.empty() ? "pdal_pipeline_failed" : cloud.error.c_str();
    set_operation_result(std::string("{\"error\":\"") + err +
                         "\",\"op\":\"world3d.pdal_pipeline\"}");
    return false;
  }
  bool attach = true;
  if (args.HasMember("attach") && args["attach"].IsBool()) {
    attach = args["attach"].GetBool();
  }
  if (!attach) {
    rapidjson::StringBuffer buf;
    rapidjson::Writer<rapidjson::StringBuffer> w(buf);
    w.StartObject();
    w.Key("ok");
    w.Bool(true);
    w.Key("op");
    w.String("world3d.pdal_pipeline");
    w.Key("point_count");
    w.Uint64(cloud.point_count());
    w.Key("pdal");
    w.Bool(vista::pdal_is_available());
    w.EndObject();
    set_operation_result(buf.GetString());
    return true;
  }
  return attach_pointcloud_result(host, cloud, "world3d.pdal_pipeline");
}

bool handle_add_pointcloud(const tool::CommandArgs&) {
  constexpr wchar_t kFilter[] =
      L"Point Cloud (*.las;*.laz;*.txt)\0*.las;*.laz;*.txt\0"
      L"LAS (*.las)\0*.las\0"
      L"LAZ (*.laz)\0*.laz\0"
      L"Text (*.txt)\0*.txt\0"
      L"All Files (*.*)\0*.*\0";
  const ui::views::FilePickerResult pick = ui::views::pick_open_file(kFilter);
  if (!pick.accepted || pick.path.empty()) {
    return false;
  }
  rapidjson::StringBuffer buf;
  rapidjson::Writer<rapidjson::StringBuffer> w(buf);
  w.StartObject();
  w.Key("path");
  w.String(pick.path.c_str());
  w.EndObject();
  return process_add_pointcloud(g_host, buf.GetString());
}

}  // namespace

namespace detail {

bool register_world3d_pointcloud(content::PluginHost* host) {
  if (!host) {
    return false;
  }
  g_host = host;
  return contribute_command_aliases(
             host,
             {{"model3d.add_pointcloud", "Add point cloud"},
              {"world3d.add_pointcloud", "加载点云"}},
             "tools", handle_add_pointcloud) &&
         contribute_processing_aliases(
             host,
             {{"model3d.add_pointcloud", "Add point cloud"},
              {"world3d.add_pointcloud", "加载点云"}},
             process_add_pointcloud) &&
         contribute_processing_aliases(
             host, {{"world3d.pdal_read", "PDAL read LAS/LAZ"}},
             process_pdal_read) &&
         contribute_processing_aliases(
             host, {{"world3d.pdal_pipeline", "PDAL pipeline JSON"}},
             process_pdal_pipeline);
}

}  // namespace detail
}  // namespace plugin
