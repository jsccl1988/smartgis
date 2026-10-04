// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writer_common.h"
#include "app/views/shell/browser/plugin/analysis_writer_flood.h"
#include "app/views/shell/browser/plugin/analysis_writer_orthogrid.h"
#include "app/views/shell/browser/plugin/analysis_writer_stormsurge.h"
#include "app/views/shell/runtime/analysis/playback.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "plugin/product/world3d/commands.h"

#include <fstream>
#include <string>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace detail {

bool apply_session_frame(content::MapScene* doc,
                         BrowserUiDelegate* ui,
                         AnalysisPlayback* session,
                         content::Scene3dPresenter* scene3d) {
  if (!doc || !session) {
    return false;
  }
  switch (session->product()) {
    case AnalysisProduct::kTraffic: {
      const auto& xy = session->traffic_xy();
      const int points = static_cast<int>(xy.size() / 2);
      const int end = session->traffic_prefix_point_count();
      if (points < 2 || end < 2) {
        return false;
      }
      doc->remove_layer("traffic_path_anim");
      if (!doc->create_layer("traffic_path_anim", "LineString")) {
        return false;
      }
      std::vector<std::pair<double, double>> prefix;
      prefix.reserve(static_cast<size_t>(end));
      for (int i = 0; i < end; ++i) {
        prefix.emplace_back(xy[static_cast<size_t>(i) * 2],
                            xy[static_cast<size_t>(i) * 2 + 1]);
      }
      if (!append_map_polyline(doc, prefix, "anim")) {
        return false;
      }
      return refresh_ui_after_layer(ui);
    }
    case AnalysisProduct::kFlood: {
      const auto* mask = session->flood_mask_at(session->frame_index());
      if (!mask) {
        return false;
      }
      return paint_flood_mask_layer(
          doc, ui, mask->data(), session->flood_width(),
          session->flood_height(), session->flood_geotransform(),
          session->flood_water_level_at(session->frame_index()), false,
          /*add_water_standin=*/true);
    }
    case AnalysisProduct::kStormSurge: {
      const int fi = session->frame_index();
      const auto* mask = session->flood_mask_at(fi);
      if (!mask) {
        return false;
      }
      if (!paint_flood_mask_layer(
              doc, ui, mask->data(), session->flood_width(),
              session->flood_height(), session->flood_geotransform(),
              session->flood_water_level_at(fi), false,
              /*add_water_standin=*/false)) {
        return false;
      }
      // Re-push free-surface TIN for this scrub index (map2d + scene3d overlay).
      const auto* xyz = session->stormsurge_water_xyz_at(fi);
      const auto* indices = session->stormsurge_water_indices_at(fi);
      if (xyz && indices) {
        const int point_count = static_cast<int>(xyz->size() / 3);
        const int triangle_count = static_cast<int>(indices->size() / 3);
        return commit_stormsurge_water_mesh(
            doc, ui, /*session=*/nullptr, scene3d, xyz->data(), point_count,
            indices->data(), triangle_count, fi);
      }
      if (scene3d) {
        scene3d->clear_overlay_tin_mesh();
      }
      return true;
    }
    case AnalysisProduct::kOrthogrid: {
      const auto* xs = session->orthogrid_xs_at(session->frame_index());
      const auto* ys = session->orthogrid_ys_at(session->frame_index());
      if (!xs || !ys || session->orthogrid_nx() < 2 ||
          session->orthogrid_ny() < 2) {
        return true;
      }
      plugin::OrthogridMeshCommit mesh;
      mesh.nx = session->orthogrid_nx();
      mesh.ny = session->orthogrid_ny();
      mesh.xs = xs->data();
      mesh.ys = ys->data();
      return commit_orthogrid_mesh(doc, ui, mesh);
    }
    case AnalysisProduct::kOrthogrid3d:
      return true;
    default:
      return false;
  }
}

int export_session_frames(content::Map2dPresenter* map2d,
                          content::MapScene* doc,
                          BrowserUiDelegate* ui,
                          AnalysisPlayback* session,
                          const std::string& dir_leaf) {
  if (!map2d || !doc || !session || session->frame_count() <= 0 ||
      dir_leaf.empty()) {
    return 0;
  }
  std::wstring leaf_w(dir_leaf.begin(), dir_leaf.end());
  const int nframes = session->frame_count();
  int wrote = 0;
  for (int i = 0; i < nframes; ++i) {
    if (!session->set_frame_index(i) ||
        !apply_session_frame(doc, ui, session, /*scene3d=*/nullptr)) {
      continue;
    }
    wchar_t frame_leaf[MAX_PATH] = {};
    _snwprintf_s(frame_leaf, _TRUNCATE, L"%s\\frame_%04d.bmp", leaf_w.c_str(),
                 i);
    wchar_t bmp_w[MAX_PATH] = {};
    if (!detail::exe_capture_path(bmp_w, MAX_PATH, frame_leaf)) {
      continue;
    }
    char bmp_a[MAX_PATH] = {};
    if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                            nullptr) <= 0) {
      continue;
    }
    if (map2d->export_bmp(bmp_a, 640, 480)) {
      ++wrote;
    }
  }

  std::wstring json_leaf = leaf_w + L"\\playback.json";
  wchar_t json_w[MAX_PATH] = {};
  if (detail::exe_capture_path(json_w, MAX_PATH, json_leaf.c_str())) {
    char json_a[MAX_PATH] = {};
    if (WideCharToMultiByte(CP_ACP, 0, json_w, -1, json_a, MAX_PATH, nullptr,
                            nullptr) > 0) {
      std::ofstream out(json_a, std::ios::binary);
      if (out) {
        out << session->playback_json();
      }
    }
  }
  return wrote;
}

}  // namespace detail

bool Browser::apply_analysis_frame(int index) {
  if (!analysis_playback_.set_frame_index(index)) {
    return false;
  }
  return detail::apply_session_frame(document(), ui_.get(), &analysis_playback_,
                                     &session_.scene3d());
}

int Browser::export_analysis_frames(const std::string& dir_leaf) {
  return detail::export_session_frames(map2d(), document(), ui_.get(),
                                       &analysis_playback_, dir_leaf);
}

}  // namespace app
