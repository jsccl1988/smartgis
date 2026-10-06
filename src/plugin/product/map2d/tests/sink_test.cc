// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/commands.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
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
  bool create_layer(std::string_view, std::string_view) override {
    ++layers_;
    return true;
  }
  size_t layer_count() const override { return layers_; }
  size_t feature_count() const override { return features_; }

  size_t layers_ = 0;
  size_t features_ = 0;
};

void install_map2d_stubs(content::PluginHost* host) {
  plugin::Map2dSink* sink = plugin::map2d_sink(host);
  expect(sink != nullptr, "map2d_sink");
  if (!sink) {
    return;
  }
  sink->set_bridges(
      [](std::string_view, double, double, double) { return true; },
      [](std::string_view) { return true; }, []() {});
  sink->set_view_bridges(
      []() { return true; },
      [](double lon, double lat, double span) {
        return lon > 116.3 && lon < 116.5 && lat > 39.8 && lat < 40.0 &&
               span > 0.0;
      },
      [](std::string_view, std::string* result) {
        if (result) {
          *result =
              "{\"ok\":true,\"op\":\"map2d.load_hillshade\",\"source\":\"stub\"}";
        }
        return true;
      });
  sink->set_look_bridges(
      [](std::string_view mode, std::string* result) {
        if (mode.empty()) {
          return false;
        }
        if (result) {
          *result = "{\"ok\":true,\"op\":\"map2d.apply_look\",\"mode\":\"stub\"}";
        }
        return true;
      },
      [](float t, std::string* result) {
        if (t < 0.f) {
          return false;
        }
        if (result) {
          *result = "{\"ok\":true,\"op\":\"map2d.frame_fly\"}";
        }
        return true;
      });
  sink->set_present_bridges(
      [](uint32_t, uint32_t) { return true; },
      [](std::string_view path, int, int) { return !path.empty(); });
}

}  // namespace

int main() {
  content::PluginHost* host =
      content::create_plugin_host(nullptr, nullptr, nullptr);
  expect(host != nullptr, "create_plugin_host");
  plugin::HostCapabilities caps;
  caps.attach(host);
  expect(plugin::register_map2d(host), "register_map2d");

  expect(!host->run_processing("map2d.open_map", "{}"),
         "open_map refuses without view bridges");
  expect(plugin::operation_result().find("no_map_device") != std::string::npos,
         "open_map no_map_device");

  StubGisDocument gis;
  host->set_gis_document(&gis);
  install_map2d_stubs(host);

  expect(host->run_processing("map2d.open_map", "{}"), "open_map");
  expect(host->run_processing("map2d.frame_to",
                              "{\"lon\":116.4,\"lat\":39.9,\"span_deg\":4}"),
         "frame_to");
  expect(host->run_processing("map2d.attach_dataset", "{\"path\":\"x.gpkg\"}"),
         "attach_dataset");
  expect(host->run_processing("map2d.load_hillshade", "{}"), "load_hillshade");
  expect(host->run_processing("map2d.apply_look", "{\"mode\":\"china\"}"),
         "apply_look");
  expect(host->run_processing("map2d.frame_fly", "{\"t\":0.42}"), "frame_fly");
  expect(host->run_processing("map2d.export_bmp", "{\"path\":\"out.bmp\"}"),
         "export_bmp");
  expect(host->run_processing("map2d.present_gpu", "{\"width\":64,\"height\":64}"),
         "present_gpu");
  expect(host->run_processing(
             "map2d.add_standin_layer",
             "{\"name\":\"box\",\"lon\":116.4,\"lat\":39.9,\"half_deg\":0.5}"),
         "add_standin_layer");
  expect(!host->run_processing("map2d.apply_look", "{\"mode\":\"\"}"),
         "apply_look empty mode");
  expect(!host->run_processing("map2d.frame_to", "{\"lon\":1}"),
         "frame_to missing lat");

  caps.detach(host);
  expect(plugin::map2d_sink(host) == nullptr, "detached");
  delete host;
  if (g_fails) {
    std::fprintf(stderr, "map2d_sink_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("map2d_sink_test: ok");
  return 0;
}
