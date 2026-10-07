// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/gis_document.h"
#include "content/browser/document/gis_scene.h"
#include "content/public/event_bus.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "plugin/runtime/host/processing/processing.h"

#include <cstdio>
#include <string>
#include <thread>
#include <utility>
#include <vector>

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
  {
    content::EventBus bus;
    int hits = 0;
    auto sub = bus.subscribe<content::LayersChanged>(
        [&](const content::LayersChanged& e) {
          (void)e;
          ++hits;
        });
    content::GisScene scene;
    content::GisSceneDocument gis(&scene, &bus);
    expect(gis.create_layer("heat", "Polygon"), "create_layer");
    expect(gis.layer_count() >= 1, "layer_count");
    expect(hits >= 1, "document.layers_changed");

    const std::vector<std::pair<double, double>> ring = {
        {116.0, 39.0}, {116.2, 39.0}, {116.2, 39.2}, {116.0, 39.2},
    };
    expect(plugin::append_map_polygon(&gis, ring, "1"), "append polygon");
    expect(gis.feature_count() >= 1, "feature_count");

    content::Extent2 ext{};
    expect(gis.compute_extent(&ext), "extent");

    const char* kStyle =
        "{\"version\":8,\"name\":\"gis-doc\",\"layers\":[]}";
    expect(gis.apply_style_json(kStyle), "style json");

    expect(plugin::add_standin_mesh(&gis, "standin", 116.1, 39.1, 0.05),
           "standin mesh");
  }
  {
    content::GisScene scene;
    content::GisSceneDocument gis(&scene, nullptr);
    content::PluginHost* host =
        content::create_plugin_host(nullptr, nullptr, nullptr);
    host->set_gis_document(&gis);
    bool presented = false;
    host->set_present_dataset_bridge(
        [&](std::string_view, std::string_view, int, int) {
          presented = true;
          return true;
        });
    const std::thread::id submitter = std::this_thread::get_id();
    std::thread::id compute_tid{};
    std::thread::id present_tid{};
    plugin::ProcessingPool pool(plugin::ProcessingMode::kThread);
    expect(pool.submit(
               "test.gis_present", "{}",
               [&](content::PluginHost*, std::string_view) {
                 compute_tid = std::this_thread::get_id();
                 return true;
               },
               [&](content::PluginHost*, std::string_view) {
                 present_tid = std::this_thread::get_id();
                 const std::vector<std::pair<double, double>> xy = {
                     {116.0, 39.0}, {116.4, 39.4},
                 };
                 expect(plugin::append_map_polyline(host->gis_document(), xy,
                                                    "0"),
                        "polyline in present");
                 expect(host->present_dataset("test.fixture", "", 0),
                        "present_dataset");
                 return true;
               },
               [&](bool ok, std::string) { expect(ok, "done"); }),
           "submit compute-then-present");
    pool.flush_for_test();
    expect(compute_tid != submitter, "compute worker");
    expect(present_tid == submitter, "present drain");
    expect(presented, "present_dataset called");
    expect(gis.feature_count() >= 1, "polyline features");
    delete host;
  }
  if (g_fails) {
    std::fprintf(stderr, "gis_document_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("gis_document_test: ok");
  return 0;
}
