// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include <cstdio>
#include <string>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/processing/operation_result.h"

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
  plugin::set_world3d_scene_writer({});

  content::PluginHost* host =
      content::create_plugin_host(nullptr, nullptr, nullptr);
  expect(host != nullptr, "create_plugin_host");
  expect(plugin::register_world3d(host), "register_world3d");

  expect(!host->run_processing("model3d.add_sphere", "{}"),
         "add_sphere refuses without scene writer");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "add_sphere structured no_scene_device");

  expect(!host->run_processing("world3d.open_earth", "{}"),
         "open_earth refuses without scene writer");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "open_earth structured no_scene_device");

  expect(!host->run_processing("world3d.load_global_dem", "{}"),
         "load_global_dem refuses without scene writer");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "load_global_dem structured no_scene_device");

  expect(!host->run_processing("world3d.set_satellite_cloud", "{}"),
         "set_satellite_cloud refuses without scene writer");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "set_satellite_cloud structured no_scene_device");

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
  plugin::World3dSceneWriter writer;
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
  writer.open_earth = [&]() {
    ++call_count;
    return true;
  };
  writer.fly_to = [&](double lon, double lat, float distance, double span) {
    ++call_count;
    last_path.clear();
    if (lon < 116.3 || lon > 116.5 || lat < 39.8 || lat > 40.0 ||
        distance <= 0.f || span <= 0.0) {
      return false;
    }
    return true;
  };
  writer.attach_tileset = [&](const std::string& path) {
    ++call_count;
    last_path = path;
    return true;
  };
  writer.load_global_dem = [&](const std::string& path, std::string* result) {
    ++call_count;
    last_path = path;
    if (result) {
      *result =
          "{\"ok\":true,\"op\":\"world3d.load_global_dem\",\"source\":\"stub\"}";
    }
    return true;
  };
  writer.set_satellite_cloud = [&](const std::string& path, bool enabled,
                                   std::string* result) {
    ++call_count;
    last_path = path;
    if (result) {
      *result = enabled ? "{\"ok\":true,\"mode\":\"procedural\"}"
                        : "{\"ok\":true,\"mode\":\"off\"}";
    }
    return true;
  };
  writer.set_atmosphere = [&](bool sky, bool ocean, bool cloud, bool fog) {
    ++call_count;
    return sky || ocean || cloud || fog || true;
  };
  plugin::set_world3d_scene_writer(std::move(writer));

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
  expect(host->run_processing("world3d.open_earth", "{}"),
         "open_earth with writer");
  expect(host->run_processing(
             "world3d.fly_to",
             "{\"lon\":116.4,\"lat\":39.9,\"distance\":1.5,\"span_deg\":3}"),
         "fly_to with writer");
  expect(host->run_processing("world3d.attach_city_tileset", "{}"),
         "attach_city_tileset empty path");
  expect(host->run_processing("world3d.load_global_dem", "{}"),
         "load_global_dem with writer");
  expect(host->run_processing("world3d.set_satellite_cloud",
                              "{\"enabled\":true}"),
         "set_satellite_cloud procedural");
  expect(host->run_processing(
             "world3d.set_atmosphere",
             "{\"sky\":true,\"ocean\":true,\"cloud\":true,\"fog\":true}"),
         "set_atmosphere with writer");
  expect(call_count >= 10, "writer callbacks invoked");

  expect(!host->run_processing("world3d.fly_to", "{}"),
         "fly_to refuses without lon/lat");
  expect(plugin::operation_result().find("bad_args") != std::string::npos,
         "fly_to structured bad_args");

  plugin::set_world3d_scene_writer({});
  expect(!host->run_processing("model3d.add_water", "{}"),
         "add_water refuses after writer cleared");
  expect(!host->run_processing("world3d.open_earth", "{}"),
         "open_earth refuses after writer cleared");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "open_earth structured no_scene_device");
  expect(!host->run_processing("world3d.load_global_dem", "{}"),
         "load_global_dem refuses after writer cleared");
  expect(!host->run_processing("world3d.set_satellite_cloud", "{}"),
         "set_satellite_cloud refuses after writer cleared");
  expect(!host->run_processing("world3d.set_atmosphere", "{}"),
         "set_atmosphere refuses after writer cleared");

  delete host;

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  return 0;
}
