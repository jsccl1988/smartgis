// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/frame/map2d_batches.h"

#include <utility>
#include <vector>

#include "gis/datasource/ogr/ogr_text_encoding.h"

namespace content {
namespace detail {
namespace {

vista::BatchGeomKind to_batch_kind(MapScene::GeomKind kind) {
  switch (kind) {
    case MapScene::GeomKind::kLine:
      return vista::BatchGeomKind::kLine;
    case MapScene::GeomKind::kPolygon:
      return vista::BatchGeomKind::kPolygon;
    case MapScene::GeomKind::kText:
      return vista::BatchGeomKind::kText;
    case MapScene::GeomKind::kPoint:
      return vista::BatchGeomKind::kPoint;
  }
  return vista::BatchGeomKind::kPoint;
}

}  // namespace

vista::LayerBatchSet visible_layer_batches(
    const std::vector<MapScene::Layer>& layers, bool use_carto_slots,
    double scale) {
  std::vector<vista::BatchLayer> pod;
  pod.reserve(layers.size());
  for (const MapScene::Layer& layer : layers) {
    vista::BatchLayer out;
    out.id = layer.id;
    out.name = layer.name;
    out.visible = layer.visible;
    out.features.reserve(layer.features.size());
    for (const MapScene::Feature& feature : layer.features) {
      vista::BatchFeature feat;
      feat.kind = to_batch_kind(feature.kind);
      feat.points.reserve(feature.points.size());
      for (const MapScene::Vertex& p : feature.points) {
        // Stored map Y is -lat. Layout batches are +lat.
        feat.points.push_back(vista::BatchPoint{p.x, -p.y});
      }
      feat.fields.reserve(feature.fields.size());
      for (const MapScene::Field& field : feature.fields) {
        vista::BatchField bf;
        bf.name = field.name;
        if (field.name == "name" || field.name == "anno" ||
            field.name == "text") {
          bf.value = gis::datasource::ogr_bytes_to_utf8(field.value);
        } else {
          bf.value = field.value;
        }
        feat.fields.push_back(std::move(bf));
      }
      out.features.push_back(std::move(feat));
    }
    pod.push_back(std::move(out));
  }
  return vista::build_layer_batches(pod, use_carto_slots, scale);
}

}  // namespace detail
}  // namespace content
