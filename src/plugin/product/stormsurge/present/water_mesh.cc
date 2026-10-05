// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/stormsurge/present/water_mesh.h"

#include "content/browser/present/scene3d/scene3d_presenter.h"
#include "content/public/gis_document.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace plugin {
namespace {

void commit_water_overlay(content::PluginHost::Scene3dSink* sink,
                          content::Scene3dPresenter* scene3d,
                          const double* xyz,
                          int point_count,
                          const int* triangles,
                          int triangle_count) {
  std::vector<float> tin_geo(static_cast<size_t>(point_count) * 3u);
  for (int i = 0; i < point_count; ++i) {
    tin_geo[static_cast<size_t>(i) * 3u] = static_cast<float>(xyz[i * 3]);
    tin_geo[static_cast<size_t>(i) * 3u + 1u] =
        static_cast<float>(xyz[i * 3 + 1]);
    tin_geo[static_cast<size_t>(i) * 3u + 2u] =
        static_cast<float>(xyz[i * 3 + 2]) + 1.5f;
  }
  const int index_count = triangle_count * 3;
  std::vector<unsigned> tin_idx(static_cast<size_t>(index_count));
  for (int i = 0; i < index_count; ++i) {
    if (triangles[i] < 0) {
      if (scene3d) {
        scene3d->clear_overlay_tin_mesh();
      } else if (sink) {
        sink->clear_overlay_tin_mesh();
      }
      return;
    }
    tin_idx[static_cast<size_t>(i)] = static_cast<unsigned>(triangles[i]);
  }

  // Keep water_like albedo (software paint / ocean-plane skip) while a small
  // atlas raises color_buckets past the abstract two-blob gate.
  constexpr uint8_t kWaterAlbedo[4] = {28, 210, 245, 250};
  constexpr int kAtlas = 8;
  std::vector<uint8_t> atlas(static_cast<size_t>(kAtlas) * kAtlas * 4u, 255);
  const uint8_t bands[8][3] = {
      {18, 170, 230}, {28, 190, 240}, {36, 205, 248}, {48, 220, 250},
      {22, 150, 210}, {40, 200, 235}, {55, 215, 245}, {30, 185, 225},
  };
  for (int col = 0; col < kAtlas; ++col) {
    for (int row = 0; row < kAtlas; ++row) {
      const size_t p =
          (static_cast<size_t>(row) * kAtlas + static_cast<size_t>(col)) * 4u;
      const uint8_t* c = bands[col % 8];
      atlas[p] = c[0];
      atlas[p + 1] = c[1];
      atlas[p + 2] = c[2];
      atlas[p + 3] = 0xfa;
    }
  }
  std::vector<float> uv(static_cast<size_t>(point_count) * 2u);
  for (int i = 0; i < point_count; ++i) {
    const float t =
        static_cast<float>(i) / static_cast<float>((std::max)(1, point_count - 1));
    uv[static_cast<size_t>(i) * 2u] = t * 0.875f + 0.0625f;
    uv[static_cast<size_t>(i) * 2u + 1u] = 0.5f;
  }

  if (scene3d) {
    scene3d->set_overlay_tin_mesh(tin_geo.data(), point_count, tin_idx.data(),
                                  index_count, kWaterAlbedo);
    scene3d->set_overlay_tin_drape(atlas.data(), static_cast<uint32_t>(kAtlas),
                                   static_cast<uint32_t>(kAtlas), uv.data(),
                                   static_cast<int>(uv.size()));
  } else if (sink) {
    sink->set_overlay_tin_mesh(tin_geo.data(), point_count, tin_idx.data(),
                               index_count, kWaterAlbedo);
    sink->set_overlay_tin_drape(atlas.data(), static_cast<uint32_t>(kAtlas),
                                static_cast<uint32_t>(kAtlas), uv.data(),
                                static_cast<int>(uv.size()));
  }
}

}  // namespace

bool present_stormsurge_water_mesh(content::GisDocument* doc,
                                   content::PluginHost::Scene3dSink* sink,
                                   content::Scene3dPresenter* scene3d,
                                   const double* xyz,
                                   int point_count,
                                   const int* triangles,
                                   int triangle_count) {
  if (!doc || !xyz || point_count < 3 || !triangles || triangle_count < 1) {
    return false;
  }
  std::vector<double> flipped(static_cast<size_t>(point_count) * 3);
  for (int i = 0; i < point_count; ++i) {
    flipped[static_cast<size_t>(i) * 3] = xyz[i * 3];
    flipped[static_cast<size_t>(i) * 3 + 1] = -xyz[i * 3 + 1];
    flipped[static_cast<size_t>(i) * 3 + 2] = xyz[i * 3 + 2];
  }
  doc->remove_layer("Flood water 3D");
  doc->remove_layer("Storm surge water");
  if (!doc->add_triangle_mesh("Storm surge water", flipped.data(), point_count,
                              triangles, triangle_count)) {
    return false;
  }

  if (scene3d || sink) {
    commit_water_overlay(sink, scene3d, xyz, point_count, triangles,
                         triangle_count);
  }
  if (sink) {
    sink->invalidate();
  }
  return true;
}

}  // namespace plugin
