// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_TESTING_HARNESS_OVERLAY_SCENE_H_
#define UI_VIEWS_TESTING_HARNESS_OVERLAY_SCENE_H_

#include <cstdint>
#include <memory>

#include "ui/views/kernel/view/view.h"

namespace ui {
namespace views {

// Fixed-size view tree with N stacked paint layers for commit benchmarks.
class OverlayScene {
 public:
  OverlayScene(int width, int height, int layer_count = 1);

  void set_layer_count(int n);
  int layer_count() const { return layer_count_; }

  View* root() const { return root_.get(); }
  int width() const { return width_; }
  int height() const { return height_; }

  // Average nanoseconds per commit_view_tree over |iters| (QPC).
  std::uint64_t measure_commit_ns(int iters) const;

 private:
  void rebuild_layers();

  int width_ = 0;
  int height_ = 0;
  int layer_count_ = 0;
  std::unique_ptr<View> root_;
};

}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_TESTING_HARNESS_OVERLAY_SCENE_H_
