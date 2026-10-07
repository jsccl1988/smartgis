// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/scene3d_bridges.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <utility>

#include "app/views/browser/plugin/path_resolve.h"
#include "app/views/browser/plugin/present.h"
#include "content/browser/session/browser_session.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "vista/terrain/dem/raster/dem_raster.h"
#include "plugin/product/world3d/scene/fly/globe_fly.h"
#include "plugin/product/world3d/scene/look/look.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {
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
        return add_standin_mesh(ctx.document, n.c_str(), lon, lat, half_deg);
      },
      [ctx](std::string_view tileset_json_path) {
        if (!ctx.session) {
          return false;
        }
        std::string path(tileset_json_path);
        if (path.empty()) {
          static const char* kRels[] = {"../data/m3_city_tileset.json",
                                          "data/m3_city_tileset.json"};
          path = resolve_under_exe(kRels);
          if (path.empty()) {
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
          ctx.session->set_scene3d_tileset_content_root(root);
        }
        return ctx.session->attach_scene3d_tileset_json(
            json.c_str(), json.size(), "world3d_city");
      },
      [ctx]() {
        if (ctx.present_scene3d) {
          ctx.present_scene3d();
        }
      });

  sink->set_overlay_bridges(
      [ctx](const float* xyz, int point_count, const unsigned* indices,
            int index_count, const uint8_t* albedo) {
        if (!ctx.session || !xyz || point_count < 3 || !indices ||
            index_count < 3) {
          return false;
        }
        ctx.session->set_scene3d_overlay_tin_mesh(xyz, point_count, indices,
                                                  index_count, albedo);
        return true;
      },
      [ctx](const uint8_t* rgba, uint32_t width, uint32_t height, const float* uv,
            int uv_float_count) {
        if (!ctx.session || !rgba || width == 0 || height == 0) {
          return false;
        }
        ctx.session->set_scene3d_overlay_tin_drape(rgba, width, height, uv,
                                                   uv_float_count);
        return true;
      },
      [ctx]() {
        if (ctx.session) {
          ctx.session->clear_scene3d_overlay_tin();
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
        if (!ctx.session) {
          return false;
        }
        const double half = (span_deg > 0.05) ? (span_deg * 0.5) : 2.0;
        content::Extent2 box;
        box.xmin = lon - half;
        box.ymin = lat - half;
        box.xmax = lon + half;
        box.ymax = lat + half;
        ctx.session->apply_orbit_world_extent(box);
        if (distance > 0.05f) {
          ctx.session->set_orbit_distance(distance);
        }
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        return true;
      },
      [ctx](std::string_view dem_path, std::string* result_json) {
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
        if (!ctx.session) {
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
        if (!content::BrowserSession::is_extent_nonempty(box)) {
          box = content::BrowserSession::china_lon_lat_extent();
        }
        ctx.session->reset_orbit();
        ctx.session->apply_orbit_world_extent(box);
        const double span =
            (std::max)(box.xmax - box.xmin, box.ymax - box.ymin);
        ctx.session->set_orbit_distance(span > 80.0 ? 4.2f : 2.55f);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        ctx.session->abandon_scene3d_mesh();

        write_result(std::string("{\"ok\":true,\"op\":\"world3d.load_global_dem\","
                                 "\"source\":\"") +
                     source + "\",\"path\":\"" + path + "\"}");
        return true;
      },
      [ctx](std::string_view imagery_path, bool enabled,
            std::string* result_json) {
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
        if (!ctx.session) {
          write_result(
              "{\"error\":\"no_scene_device\",\"op\":\"world3d.set_satellite_cloud\"}");
          return false;
        }
        ctx.session->set_scene3d_look_atmosphere();
        if (!enabled) {
          ctx.session->set_scene3d_cloud_enabled(false);
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
          path = resolve_under_exe(kRels);
        }

        if (path.empty()) {
          ctx.session->seed_scene3d_procedural(/*with_land_rings=*/true);
          ctx.session->set_scene3d_cloud_enabled(true);
          write_result(
              "{\"ok\":true,\"op\":\"world3d.set_satellite_cloud\","
              "\"mode\":\"procedural\","
              "\"hint\":\"place single-band GeoTIFF at "
              "out/data/satellite_cloud.tif (channel cloud_cover)\"}");
          return true;
        }

        const std::string spec = path + ":cloud_cover";
        if (!ctx.session->load_scene3d_fields(spec)) {
          write_result(
              "{\"error\":\"field_load_failed\",\"op\":\"world3d.set_satellite_cloud\","
              "\"path\":\"" +
              path + "\"}");
          return false;
        }
        ctx.session->set_scene3d_cloud_enabled(true);
        write_result(
            "{\"ok\":true,\"op\":\"world3d.set_satellite_cloud\","
            "\"mode\":\"field\",\"path\":\"" +
            path + "\"}");
        return true;
      },
      [ctx](bool sky, bool ocean, bool cloud, bool fog) {
        if (!ctx.session) {
          return false;
        }
        ctx.session->set_scene3d_look_atmosphere();
        ctx.session->set_scene3d_atmosphere_layers(ocean, cloud, sky, fog);
        return true;
      });

  sink->set_atmosphere_panel_bridges(
      [ctx](double t) {
        if (ctx.session) {
          ctx.session->set_scene3d_time_sec(t);
        }
      },
      [ctx]() {
        if (!ctx.session) {
          return;
        }
        ctx.session->seed_scene3d_procedural_if_empty();
      },
      [ctx](bool on) {
        if (ctx.session) {
          ctx.session->set_scene3d_wind_overlay(on);
        }
      });

  sink->set_look_bridges(
      [ctx](std::string_view mode_id, std::string* result_json) {
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
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
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
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

}  // namespace detail
}  // namespace app
