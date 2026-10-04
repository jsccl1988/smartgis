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
#include "gis/style/document/style_document.h"
#include "vista/assets/pointcloud/load.h"
#include "vista/terrain/dem/dem_raster.h"
#include "plugin/product/world3d/commands.h"

#include <algorithm>
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

// Point cloud via shared gis loader 鈫?map point features (Views footprint).
bool add_pointcloud_layer(content::MapScene* doc,
                           BrowserUiDelegate* ui,
                           const std::string& path) {
  if (!doc || path.empty()) {
    return false;
  }
  vista::PointCloud cloud;
  if (!vista::load_point_cloud(path.c_str(), &cloud) || cloud.empty()) {
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
        // Style must be set here: default carto maps type=polygon 鈫?cream
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
  scene_writer.add_terrain_heightmap = [browser]() {
    return ::app::detail::add_standin_mesh(&browser->session().document(), browser->ui(), "Terrain heightmap",
                            100.0, 35.0, 1.0);
  };
  scene_writer.add_terrain_trimesh = [browser]() {
    return ::app::detail::add_standin_mesh(&browser->session().document(), browser->ui(), "Terrain trimesh",
                            102.0, 36.0, 1.0);
  };
  scene_writer.create_trimesh_from_active_layer = [browser]() {
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
  scene_writer.load_global_dem =
      [browser](const std::string& dem_path, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::Scene3dPresenter* cam = browser->scene3d();
        content::OrbitFrame* orbit = browser->orbit_frame();
        if (!cam || !orbit) {
          write_result(
              "{\"error\":\"no_scene_device\",\"op\":\"world3d.load_global_dem\"}");
          return false;
        }

        std::string path = dem_path;
        const char* source = "explicit";
        if (path.empty()) {
          // Prefer shared finder (global_dem → china_dem stand-in). Avoid a
          // second relative-path search that can miss out/data/china_dem.tif.
          path = vista::find_sample_global_dem_path();
          if (!path.empty()) {
            source = (path.find("global_dem") != std::string::npos)
                         ? "global_default"
                         : "china_standin";
          }
        }

        if (path.empty()) {
          write_result(
              "{\"error\":\"missing_dem\",\"op\":\"world3d.load_global_dem\","
              "\"hint\":\"place GeoTIFF at out/data/china_dem.tif or "
              "out/data/global_dem.tif\"}");
          return false;
        }

        vista::DemRaster dem;
        if (!dem.load_gdal_raster(path.c_str()) || dem.empty()) {
          write_result(
              "{\"error\":\"missing_or_invalid_dem\",\"op\":\"world3d.load_global_dem\","
              "\"path\":\"" +
              path + "\"}");
          return false;
        }

        vista::set_sample_dem_path_override(path.c_str());
        browser->select_map_tab(2);
        apply_china_scene3d_atmosphere(*browser);

        double minx = 0, miny = 0, maxx = 0, maxy = 0;
        dem.envelope(&minx, &miny, &maxx, &maxy);
        content::Extent2 box;
        box.xmin = minx;
        box.ymin = miny;
        box.xmax = maxx;
        box.ymax = maxy;
        if (!content::extent_nonempty(box)) {
          box = content::kChinaLonLatExtent;
        }
        orbit->reset();
        orbit->apply_world_extent(box);
        const double span =
            (std::max)(box.xmax - box.xmin, box.ymax - box.ymin);
        // Wider geographic DEM needs a farther orbit to keep mesh readable.
        orbit->set_distance(span > 80.0 ? 4.2f : 2.55f);
        browser->push_shared_extent();
        cam->abandon_mesh();

        write_result(std::string("{\"ok\":true,\"op\":\"world3d.load_global_dem\","
                                 "\"source\":\"") +
                     source + "\",\"path\":\"" + path + "\"}");
        return true;
      };
  scene_writer.set_satellite_cloud =
      [browser](const std::string& imagery_path, bool enabled,
                std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::Scene3dPresenter* cam = browser->scene3d();
        if (!cam) {
          write_result(
              "{\"error\":\"no_scene_device\",\"op\":\"world3d.set_satellite_cloud\"}");
          return false;
        }
        cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
        if (!enabled) {
          cam->atmosphere_session().set_cloud_enabled(false);
          write_result(
              "{\"ok\":true,\"op\":\"world3d.set_satellite_cloud\","
              "\"mode\":\"off\"}");
          return true;
        }

        std::string path = imagery_path;
        if (path.empty()) {
          static const char* kRels[] = {
              "../data/satellite_cloud.tif",
              "../plugins/world3d/data/satellite_cloud.tif",
              "data/satellite_cloud.tif",
          };
          char base[MAX_PATH] = {};
          if (detail::exe_dir_with_slash_a(base, MAX_PATH)) {
            for (const char* rel : kRels) {
              char full[MAX_PATH] = {};
              if (strcpy_s(full, base) != 0 || strcat_s(full, rel) != 0) {
                continue;
              }
              if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
                continue;
              }
              path = full;
              break;
            }
          }
        }

        if (path.empty()) {
          // No GeoTIFF yet: keep procedural cloud deck (Google-Earth-class MVP).
          cam->atmosphere_session().seed_procedural(/*with_land_rings=*/true);
          cam->atmosphere_session().set_cloud_enabled(true);
          write_result(
              "{\"ok\":true,\"op\":\"world3d.set_satellite_cloud\","
              "\"mode\":\"procedural\","
              "\"hint\":\"place single-band GeoTIFF at "
              "out/data/satellite_cloud.tif (channel cloud_cover)\"}");
          return true;
        }

        const std::string spec = path + ":cloud_cover";
        if (!cam->atmosphere_session().load_fields(spec)) {
          write_result(
              "{\"error\":\"field_load_failed\",\"op\":\"world3d.set_satellite_cloud\","
              "\"path\":\"" +
              path + "\"}");
          return false;
        }
        cam->atmosphere_session().set_cloud_enabled(true);
        write_result(
            "{\"ok\":true,\"op\":\"world3d.set_satellite_cloud\","
            "\"mode\":\"field\",\"path\":\"" +
            path + "\"}");
        return true;
      };
  scene_writer.set_atmosphere = [browser](bool sky, bool ocean, bool cloud,
                                          bool fog) {
    content::Scene3dPresenter* cam = browser->scene3d();
    if (!cam) {
      return false;
    }
    cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
    cam->atmosphere_session().set_sky_enabled(sky);
    cam->atmosphere_session().set_ocean_enabled(ocean);
    cam->atmosphere_session().set_cloud_enabled(cloud);
    cam->atmosphere_session().set_fog_enabled(fog);
    return true;
  };
  plugin::set_world3d_scene_writer(std::move(scene_writer));

}

}  // namespace detail
}  // namespace app
