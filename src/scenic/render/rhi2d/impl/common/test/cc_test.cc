// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <atomic>
#include <cstdio>

#include "scenic/render/rhi2d/impl/common/cc/layer_tree_impl.h"
#include "scenic/render/rhi2d/impl/common/cc/raster_tile.h"
#include "scenic/render/rhi2d/impl/common/cc/scheduler.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_layer_tree_activate_retire() {
  scenic::detail::Rhi2dLayerTreeImpl tree;
  scenic::detail::RenderContext rc{};
  expect(!tree.is_active(), "tree starts inactive");
  expect(tree.activate(7, rc), "activate ok");
  expect(tree.is_active() && tree.is_current(7), "active gen 7");
  expect(!tree.is_current(8), "wrong gen not current");
  tree.retire();
  expect(!tree.is_active(), "retired inactive");
  expect(!tree.is_current(7), "retired not current");
}

void test_resolved_damage_full_and_partial() {
  scenic::detail::Rhi2dLayerTreeImpl tree;
  tree.set_full_damage();
  RECT full = tree.resolved_damage(100, 50);
  expect(full.left == 0 && full.top == 0 && full.right == 100 &&
             full.bottom == 50,
         "full damage fills viewport");

  RECT partial{10, 20, 40, 45};
  tree.set_damage(partial, /*full_damage=*/false);
  RECT got = tree.resolved_damage(100, 50);
  expect(got.left == 10 && got.top == 20 && got.right == 40 &&
             got.bottom == 45,
         "partial damage preserved");

  RECT clipped{80, 40, 200, 90};
  tree.set_damage(clipped, false);
  RECT c = tree.resolved_damage(100, 50);
  expect(c.left == 80 && c.top == 40 && c.right == 100 && c.bottom == 50,
         "damage clipped to viewport");
}

void test_scheduler_submit_and_abort() {
  scenic::detail::Rhi2dScheduler sched;
  std::atomic<int> paints{0};
  sched.set_paint_fn([&]() { ++paints; });
  sched.start();

  scenic::detail::RenderContext rc{};
  sched.set_context(rc);
  sched.submit();
  expect(sched.wait_idle(5000), "scheduler idle");
  expect(paints.load() == 1, "one paint");

  const uint64_t gen = sched.job_generation();
  sched.mark_published(gen);
  expect(sched.published_generation() == gen, "published gen");
  expect(!sched.should_abort(gen), "current gen not aborted");
  sched.cancel();
  expect(sched.should_abort(gen), "cancel aborts gen");

  (void)sched.shutdown();
}

void test_tile_enumerate_smoke() {
  auto tiles = scenic::detail::enumerate_viewport_tiles(
      512, 512, RECT{0, 0, 512, 512}, true, 9);
  expect(tiles.size() >= 4, "512 grid yields multiple tiles");
  expect(scenic::detail::rhi2d_tile_raster_worker_count(tiles.size()) >= 1,
         "worker count");
}

}  // namespace

int main() {
  test_layer_tree_activate_retire();
  test_resolved_damage_full_and_partial();
  test_scheduler_submit_and_abort();
  test_tile_enumerate_smoke();
  if (g_fails != 0) {
    std::fprintf(stderr, "gdi_cc_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("gdi_cc_test: ok\n");
  return 0;
}
