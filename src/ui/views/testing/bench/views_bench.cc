// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark for views test harness.

#include <cstdint>
#include <cstdio>
#include <memory>

#include <benchmark/benchmark.h>

#include "ui/views/kernel/compositor/shell_compositor.h"
#include "ui/views/kernel/paint/paint_commit.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/shell/theme_service.h"
#include "ui/views/primitives/button/button.h"
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
