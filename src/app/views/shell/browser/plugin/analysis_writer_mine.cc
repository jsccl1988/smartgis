// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writer_mine.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "gis/present/style/style_document.h"
#include "plugin/product/mine/commands.h"
#include "tool/draft/draft.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace app {
namespace detail {
namespace {

// Semi-transparent stratum TIN + dark borehole sticks.
constexpr const char* kMineStyleJson = R"json({
  "version": 8,
  "name": "mine_stratum",
  "layers": [
    {"id":"stratum-fill","type":"fill","source-layer":"mine_stratum",
     "paint":{"fill-color":"#8e44ad","fill-opacity":0.45}},
    {"id":"stratum-line","type":"line","source-layer":"mine_stratum",
     "paint":{"line-color":"#4a235a","line-width":1.5}},
    {"id":"sticks","type":"line","source-layer":"mine_boreholes",
     "paint":{"line-color":"#1b1b1b","line-width":3.0}}
  ]
})json";

}  // namespace

// Borehole sticks (map LineString footprint) + stratum TIN (map2d polygons) +
// scene3d overlay: TIN mesh with real Z and vertical stick samples.
bool commit_mine_stratum(content::MapScene* doc,
                         BrowserUiDelegate* ui,
                         content::Scene3dPresenter* scene3d,
                         const gis::detail::StratumTin& tin,
                         const gis::detail::BoreholeSet& holes,
                         std::string* err) {
  if (!doc) {
    if (err) {
      *err = "no_doc";
    }
    return false;
  }
  if (!holes.ok || holes.contacts.empty()) {
    if (err) {
      *err = holes.error.empty() ? "no_holes" : holes.error;
    }
    return false;
  }

  doc->clear();
  (void)apply_style_json(doc, kMineStyleJson);

  if (tin.ok && !tin.xyz.empty() && tin.indices.size() >= 3) {
    const int point_count = static_cast<int>(tin.xyz.size() / 3);
    const int triangle_count = static_cast<int>(tin.indices.size() / 3);
    std::vector<int> tris(tin.indices.begin(), tin.indices.end());
    if (!doc->add_triangle_layer("mine_stratum", tin.xyz.data(), point_count,
                                 tris.data(), triangle_count)) {
      if (err) {
        *err = "tin_layer_failed";
      }
      return false;
    }
  }

  // Group contacts by hole; draw a short map stick (2D footprint).
  std::map<std::string, std::vector<const gis::detail::BoreholeContact*>> by_hole;
  for (const auto& c : holes.contacts) {
    by_hole[c.hole_id].push_back(&c);
  }
  if (!doc->create_layer("mine_boreholes", "LineString")) {
    if (err) {
      *err = "stick_layer_failed";
    }
    return false;
  }
  int sticks = 0;
  double z_min = holes.contacts.front().z;
  double z_max = holes.contacts.front().z;
  for (const auto& c : holes.contacts) {
    z_min = (std::min)(z_min, c.z);
    z_max = (std::max)(z_max, c.z);
  }
  if (tin.ok) {
    for (size_t i = 0; i + 2 < tin.xyz.size(); i += 3) {
      z_min = (std::min)(z_min, tin.xyz[i + 2]);
      z_max = (std::max)(z_max, tin.xyz[i + 2]);
    }
  }
  const double peak = (std::max)(z_max - z_min, 1.0);

  for (auto& [hole_id, contacts] : by_hole) {
    (void)hole_id;
    if (contacts.empty()) {
      continue;
    }
    std::sort(contacts.begin(), contacts.end(),
              [](const gis::detail::BoreholeContact* a,
                 const gis::detail::BoreholeContact* b) {
                return a->z > b->z;
              });
    const double x = contacts.front()->x;
    const double y = contacts.front()->y;
    const double depth =
        contacts.front()->z - contacts.back()->z;
    const double stick_len =
        std::max(0.002, std::abs(depth) * 0.0002);
    std::vector<std::pair<double, double>> pts = {
        {x, y},
        {x, y - stick_len},
    };
    if (append_map_polyline(doc, pts, nullptr)) {
      ++sticks;
    }
  }
  if (sticks == 0) {
    if (err) {
      *err = "no_sticks";
    }
    return false;
  }

  // Scene3d: TIN surface mesh + densified vertical stick samples.
  // Overlay elev must be geographic meters (peer stormsurge +8m lift).
  // Do not pre-scale Z into orbit units — lon_lat_to_orbit expects meters.
  if (scene3d) {
    // Clear DEM hypsometric by tens of meters so solid purple albedo reads.
    constexpr float kOverlayLiftM = 40.f;
    if (tin.ok && !tin.xyz.empty() && tin.indices.size() >= 3) {
      const int point_count = static_cast<int>(tin.xyz.size() / 3);
      std::vector<float> tin_geo(static_cast<size_t>(point_count) * 3u);
      for (int i = 0; i < point_count; ++i) {
        tin_geo[static_cast<size_t>(i) * 3u] =
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u]);
        tin_geo[static_cast<size_t>(i) * 3u + 1u] =
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u + 1u]);
        tin_geo[static_cast<size_t>(i) * 3u + 2u] =
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u + 2u]) +
            kOverlayLiftM;
      }
      std::vector<unsigned> tin_idx(tin.indices.begin(), tin.indices.end());
      // Match kMineStyleJson stratum-fill (#8e44ad); alpha keeps DEM readable.
      constexpr uint8_t kStratumAlbedo[4] = {0x8e, 0x44, 0xad, 0xe6};
      scene3d->set_overlay_tin_mesh(tin_geo.data(), point_count, tin_idx.data(),
                                    static_cast<int>(tin_idx.size()),
                                    kStratumAlbedo);
    } else {
      scene3d->clear_overlay_tin_mesh();
    }

    std::vector<float> stick_xyz;
    std::vector<uint8_t> stick_rgba;
    constexpr int kStickSamples = 12;
    // Lift beads above the TIN (meters) so they clear depth against overlay.
    const float stick_lift_m =
        kOverlayLiftM + static_cast<float>((std::max)(peak * 0.15, 4.0));
    for (const auto& [hole_id, contacts] : by_hole) {
      (void)hole_id;
      if (contacts.size() < 1) {
        continue;
      }
      const double lon = contacts.front()->x;
      const double lat = contacts.front()->y;
      double top_z = contacts.front()->z;
      double bot_z = contacts.front()->z;
      for (const auto* c : contacts) {
        top_z = (std::max)(top_z, c->z);
        bot_z = (std::min)(bot_z, c->z);
      }
      for (int s = 0; s < kStickSamples; ++s) {
        const double t =
            static_cast<double>(s) / static_cast<double>(kStickSamples - 1);
        const double z = top_z + (bot_z - top_z) * t;
        stick_xyz.push_back(static_cast<float>(lon));
        stick_xyz.push_back(static_cast<float>(lat));
        stick_xyz.push_back(static_cast<float>(z) + stick_lift_m);
        // Amber beads contrast purple stratum TIN (map2d sticks are near-black).
        stick_rgba.push_back(0xf1);
        stick_rgba.push_back(0xc4);
        stick_rgba.push_back(0x0f);
        stick_rgba.push_back(255);
      }
    }
    // Seed TIN verts as bright markers so sparse meshes still read in 3D.
    if (tin.ok) {
      const int point_count = static_cast<int>(tin.xyz.size() / 3);
      for (int i = 0; i < point_count; ++i) {
        stick_xyz.push_back(
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u]));
        stick_xyz.push_back(
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u + 1u]));
        stick_xyz.push_back(
            static_cast<float>(tin.xyz[static_cast<size_t>(i) * 3u + 2u]) +
            stick_lift_m * 1.25f);
        stick_rgba.push_back(0x8e);
        stick_rgba.push_back(0x44);
        stick_rgba.push_back(0xad);
        stick_rgba.push_back(255);
      }
    }
    if (!stick_xyz.empty()) {
      scene3d->set_overlay_pointcloud(
          stick_xyz.data(), static_cast<int>(stick_xyz.size() / 3),
          stick_rgba.data());
    } else {
      scene3d->clear_overlay_pointcloud();
    }
  }

  return refresh_ui_after_layer(ui);
}

void wire_mine_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  plugin::set_mine_stratum_writer(
      [browser](const gis::detail::StratumTin& tin,
             const gis::detail::BoreholeSet& holes, std::string* err) {
        return commit_mine_stratum(&browser->session().document(), browser->ui(),
                                   &browser->session().scene3d(), tin, holes, err);
      });

}

}  // namespace detail
}  // namespace app
