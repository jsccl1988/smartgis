// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/browser/plugin/map2d_bridges.h"

#include <algorithm>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "app/views/browser/plugin/path_resolve.h"
#include "base/process/switches.h"
#include "content/browser/session/browser_session.h"
#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/present/gis_present.h"
#include "plugin/product/map2d/seed/seed.h"
#include "vista/terrain/dem/raster/dem_raster.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {
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

void select_map_tab_unless_env_locked(const Map2dHostContext& ctx, int tab) {
  if (!ctx.select_map_tab || base::switch_cstr("views-start-map-tab")) {
    return;
  }
  ctx.select_map_tab(tab);
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
          path = resolve_under_exe(kRels);
        }
        return ctx.host->present_dataset("smartgis.map2d", path, 0);
      },
      [ctx]() {
        // Sink invalidate after dataset/stand-in edits — fingerprint gate.
        if (ctx.session) {
          ctx.session->invalidate_map2d_frame_cache();
        }
        if (ctx.present_map2d) {
          ctx.present_map2d();
        }
      });

  sink->set_view_bridges(
      [ctx]() {
        select_map_tab_unless_env_locked(ctx, 0);
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
        if (!ctx.session) {
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
        ctx.session->apply_view_world_extent(box, w, h);
        select_map_tab_unless_env_locked(ctx, 0);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        // Camera framing only — presenter no-ops when fingerprint stable.
        if (ctx.session) {
          ctx.session->invalidate_map2d_frame_cache();
        }
        return true;
      },
      [ctx](std::string_view dem_path, std::string* result_json) {
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
        if (!ctx.session) {
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
        if (!ctx.session->open_document(path)) {
          write_result(
              "{\"error\":\"missing_or_invalid_dem\",\"op\":\"map2d.load_hillshade\","
              "\"path\":\"" +
              path + "\"}");
          return false;
        }
        select_map_tab_unless_env_locked(ctx, 0);
        content::Extent2 box = ctx.session->document_world_extent();
        if (!content::BrowserSession::is_extent_nonempty(box)) {
          box = content::BrowserSession::china_map2d_frame_extent();
        }
        int w = 1280;
        int h = 720;
        map2d_view_wh(ctx, &w, &h);
        ctx.session->apply_view_world_extent(box, w, h);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        // open_path changes scene content — fingerprint must force drop.
        if (ctx.session) {
          ctx.session->invalidate_map2d_frame_cache();
        }
        write_result(std::string("{\"ok\":true,\"op\":\"map2d.load_hillshade\","
                                 "\"source\":\"") +
                     source + "\",\"path\":\"" + path + "\"}");
        return true;
      });

  sink->set_look_bridges(
      [ctx](std::string_view mode_id, std::string* result_json) {
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
        plugin::Map2dSeedMode mode = plugin::Map2dSeedMode::kChina;
        if (!plugin::parse_map2d_seed_mode(mode_id, &mode)) {
          write_result(
              "{\"error\":\"bad_args\",\"op\":\"map2d.apply_look\","
              "\"need\":\"mode\"}");
          return false;
        }
        select_map_tab_unless_env_locked(ctx, 0);
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
        auto write_result = [&](const std::string& json) { write_json_out(result_json, json); };
        if (!ctx.session) {
          write_result(
              "{\"error\":\"no_map_device\",\"op\":\"map2d.frame_fly\"}");
          return false;
        }
        const float t = (std::max)(0.f, (std::min)(1.f, t01));
        const content::Extent2 wide = content::BrowserSession::china_lon_lat_extent();
        const content::Extent2 tight =
            content::BrowserSession::china_map2d_frame_extent();
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
        ctx.session->apply_view_world_extent(box, w, h);
        if (ctx.push_shared_extent) {
          ctx.push_shared_extent();
        }
        // frame_fly is extent-only; avoid layout_builds during fly samples.
        if (ctx.session) {
          ctx.session->invalidate_map2d_frame_cache();
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
        if (!ctx.session) {
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
        return ctx.session->map2d_present_gpu(nullptr, w, h);
      },
      [ctx](std::string_view path, int width_px, int height_px) {
        if (!ctx.session || path.empty()) {
          return false;
        }
        int w = width_px;
        int h = height_px;
        if (w <= 0 || h <= 0) {
          map2d_view_wh(ctx, &w, &h);
        }
        return ctx.session->map2d_export_bmp(std::string(path), w, h);
      });
}

}  // namespace detail
}  // namespace app
