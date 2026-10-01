// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_world3d.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/view_host.h"
#include "gis/present/style/style_document.h"
#include "gis/vista/world/pointcloud/ingest/load.h"
#include "plugin/product/world3d/commands.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {
namespace {

// Point cloud via shared gis loader → map point features (Views footprint).
bool add_pointcloud_layer(content::MapScene* doc,
                           BrowserUiDelegate* ui,
                           const std::string& path) {
  if (!doc || path.empty()) {
    return false;
  }
  gis::PointCloud cloud;
  if (!gis::load_point_cloud(path.c_str(), &cloud) || cloud.empty()) {
    return false;
  }
  const uint8_t* rgba = cloud.has_color() ? cloud.rgba.data() : nullptr;
  if (!doc->add_point_cloud_layer("Point cloud", cloud.xyz.data(),
                                 static_cast<int>(cloud.point_count()),
                                 rgba)) {
    return false;
  }
  return ::app::detail::refresh_ui_after_layer(ui);
}

bool add_pointcloud_xyz_layer(content::MapScene* doc,
                               BrowserUiDelegate* ui,
                               const float* xyz,
                               int point_count,
                               const uint8_t* rgba) {
  if (!doc || !xyz || point_count <= 0) {
    return false;
  }
  if (!doc->add_point_cloud_layer("Point cloud", xyz, point_count, rgba)) {
    return false;
  }
  return ::app::detail::refresh_ui_after_layer(ui);
}

constexpr const char* kWorld3dStyleJson = R"json({
  "version": 8,
  "name": "world3d_mesh",
  "layers": [
    {"id":"tin-fill","type":"fill","source-layer":"DEM tin",
     "paint":{"fill-color":"#102a43","fill-opacity":1.0}},
    {"id":"tin-line","type":"line","source-layer":"DEM tin",
     "paint":{"line-color":"#000000","line-width":2.0}},
    {"id":"grid-fill","type":"fill","source-layer":"DEM grid",
     "paint":{"fill-color":"#243b53","fill-opacity":0.95}},
    {"id":"grid-line","type":"line","source-layer":"DEM grid",
     "paint":{"line-color":"#0a0a0a","line-width":1.5}},
    {"id":"cloud","type":"circle","source-layer":"Point cloud",
     "paint":{"circle-color":["coalesce",["get","color"],"#c48a3a"],
       "circle-radius":10,"circle-opacity":1.0}},
    {"id":"sphere","type":"fill","source-layer":"Sphere",
     "paint":{"fill-color":"#334e68","fill-opacity":1.0}},
    {"id":"sphere-line","type":"line","source-layer":"Sphere",
     "paint":{"line-color":"#000000","line-width":2.0}}
  ]
})json";

bool apply_world3d_mesh_style(content::MapScene* doc) {
  if (!doc) {
    return false;
  }
  auto style = std::make_shared<gis::style::StyleDocument>();
  if (!gis::style::parse_style_document(kWorld3dStyleJson, style.get())) {
    return false;
  }
  doc->set_style_document(std::move(style));
  return true;
}

}  // namespace

