// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark for views test harness.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <benchmark/benchmark.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/primitives/button/button.h"
#include "ui/views/primitives/collection/table_view.h"
#include "ui/views/testing/harness/event_generator.h"
#include "ui/views/testing/harness/overlay_scene.h"
#include "ui/views/testing/harness/views_test_base.h"

namespace {

using namespace ui::views;

void BM_event_generator_click(benchmark::State& state) {
  TestWidgetRoot host;
  make_test_widget(&host, 400, 300);
  EventGenerator gen(&host.widget);
  auto* button = new Button("Go");
  button->set_bounds({10, 10, 80, 28});
  host.root->add_child(std::unique_ptr<View>(button));

  for (auto _ : state) {
    gen.click(20, 20);
  }
}
BENCHMARK(BM_event_generator_click);

void BM_overlay_commit(benchmark::State& state) {
  const int layers = static_cast<int>(state.range(0));
  OverlayScene scene(320, 240, layers);
  for (auto _ : state) {
    benchmark::DoNotOptimize(scene.measure_commit_ns(1));
  }
}
BENCHMARK(BM_overlay_commit)->Arg(1)->Arg(4)->Arg(16);

void BM_shell_compositor_smoke(benchmark::State& state) {
  ShellCompositor compositor;
  compositor.start();

  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 64, 48});
  const Rect dirty{0, 0, 64, 48};

  for (auto _ : state) {
    PaintCommit frame;
    commit_view_tree(root.get(), dirty, 64, 48, 12,
                     ui::gfx::color_rgb(20, 20, 20), &frame);
    const std::uint64_t gen = frame.generation;
    compositor.commit(std::move(frame));
    compositor.wait_published(gen);
  }

  compositor.shutdown();
}
BENCHMARK(BM_shell_compositor_smoke);

// Dirty Button rect → commit_view_tree → ShellCompositor publish (hover gate).
void BM_hover_commit(benchmark::State& state) {
  ShellCompositor compositor;
  compositor.start();

  auto root = std::make_unique<View>();
  root->set_bounds({0, 0, 200, 100});
  auto* button = new Button("Hover");
  button->set_bounds({10, 10, 80, 28});
  root->add_child(std::unique_ptr<View>(button));
  const Rect dirty{10, 10, 80, 28};

  for (auto _ : state) {
    LARGE_INTEGER t0 = {};
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t0);
    PaintCommit frame;
    commit_view_tree(root.get(), dirty, 200, 100, 12,
                     ui::gfx::color_rgb(20, 20, 20), &frame);
    const std::uint64_t gen = frame.generation;
    compositor.commit(std::move(frame));
    compositor.wait_published(gen);
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_hover_commit_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
  }

  compositor.shutdown();
}
BENCHMARK(BM_hover_commit);

// TableView scroll strip: 500x4, exposed viewport strip → commit+wait.
void BM_table_scroll_commit(benchmark::State& state) {
  ShellCompositor compositor;
  compositor.start();

  auto table = std::make_unique<TableView>();
  table->set_columns({"a", "b", "c", "d"});
  for (int i = 0; i < 500; ++i) {
    table->add_row({std::to_string(i), "x", "y", "z"});
  }
  const int row_h = table->row_height();
  const int header_h = table->header_height();
  const int full_h = header_h + 500 * row_h;
  const int view_h = header_h + 20 * row_h;
  constexpr int kWidth = 640;
  table->set_bounds({0, 0, kWidth, full_h});

  // Warm the front DIB once so steady-state scrolls stay subset-publish.
  {
    const Rect warm{0, header_h, kWidth, view_h};
    table->set_exposed_rect(warm);
    PaintCommit frame;
    commit_view_tree(table.get(), warm, kWidth, full_h, 12,
                     ui::gfx::color_rgb(20, 20, 20), &frame);
    const std::uint64_t gen = frame.generation;
    compositor.commit(std::move(frame));
    compositor.wait_published(gen);
  }

  int scroll_row = 0;
  int prev_scroll = -1;
  for (auto _ : state) {
    const int y0 = header_h + scroll_row * row_h;
    const Rect strip{0, y0, kWidth, view_h};
    // After warm-up, one-row scroll only records/rasters the newly exposed
    // leading edge; retained DIB already holds the overlapping viewport rows.
    Rect dirty = strip;
    if (prev_scroll >= 0 && scroll_row == prev_scroll + 1) {
      dirty = Rect{0, y0 + view_h - row_h, kWidth, row_h};
    }
    table->set_exposed_rect(dirty);
    prev_scroll = scroll_row;
    scroll_row = (scroll_row + 1) % 40;

    LARGE_INTEGER t0 = {};
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t0);
    PaintCommit frame;
    commit_view_tree(table.get(), dirty, kWidth, full_h, 12,
                     ui::gfx::color_rgb(20, 20, 20), &frame);
    const std::uint64_t gen = frame.generation;
    compositor.commit(std::move(frame));
    compositor.wait_published(gen);
    QueryPerformanceCounter(&t1);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_table_scroll_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
  }

  compositor.shutdown();
}
BENCHMARK(BM_table_scroll_commit);

// Mimic BrowserView shell crop: 1280x720 BGRA → 800x600 row memcpy.
void BM_overlay_crop_memcpy(benchmark::State& state) {
  constexpr int kShellW = 1280;
  constexpr int kShellH = 720;
  constexpr int kCropW = 800;
  constexpr int kCropH = 600;
  constexpr int kX0 = 40;
  constexpr int kY0 = 60;
  const uint32_t shell_stride = static_cast<uint32_t>(kShellW) * 4u;
  std::vector<uint8_t> shell(static_cast<size_t>(shell_stride) * kShellH, 0);
  std::vector<uint8_t> crop_dst(static_cast<size_t>(kCropW) * kCropH * 4u, 0);
  const std::uint64_t copy_bytes =
      static_cast<std::uint64_t>(kCropW) * static_cast<std::uint64_t>(kCropH) *
      4u;

  for (auto _ : state) {
    LARGE_INTEGER t0 = {};
    LARGE_INTEGER t1 = {};
    QueryPerformanceCounter(&t0);
    const uint8_t* src =
        shell.data() + static_cast<size_t>(kY0) * shell_stride +
        static_cast<size_t>(kX0) * 4u;
    for (int y = 0; y < kCropH; ++y) {
      std::memcpy(crop_dst.data() + static_cast<size_t>(y) * kCropW * 4u,
                  src + static_cast<size_t>(y) * shell_stride,
                  static_cast<size_t>(kCropW) * 4u);
    }
    QueryPerformanceCounter(&t1);
    ui::gfx::note_overlay_copy_bytes(copy_bytes);
    if (t1.QuadPart > t0.QuadPart) {
      ui::gfx::note_overlay_commit_qpc(
          static_cast<std::uint64_t>(t1.QuadPart - t0.QuadPart));
    }
    benchmark::DoNotOptimize(crop_dst.data());
  }
}
BENCHMARK(BM_overlay_crop_memcpy);

}  // namespace

int main(int argc, char** argv) {
  ThemeService::get().ensure_builtin_packs();
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
