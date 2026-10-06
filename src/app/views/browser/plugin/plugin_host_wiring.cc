// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/view_host.h"
#include "app/views/browser/browser.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <functional>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/views/browser/china_product_defaults.h"
#include "app/views/browser/plugin/plugin_shell.h"
#include "app/views/browser/plugin/present.h"
#include "app/views/util/exe_sidecar_path.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/event_bus.h"
#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "plugin/product/map2d/seed/seed.h"
#include "vista/component/atmosphere/environment.h"
#include "vista/terrain/dem/dem_raster.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scene/look/look.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace {

// Narrow PluginHost Scene3dSink wiring. No Browser* — horizon fills callbacks.
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
  plugin::Scene3dSink* sink = plugin::scene3d_sink(ctx.host);
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
              "plugins/world3d/data/satellite_cloud.tif",
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

  sink->set_atmosphere_panel_bridges(
      [ctx](double t) {
        if (ctx.scene3d) {
          ctx.scene3d->atmosphere_session().set_time_sec(t);
        }
      },
      [ctx]() {
        content::Scene3dPresenter* cam = ctx.scene3d;
        if (!cam) {
          return;
        }
        const vista::atmosphere::Environment* env =
            cam->atmosphere_session().environment();
        if (!env || env->field_store().layer_count() == 0) {
          cam->atmosphere_session().seed_procedural();
        }
      },
      [ctx](bool on) {
        if (ctx.scene3d) {
          ctx.scene3d->atmosphere_session().set_wind_overlay_enabled(on);
        }
      });

  sink->set_look_bridges(
      [ctx](std::string_view mode_id, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        plugin::World3dLook look = plugin::World3dLook::kFull;
        if (!plugin::parse_world3d_look(mode_id, &look)) {
          write_result(
              "{\"error\":\"bad_args\",\"op\":\"world3d.apply_look\","
              "\"need\":\"mode\"}");
          return false;
        }
        plugin::World3dLookSeed seed;
        if (!plugin::apply_world3d_look(ctx.scene3d, ctx.orbit, look, &seed)) {
          write_result(
              "{\"error\":\"look_failed\",\"op\":\"world3d.apply_look\"}");
          return false;
        }
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        const char* name = plugin::world3d_look_name(look);
        write_result(std::string("{\"ok\":true,\"op\":\"world3d.apply_look\","
                                 "\"mode\":\"") +
                     name + "\"}");
        return true;
      },
      [ctx](float t01, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::Scene3dPresenter* cam = ctx.scene3d;
        content::OrbitFrame* orbit = ctx.orbit;
        if (!cam || !orbit) {
          write_result(
              "{\"error\":\"no_scene_device\",\"op\":\"world3d.fly_globe\"}");
          return false;
        }
        float yaw = 0.f;
        float pitch = 0.f;
        plugin::world3d_china_aim_yaw_pitch(&yaw, &pitch);
        plugin::apply_world3d_globe_flythrough(
            orbit, t01, yaw, pitch, &cam->atmosphere_session().globe_pass(),
            &cam->atmosphere_session());
        write_result("{\"ok\":true,\"op\":\"world3d.fly_globe\"}");
        return true;
      });
}

struct Map2dHostContext {
  content::MapScene* document = nullptr;
  content::Map2dPresenter* map2d = nullptr;
  content::ViewFrame* view_frame = nullptr;
  content::PluginHost* host = nullptr;
  std::function<void()> present_map2d;
  std::function<void(int, int)> apply_china_product;
  std::function<void()> push_shared_extent;
  std::function<void(int)> select_map_tab;
  std::function<void(int*, int*)> view_size;
};

void map2d_view_wh(const Map2dHostContext& ctx, int* w, int* h) {
  if (w) {
    *w = 1280;
  }
  if (h) {
    *h = 720;
  }
  if (ctx.view_size) {
    ctx.view_size(w, h);
  }
}