void wire_world3d_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_world3d_surface_writer(
      [browser](const double* xyz, int point_count, const int* triangles,
             int triangle_count, const char* op) {
        const char* name =
            (op && std::strstr(op, "grid")) ? "DEM grid" : "DEM tin";
        if (!browser->session().document().add_triangle_layer(
                name, xyz, point_count, triangles, triangle_count)) {
          return false;
        }
        // Style must be set here: default carto maps type=polygon → cream
        // "land" (lit_ratio=1). UI refresh stays deferred (pool workers).
        (void)apply_world3d_mesh_style(&browser->session().document());
        return true;
      });

  plugin::World3dSceneWriter scene_writer;
  scene_writer.add_pointcloud = [browser](const std::string& path) {
    if (!add_pointcloud_layer(&browser->session().document(), browser->ui(), path)) {
      return false;
    }
    return apply_world3d_mesh_style(&browser->session().document());
  };
  scene_writer.add_pointcloud_xyz =
      [browser](const float* xyz, int point_count, const uint8_t* rgba) {
        if (!add_pointcloud_xyz_layer(&browser->session().document(), browser->ui(), xyz,
                                     point_count, rgba)) {
          return false;
        }
        return apply_world3d_mesh_style(&browser->session().document());
      };
  scene_writer.add_sphere = [browser]() {
    if (!::app::detail::add_standin_mesh(&browser->session().document(), browser->ui(), "Sphere", 105.0,
                          35.0, 0.4)) {
      return false;
    }
    return apply_world3d_mesh_style(&browser->session().document());
  };
  scene_writer.add_water = [browser]() {
    return ::app::detail::add_standin_mesh(&browser->session().document(), browser->ui(), "Water", 110.0,
                            30.0, 0.6);
  };
  scene_writer.add_terrain_grid = [browser]() {
    return ::app::detail::add_standin_mesh(&browser->session().document(), browser->ui(), "Terrain GRID",
                            100.0, 35.0, 1.0);
  };
  scene_writer.add_terrain_tin = [browser]() {
    return ::app::detail::add_standin_mesh(&browser->session().document(), browser->ui(), "Terrain TIN",
                            102.0, 36.0, 1.0);
  };
  scene_writer.create_tin_from_active_layer = [browser]() {
    return browser->session().document().feature_count() > 0;
  };
  scene_writer.layer_points_to_3d = [browser]() {
    return browser->session().document().feature_count() > 0;
  };
  scene_writer.layer_lines_to_3d = [browser]() {
    return browser->session().document().feature_count() > 0;
  };
  scene_writer.layer_polygons_to_3d = [browser]() {
    return browser->session().document().feature_count() > 0;
  };
  scene_writer.open_earth = [browser]() {
    // Scene tab index 2 matches plugin-showcase / Views chrome (Map | Data | 3D).
    browser->select_map_tab(2);
    apply_china_scene3d_product_defaults(*browser);
    return browser->scene3d() != nullptr && browser->orbit_frame() != nullptr;
  };
  scene_writer.fly_to = [browser](double lon, double lat, float distance,
                               double span_deg) {
    content::OrbitFrame* orbit = browser->orbit_frame();
    if (!orbit) {
      return false;
    }
    const double half = (span_deg > 0.05) ? (span_deg * 0.5) : 2.0;
    content::Extent2 box;
    box.xmin = lon - half;
    box.ymin = lat - half;
    box.xmax = lon + half;
    box.ymax = lat + half;
    orbit->apply_world_extent(box);
    if (distance > 0.05f) {
      orbit->set_distance(distance);
    }
    browser->push_shared_extent();
    return true;
  };
  scene_writer.attach_tileset = [browser](const std::string& tileset_json_path) {
    content::Scene3dPresenter* cam = browser->scene3d();
    if (!cam) {
      return false;
    }
    std::string path = tileset_json_path;
    if (path.empty()) {
      // Prefer shipped m3 city fixture next to out/data (exe is out/<cfg>/).
      static const char* kRels[] = {"../data/m3_city_tileset.json",
                                    "data/m3_city_tileset.json"};
      char base[MAX_PATH] = {};
      if (!detail::exe_dir_with_slash_a(base, MAX_PATH)) {
        return false;
      }
      bool found = false;
      for (const char* rel : kRels) {
        char full[MAX_PATH] = {};
        if (strcpy_s(full, base) != 0 || strcat_s(full, rel) != 0) {
          continue;
        }
        if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
          continue;
        }
        path = full;
        found = true;
        break;
      }
      if (!found) {
        return false;
      }
    }
    std::ifstream in(path, std::ios::binary);
    if (!in) {
      return false;
    }
    std::string json((std::istreambuf_iterator<char>(in)),
                     std::istreambuf_iterator<char>());
    if (json.empty()) {
      return false;
    }
    std::string root = path;
    const auto slash = root.find_last_of("/\\");
    if (slash != std::string::npos) {
      root.resize(slash + 1);
      cam->gpu().set_tileset_content_root(root);
    }
    return cam->gpu().attach_tileset_json(json.c_str(), json.size(),
                                          "world3d_city");
  };
  plugin::set_world3d_scene_writer(std::move(scene_writer));

}

}  // namespace detail
}  // namespace app
