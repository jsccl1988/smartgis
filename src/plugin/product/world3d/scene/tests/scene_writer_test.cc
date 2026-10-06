// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/commands.h"

#include <cstdio>
#include <string>

#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/scene/look/look.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/processing/operation_result.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

class StubGisDocument final : public content::GisDocument {
 public:
  size_t feature_count() const override { return feature_count_; }
  bool add_point_cloud(std::string_view, const float*, int,
                       const uint8_t*) override {
    ++pointcloud_commits_;
    return true;
  }
  bool add_triangle_mesh(std::string_view, const double*, int, const int*,
                         int) override {
    ++mesh_commits_;
    return true;
  }

  size_t feature_count_ = 1;
  int pointcloud_commits_ = 0;
  int mesh_commits_ = 0;
};

void install_scene_stubs(content::PluginHost* host, StubGisDocument* doc) {
  host->set_gis_document(doc);
  plugin::Scene3dSink* sink = plugin::scene3d_sink(host);
  expect(sink != nullptr, "scene3d_sink");
  if (!sink) {
    return;
  }
  sink->set_bridges(
      [](std::string_view, double, double, double) { return true; },
      [](std::string_view) { return true; }, []() {});
  sink->set_earth_bridges(
      []() { return true; },
      [](double lon, double lat, float distance, double span) {
        return lon > 116.3 && lon < 116.5 && lat > 39.8 && lat < 40.0 &&
               distance > 0.f && span > 0.0;
      },
      [](std::string_view path, std::string* result) {
        if (result) {
          *result =
              "{\"ok\":true,\"op\":\"world3d.load_global_dem\",\"source\":\"stub\"}";
        }
        return true;
      },
      [](std::string_view, bool enabled, std::string* result) {
        if (result) {
          *result = enabled ? "{\"ok\":true,\"mode\":\"procedural\"}"
                            : "{\"ok\":true,\"mode\":\"off\"}";
        }
        return true;
      },
      [](bool sky, bool ocean, bool cloud, bool fog) {
        return sky || ocean || cloud || fog || true;
      });
  sink->set_look_bridges(
      [](std::string_view mode, std::string* result) {
        if (mode.empty()) {
          return false;
        }
        if (result) {
          *result = "{\"ok\":true,\"op\":\"world3d.apply_look\",\"mode\":\"stub\"}";
        }
        return true;
      },
      [](float t, std::string* result) {
        if (t < 0.f) {
          return false;
        }
        if (result) {
          *result = "{\"ok\":true,\"op\":\"world3d.fly_globe\"}";
        }
        return true;
      });
}

}  // namespace

