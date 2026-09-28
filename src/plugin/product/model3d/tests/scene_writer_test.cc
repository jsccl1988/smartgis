// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/model3d/commands.h"

#include <cstdio>
#include <string>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/operation_result.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  // Clear any leftover writer from process-local statics.
  plugin::set_model3d_scene_writer({});

  content::PluginHost* host =
      content::create_plugin_host(nullptr, nullptr, nullptr);
  expect(host != nullptr, "create_plugin_host");
  expect(plugin::register_model3d(host), "register_model3d");

  expect(!host->run_processing("model3d.add_sphere", "{}"),
         "add_sphere refuses without scene writer");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "add_sphere structured no_scene_device");

  expect(!host->run_processing("model3d.add_pointcloud", "{}"),
         "add_pointcloud bad_args without path");
  expect(plugin::operation_result().find("bad_args") != std::string::npos,
         "add_pointcloud structured bad_args");

  expect(!host->run_processing(
             "model3d.add_pointcloud",
             "{\"path\":\"Z:/no/such/model3d_fixture.txt\"}"),
         "add_pointcloud refuses without scene writer");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "add_pointcloud structured no_scene_device");

  int call_count = 0;
  std::string last_path;
  plugin::Model3dSceneWriter writer;
  writer.add_pointcloud = [&](const std::string& path) {
    ++call_count;
    last_path = path;
    return true;
  };
  writer.add_sphere = [&]() {
    ++call_count;
    return true;
  };
  writer.add_water = [&]() {
    ++call_count;
    return true;
  };
  writer.add_terrain_grid = [&]() {
    ++call_count;
    return true;
  };
  writer.add_terrain_tin = [&]() {
    ++call_count;
    return true;
  };
  writer.layer_points_to_3d = [&]() {
    ++call_count;
    return true;
  };
  writer.layer_lines_to_3d = [&]() {
    ++call_count;
    return true;
  };
  writer.layer_polygons_to_3d = [&]() {
    ++call_count;
    return true;
  };
  writer.create_tin_from_active_layer = [&]() {
    ++call_count;
    return true;
  };
  plugin::set_model3d_scene_writer(std::move(writer));

  expect(host->run_processing("model3d.add_sphere", "{}"),
         "add_sphere with writer");
  expect(host->run_processing("model3d.add_pointcloud",
                              "{\"path\":\"C:/tmp/cloud.txt\"}"),
         "add_pointcloud with writer");
  expect(last_path == "C:/tmp/cloud.txt", "pointcloud path forwarded");
  expect(host->run_processing("model3d.create_tin", "{}"),
         "create_tin with writer");
  expect(host->run_processing("model3d.layer_points_to_3d", "{}"),
         "layer_points_to_3d with writer");
  expect(call_count >= 4, "writer callbacks invoked");

  plugin::set_model3d_scene_writer({});
  expect(!host->run_processing("model3d.add_water", "{}"),
         "add_water refuses after writer cleared");

  delete host;

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
