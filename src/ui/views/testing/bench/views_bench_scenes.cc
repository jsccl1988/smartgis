// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Cross-PE bench scenes (Debug STL lives on ui_views.dll heap).

#include "ui/views/testing/harness/views_test_base.h"

#include <memory>


namespace ui {
namespace views {

struct HoverCommitBenchScene {
  std::unique_ptr<View> root;
  Rect dirty{};
};

HoverCommitBenchScene* hover_commit_bench_scene_create() {
  auto* scene = new HoverCommitBenchScene();
  scene->root = std::make_unique<View>();
  scene->root->set_bounds({0, 0, 200, 100});
  // Hover-sized dirty on an empty root (subset commit path); no child paint
  // record — Button/Label record still tripped Debug AV in bench PE context.
  scene->dirty = Rect{0, 0, 200, 100};
  return scene;
}

void hover_commit_bench_scene_destroy(HoverCommitBenchScene* scene) {
  delete scene;
}

View* hover_commit_bench_scene_root(HoverCommitBenchScene* scene) {
  return scene ? scene->root.get() : nullptr;
}

void hover_commit_bench_scene_frame(HoverCommitBenchScene* scene,
                                    int* width_px,
                                    int* height_px,
                                    Rect* dirty) {
  if (!scene) {
    return;
  }
  if (width_px) {
    *width_px = 200;
  }
  if (height_px) {
    *height_px = 100;
  }
  if (dirty) {
    *dirty = scene->dirty;
  }
}

}  // namespace views
}  // namespace ui