int main() {
  content::PluginHost* host =
      content::create_plugin_host(nullptr, nullptr, nullptr);
  expect(host != nullptr, "create_plugin_host");
  plugin::HostCapabilities caps;
  caps.attach(host);
  expect(plugin::register_world3d(host), "register_world3d");

  plugin::World3dLook parsed = plugin::World3dLook::kLand;
  expect(plugin::parse_world3d_look("globe", &parsed) &&
             parsed == plugin::World3dLook::kGlobe,
         "parse globe look");
  expect(plugin::parse_world3d_look("east_china", &parsed) &&
             parsed == plugin::World3dLook::kEastChina,
         "parse east_china look");
  expect(!plugin::parse_world3d_look("nope", &parsed), "parse rejects unknown");

  expect(!host->run_processing("model3d.add_sphere", "{}"),
         "add_sphere refuses without gis/sink");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "add_sphere structured no_scene_device");

  expect(!host->run_processing("world3d.open_earth", "{}"),
         "open_earth refuses without earth bridges");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "open_earth structured no_scene_device");

  expect(!host->run_processing("world3d.load_global_dem", "{}"),
         "load_global_dem refuses without earth bridges");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "load_global_dem structured no_scene_device");

  expect(!host->run_processing("world3d.set_satellite_cloud", "{}"),
         "set_satellite_cloud refuses without earth bridges");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "set_satellite_cloud structured no_scene_device");

  expect(!host->run_processing("world3d.apply_look", "{\"mode\":\"globe\"}"),
         "apply_look refuses without look bridges");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "apply_look structured no_scene_device");

  expect(!host->run_processing("model3d.add_pointcloud", "{}"),
         "add_pointcloud bad_args without path");
  expect(plugin::operation_result().find("bad_args") != std::string::npos,
         "add_pointcloud structured bad_args");

  expect(!host->run_processing(
             "model3d.add_pointcloud",
             "{\"path\":\"Z:/no/such/model3d_fixture.txt\"}"),
         "add_pointcloud refuses without gis document");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "add_pointcloud structured no_scene_device");

  StubGisDocument stub_doc;
  install_scene_stubs(host, &stub_doc);

  expect(host->run_processing("model3d.add_sphere", "{}"),
         "add_sphere with gis/sink");
  expect(stub_doc.mesh_commits_ > 0, "standin mesh committed");
  expect(host->run_processing("model3d.create_trimesh", "{}"),
         "create_trimesh with gis");
  expect(host->run_processing("model3d.layer_points_to_3d", "{}"),
         "layer_points_to_3d with gis");
  expect(host->run_processing("world3d.open_earth", "{}"),
         "open_earth with sink stubs");
  expect(host->run_processing(
             "world3d.fly_to",
             "{\"lon\":116.4,\"lat\":39.9,\"distance\":1.5,\"span_deg\":3}"),
         "fly_to with sink stubs");
  expect(host->run_processing("world3d.attach_city_tileset", "{}"),
         "attach_city_tileset empty path");
  expect(host->run_processing("world3d.load_global_dem", "{}"),
         "load_global_dem with sink stubs");
  expect(host->run_processing("world3d.set_satellite_cloud",
                              "{\"enabled\":true}"),
         "set_satellite_cloud procedural");
  expect(host->run_processing(
             "world3d.set_atmosphere",
             "{\"sky\":true,\"ocean\":true,\"cloud\":true,\"fog\":true}"),
         "set_atmosphere with sink stubs");
  expect(host->run_processing("world3d.apply_look", "{\"mode\":\"globe\"}"),
         "apply_look with look stubs");
  expect(host->run_processing("world3d.fly_globe", "{\"t\":0.42}"),
         "fly_globe with look stubs");

  expect(!host->run_processing("world3d.fly_to", "{}"),
         "fly_to refuses without lon/lat");
  expect(plugin::operation_result().find("bad_args") != std::string::npos,
         "fly_to structured bad_args");

  host->set_gis_document(nullptr);
  if (plugin::Scene3dSink* sink = plugin::scene3d_sink(host)) {
    sink->set_earth_bridges(nullptr, nullptr, nullptr, nullptr, nullptr);
    sink->set_look_bridges(nullptr, nullptr);
  }
  expect(!host->run_processing("model3d.add_water", "{}"),
         "add_water refuses after gis cleared");
  expect(!host->run_processing("world3d.open_earth", "{}"),
         "open_earth refuses after bridges cleared");
  expect(plugin::operation_result().find("no_scene_device") != std::string::npos,
         "open_earth structured no_scene_device");
  expect(!host->run_processing("world3d.load_global_dem", "{}"),
         "load_global_dem refuses after bridges cleared");
  expect(!host->run_processing("world3d.set_satellite_cloud", "{}"),
         "set_satellite_cloud refuses after bridges cleared");
  expect(!host->run_processing("world3d.set_atmosphere", "{}"),
         "set_atmosphere refuses after bridges cleared");
  expect(!host->run_processing("world3d.apply_look", "{\"mode\":\"globe\"}"),
         "apply_look refuses after look bridges cleared");
  expect(!host->run_processing("world3d.fly_globe", "{}"),
         "fly_globe refuses after look bridges cleared");

  delete host;

  if (g_fails != 0) {
    std::fprintf(stderr, "%d check(s) failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "world3d_scene_writer_test ok\n");
  return 0;
}
