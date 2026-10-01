// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/edit/feature_edit.h"
#include "content/browser/document/store/layer_store.h"

#include <cmath>
#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  content::detail::LayerStore store;
  expect(store.create_layer("snap_line"), "create layer");
  content::detail::MapLayer* layer = store.find_layer(store.active_layer_id());
  expect(layer != nullptr, "active layer");
  if (!layer) {
    return 1;
  }

  content::detail::MapFeature line;
  line.id = store.next_feature_id();
  line.kind = content::detail::GeomKind::kLine;
  line.points = {{0.0, 0.0}, {10.0, 0.0}};
  layer->features.push_back(line);

  content::detail::MapFeature pt;
  pt.id = store.next_feature_id();
  pt.kind = content::detail::GeomKind::kPoint;
  pt.points = {{10.0, 10.0}};
  layer->features.push_back(pt);

  const content::detail::SnapHit vhit =
      content::detail::snap_to_features(store, 10.05, 0.02, 0.5);
  expect(vhit.kind == content::detail::SnapHit::Kind::kVertex, "vertex kind");
  expect(std::fabs(vhit.x - 10.0) < 1e-9 && std::fabs(vhit.y) < 1e-9,
         "vertex coords");

  const content::detail::SnapHit ehit =
      content::detail::snap_to_features(store, 5.0, 0.2, 0.5);
  expect(ehit.kind == content::detail::SnapHit::Kind::kEdge, "edge kind");
  expect(std::fabs(ehit.x - 5.0) < 1e-6 && std::fabs(ehit.y) < 1e-6,
         "edge coords");

  double sx = 0;
  double sy = 0;
  expect(content::detail::snap_point(store, 10.1, 10.1, 0.5, &sx, &sy),
         "snap_point");
  expect(std::fabs(sx - 10.0) < 1e-9 && std::fabs(sy - 10.0) < 1e-9,
         "snap_point coords");

  const content::detail::SnapHit miss =
      content::detail::snap_to_features(store, 100.0, 100.0, 0.5);
  expect(miss.kind == content::detail::SnapHit::Kind::kNone, "miss outside tol");

  if (g_fails) {
    std::fprintf(stderr, "%d feature_edit_test fail(s)\n", g_fails);
    return 1;
  }
  std::fprintf(stderr, "feature_edit_test ok\n");
  return 0;
}
