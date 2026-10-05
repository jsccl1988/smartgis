// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>

#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/runtime/plugin_present.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/event_bus.h"
#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "content/public/view_host.h"
#include "vista/terrain/dem/dem_raster.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace {

// Narrow PluginHost Scene3dSink wiring. No Browser* — chrome fills callbacks.
struct Scene3dHostContext {
  content::MapScene* document = nullptr;
  content::Scene3dPresenter* scene3d = nullptr;
  content::OrbitFrame* orbit = nullptr;
  content::PluginHost* host = nullptr;
  std::function<void()> present_scene3d;
  std::function<void()> apply_china_product;
  std::function<void()> apply_china_atmo;
  std::function<void()> push_shared_extent;
  std::function<void(int)> select_map_tab;
};

void install_scene3d_host_bridges(const Scene3dHostContext& ctx) {
  if (!ctx.host) {
    return;
  }
  content::PluginHost::Scene3dSink* sink = ctx.host->scene3d_sink();
  if (!sink) {
    return;
  }

  sink->set_bridges(
      [ctx](std::string_view name, double lon, double lat, double half_deg) {
        const std::string n(name);
        return detail::add_standin_mesh(ctx.document, n.c_str(), lon, lat,
                                        half_deg);
      },
      [ctx](std::string_view tileset_json_path) {
        content::Scene3dPresenter* cam = ctx.scene3d;
        if (!cam) {
          return false;
        }
        std::string path(tileset_json_path);
        if (path.empty()) {
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
      },
      [ctx]() {
        if (ctx.present_scene3d) {
          ctx.present_scene3d();
        }
      });

  sink->set_overlay_bridges(
      [ctx](const float* xyz, int point_count, const unsigned* indices,
            int index_count, const uint8_t* albedo) {
        content::Scene3dPresenter* cam = ctx.scene3d;
        if (!cam || !xyz || point_count < 3 || !indices || index_count < 3) {
          return false;
        }
        cam->set_overlay_tin_mesh(xyz, point_count, indices, index_count, albedo);
        return true;
      },
      [ctx](const uint8_t* rgba, uint32_t width, uint32_t height, const float* uv,
            int uv_float_count) {
        content::Scene3dPresenter* cam = ctx.scene3d;
        if (!cam || !rgba || width == 0 || height == 0) {
          return false;
        }
        cam->set_overlay_tin_drape(rgba, width, height, uv, uv_float_count);
        return true;
      },
      [ctx]() {
        if (ctx.scene3d) {
          ctx.scene3d->clear_overlay_tin_mesh();
        }
      });

  sink->set_earth_bridges(
      [ctx]() {
        if (ctx.present_scene3d) {
          ctx.present_scene3d();
        }
        if (ctx.apply_china_product) {
          ctx.apply_china_product();
        }
        return ctx.scene3d != nullptr && ctx.orbit != nullptr;
      },
      [ctx](double lon, double lat, float distance, double span_deg) {
        content::OrbitFrame* orbit = ctx.orbit;
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
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        return true;
      },
      [ctx](std::string_view dem_path, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::Scene3dPresenter* cam = ctx.scene3d;
        content::OrbitFrame* orbit = ctx.orbit;
        if (!cam || !orbit) {
          write_result(
              "{\"error\":\"no_scene_device\",\"op\":\"world3d.load_global_dem\"}");
          return false;
        }

        std::string path(dem_path);
        const char* source = "explicit";
        if (path.empty()) {
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
        if (ctx.select_map_tab) {
          ctx.select_map_tab(1);
        }
        if (ctx.apply_china_atmo) {
          ctx.apply_china_atmo();
        }

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
        orbit->set_distance(span > 80.0 ? 4.2f : 2.55f);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        cam->abandon_mesh();

        write_result(std::string("{\"ok\":true,\"op\":\"world3d.load_global_dem\","
                                 "\"source\":\"") +
                     source + "\",\"path\":\"" + path + "\"}");
        return true;
      },
      [ctx](std::string_view imagery_path, bool enabled,
            std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::Scene3dPresenter* cam = ctx.scene3d;
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

        std::string path(imagery_path);
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
      },
      [ctx](bool sky, bool ocean, bool cloud, bool fog) {
        content::Scene3dPresenter* cam = ctx.scene3d;
        if (!cam) {
          return false;
        }
        cam->set_look_preset(content::Scene3dLookPreset::kAtmosphere);
        cam->atmosphere_session().set_sky_enabled(sky);
        cam->atmosphere_session().set_ocean_enabled(ocean);
        cam->atmosphere_session().set_cloud_enabled(cloud);
        cam->atmosphere_session().set_fog_enabled(fog);
        return true;
      });
}

}  // namespace

void Browser::install_plugin_host_bridges() {
  if (!plugins_ || !plugins_->host()) {
    return;
  }
  content::PluginHost* host = plugins_->host();
  content::EventBus* events = nullptr;
  if (session_->edit_host()) {
    events = session_->edit_host()->events();
  }
  gis_document_ = std::make_unique<content::MapSceneGisDocument>(
      &session_->document(), events);
  host->set_gis_document(gis_document_.get());

  Scene3dHostContext ctx;
  ctx.document = &session_->document();
  ctx.scene3d = &session_->scene3d();
  ctx.orbit = &session_->orbit_frame();
  ctx.host = host;
  ctx.present_scene3d = [this]() { detail::present_plugin_scene3d(ui_.get()); };
  ctx.apply_china_product = [this]() {
    apply_china_scene3d_product_defaults(*this);
  };
  ctx.apply_china_atmo = [this]() { apply_china_scene3d_atmosphere(*this); };
  ctx.push_shared_extent = [this]() { push_shared_extent(); };
  ctx.select_map_tab = [this](int index) { select_map_tab(index); };
  install_scene3d_host_bridges(ctx);

  if (content::PluginHost::Playback* pb = host->playback()) {
    plugin_playback_.bind_host_playback(
        [pb](int index, int count, bool rebuild) {
          if (rebuild) {
            pb->clear();
            for (int i = 0; i < count; ++i) {
              pb->push_frame("{\"index\":" + std::to_string(i) + "}");
            }
          }
          if (count > 0) {
            pb->set_index(static_cast<size_t>(index < 0 ? 0 : index));
          }
        });
  }

  if (events) {
    layers_sub_ = events->subscribe<content::LayersChanged>(
        [this](const content::LayersChanged&) {
          sync_catalog_from_scene();
          refresh_inspectors();
        });
  }
}

void Browser::wire_plugin_present_dataset() {
  if (!plugins_ || !plugins_->host()) {
    return;
  }
  plugins_->host()->set_present_dataset_bridge(
      [this](std::string_view plugin_id, std::string_view path, int face,
             int surface) {
        const bool scene3d = face == 1;
        if (!scene3d && !path.empty()) {
          const size_t dot = path.find_last_of('.');
          if (dot != std::string_view::npos) {
            const std::string ext(path.substr(dot));
            if (_stricmp(ext.c_str(), ".geojson") == 0 ||
                _stricmp(ext.c_str(), ".gpkg") == 0 ||
                _stricmp(ext.c_str(), ".shp") == 0 ||
                _stricmp(ext.c_str(), ".kml") == 0 ||
                _stricmp(ext.c_str(), ".gml") == 0) {
              (void)session_->document().open_path(std::string(path));
              sync_catalog_from_scene();
            }
          }
        }
        if (plugins_ && plugins_->host() && plugins_->host()->playback()) {
          const int n = static_cast<int>(
              plugins_->host()->playback()->frame_count());
          if (n > 0 && !plugin_id.empty()) {
            plugin_playback_.adopt_host_frames(
                std::string(plugin_id),
                detail::present_frame_processing_id(plugins_->host(),
                                                    plugin_id),
                n);
          }
        }
        return detail::present_plugin_dataset(
            this, map2d(), [this]() { fit_map_extent(); }, path, face, surface);
      });
}

bool Browser::apply_plugin_frame(int index) {
  return detail::present_plugin_frame(plugins_.get(), &plugin_playback_, index);
}

int Browser::export_plugin_frames(const std::string& dir_leaf) {
  return detail::export_plugin_frames(plugins_.get(), &plugin_playback_,
                                      map2d(), dir_leaf);
}

}  // namespace app