void install_map2d_host_bridges(const Map2dHostContext& ctx) {
  if (!ctx.host) {
    return;
  }
  plugin::Map2dSink* sink = plugin::map2d_sink(ctx.host);
  if (!sink) {
    return;
  }

  sink->set_bridges(
      [ctx](std::string_view name, double lon, double lat, double half_deg) {
        content::GisDocument* gis =
            ctx.host ? ctx.host->gis_document() : nullptr;
        if (!gis || half_deg <= 0.0) {
          return false;
        }
        const std::string n(name);
        (void)gis->create_layer(n.empty() ? "standin" : n, "Polygon");
        const double half = half_deg;
        const std::vector<std::pair<double, double>> ring = {
            {lon - half, lat - half},
            {lon + half, lat - half},
            {lon + half, lat + half},
            {lon - half, lat + half},
        };
        return plugin::append_map_polygon(gis, ring, "standin");
      },
      [ctx](std::string_view path_sv) {
        if (!ctx.host) {
          return false;
        }
        std::string path(path_sv);
        if (path.empty()) {
          static const char* kRels[] = {"../data/china_city.shp",
                                        "data/china_city.shp",
                                        "../data/china_city.gpkg",
                                        "data/china_city.gpkg"};
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
        return ctx.host->present_dataset("smartgis.map2d", path, 0);
      },
      [ctx]() {
        if (ctx.map2d) {
          ctx.map2d->invalidate_frame_cache();
        }
        if (ctx.present_map2d) {
          ctx.present_map2d();
        }
      });

  sink->set_view_bridges(
      [ctx]() {
        if (ctx.select_map_tab) {
          ctx.select_map_tab(0);
        }
        if (ctx.present_map2d) {
          ctx.present_map2d();
        }
        int w = 1280;
        int h = 720;
        map2d_view_wh(ctx, &w, &h);
        if (ctx.apply_china_product) {
          ctx.apply_china_product(w, h);
        }
        return ctx.map2d != nullptr && ctx.view_frame != nullptr;
      },
      [ctx](double lon, double lat, double span_deg) {
        content::ViewFrame* frame = ctx.view_frame;
        if (!frame) {
          return false;
        }
        const double half = (span_deg > 0.05) ? (span_deg * 0.5) : 2.0;
        content::Extent2 box;
        box.xmin = lon - half;
        box.ymin = lat - half;
        box.xmax = lon + half;
        box.ymax = lat + half;
        int w = 1280;
        int h = 720;
        map2d_view_wh(ctx, &w, &h);
        frame->apply_world_extent(box, w, h);
        if (ctx.select_map_tab) {
          ctx.select_map_tab(0);
        }
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        if (ctx.map2d) {
          ctx.map2d->invalidate_frame_cache();
        }
        return true;
      },
      [ctx](std::string_view dem_path, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::MapScene* doc = ctx.document;
        content::ViewFrame* frame = ctx.view_frame;
        if (!doc || !frame) {
          write_result(
              "{\"error\":\"no_map_device\",\"op\":\"map2d.load_hillshade\"}");
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
              "{\"error\":\"missing_dem\",\"op\":\"map2d.load_hillshade\","
              "\"hint\":\"place GeoTIFF at out/data/china_dem.tif or "
              "out/data/global_dem.tif\"}");
          return false;
        }
        if (!doc->open_path(path)) {
          write_result(
              "{\"error\":\"missing_or_invalid_dem\",\"op\":\"map2d.load_hillshade\","
              "\"path\":\"" +
              path + "\"}");
          return false;
        }
        if (ctx.select_map_tab) {
          ctx.select_map_tab(0);
        }
        content::Extent2 box = doc->world_extent();
        if (!content::extent_nonempty(box)) {
          box = content::kChinaMap2dFrameExtent;
        }
        int w = 1280;
        int h = 720;
        map2d_view_wh(ctx, &w, &h);
        frame->apply_world_extent(box, w, h);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        if (ctx.map2d) {
          ctx.map2d->invalidate_frame_cache();
        }
        write_result(std::string("{\"ok\":true,\"op\":\"map2d.load_hillshade\","
                                 "\"source\":\"") +
                     source + "\",\"path\":\"" + path + "\"}");
        return true;
      });

  sink->set_look_bridges(
      [ctx](std::string_view mode_id, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        plugin::Map2dSeedMode mode = plugin::Map2dSeedMode::kChina;
        if (!plugin::parse_map2d_seed_mode(mode_id, &mode)) {
          write_result(
              "{\"error\":\"bad_args\",\"op\":\"map2d.apply_look\","
              "\"need\":\"mode\"}");
          return false;
        }
        if (ctx.select_map_tab) {
          ctx.select_map_tab(0);
        }
        if (mode == plugin::Map2dSeedMode::kChina) {
          if (ctx.present_map2d) {
            ctx.present_map2d();
          }
          int w = 1280;
          int h = 720;
          map2d_view_wh(ctx, &w, &h);
          if (ctx.apply_china_product) {
            ctx.apply_china_product(w, h);
          }
        } else if (!plugin::seed_map2d(ctx.host, mode)) {
          write_result(
              "{\"error\":\"look_failed\",\"op\":\"map2d.apply_look\"}");
          return false;
        }
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        const char* name = plugin::map2d_seed_mode_name(mode);
        write_result(std::string("{\"ok\":true,\"op\":\"map2d.apply_look\","
                                 "\"mode\":\"") +
                     name + "\"}");
        return true;
      },
      [ctx](float t01, std::string* result_json) {
        auto write_result = [&](const std::string& json) {
          if (result_json) {
            *result_json = json;
          }
        };
        content::ViewFrame* frame = ctx.view_frame;
        if (!frame) {
          write_result(
              "{\"error\":\"no_map_device\",\"op\":\"map2d.frame_fly\"}");
          return false;
        }
        const float t = (std::max)(0.f, (std::min)(1.f, t01));
        const content::Extent2 wide = content::kChinaLonLatExtent;
        const content::Extent2 tight = content::kChinaMap2dFrameExtent;
        auto lerp = [t](double a, double b) {
          return a + (b - a) * static_cast<double>(t);
        };
        content::Extent2 box;
        box.xmin = lerp(wide.xmin, tight.xmin);
        box.ymin = lerp(wide.ymin, tight.ymin);
        box.xmax = lerp(wide.xmax, tight.xmax);
        box.ymax = lerp(wide.ymax, tight.ymax);
        int w = 1280;
        int h = 720;
        map2d_view_wh(ctx, &w, &h);
        frame->apply_world_extent(box, w, h);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        if (ctx.map2d) {
          ctx.map2d->invalidate_frame_cache();
        }
        write_result("{\"ok\":true,\"op\":\"map2d.frame_fly\"}");
        return true;
      });

  sink->set_seed_bridges([ctx]() {
    content::GisDocument* gis = ctx.host ? ctx.host->gis_document() : nullptr;
    if (gis && gis->feature_count() >= 3) {
      return;
    }
    if (ctx.host) {
      (void)ctx.host->present_dataset("smartgis.map2d", "", 0);
    }
  });

  sink->set_present_bridges(
      [ctx](uint32_t width_px, uint32_t height_px) {
        if (!ctx.map2d) {
          return false;
        }
        uint32_t w = width_px;
        uint32_t h = height_px;
        if (w == 0 || h == 0) {
          int iw = 1280;
          int ih = 720;
          map2d_view_wh(ctx, &iw, &ih);
          w = static_cast<uint32_t>(iw);
          h = static_cast<uint32_t>(ih);
        }
        return ctx.map2d->present_gpu(nullptr, w, h);
      },
      [ctx](std::string_view path, int width_px, int height_px) {
        if (!ctx.map2d || path.empty()) {
          return false;
        }
        int w = width_px;
        int h = height_px;
        if (w <= 0 || h <= 0) {
          map2d_view_wh(ctx, &w, &h);
        }
        return ctx.map2d->export_bmp(std::string(path), w, h);
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

  Map2dHostContext map2d_ctx;
  map2d_ctx.document = &session_->document();
  map2d_ctx.map2d = map2d();
  map2d_ctx.view_frame = view_frame();
  map2d_ctx.host = host;
  map2d_ctx.present_map2d = [this]() {
    detail::present_plugin_map2d(ui_.get(), map2d(),
                                 [this]() { fit_map_extent(); });
  };
  map2d_ctx.apply_china_product = [this](int w, int h) {
    apply_china_map2d_product_defaults(*this, w, h);
  };
  map2d_ctx.push_shared_extent = [this]() { push_shared_extent(); };
  map2d_ctx.select_map_tab = [this](int index) { select_map_tab(index); };
  map2d_ctx.view_size = [this](int* w, int* h) {
    if (w) {
      *w = 1280;
    }
    if (h) {
      *h = 720;
    }
    if (HWND horizon = hwnd()) {
      if (IsWindow(horizon)) {
        RECT rc = {};
        GetClientRect(horizon, &rc);
        if (w && rc.right > 32) {
          *w = rc.right;
        }
        if (h && rc.bottom > 32) {
          *h = rc.bottom;
        }
      }
    }
  };
  install_map2d_host_bridges(map2d_ctx);

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
        // Nested present_dataset (orthogrid commit → tab/fit/invalidate, or
        // for_each_processing walking the same vtable) must not re-enter.
        static thread_local int depth = 0;
        if (depth > 0) {
          return true;
        }
        ++depth;
        struct DepthGuard {
          ~DepthGuard() { --depth; }
        } guard;

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
            // Resolve *.present_frame lazily in present_plugin_frame. Looking
            // it up here via for_each_processing re-entered present_dataset
            // (0xC00000FD) on --plugin-showcase=orthogrid.
            plugin_playback_.adopt_host_frames(std::string(plugin_id),
                                               std::string(), n);
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
