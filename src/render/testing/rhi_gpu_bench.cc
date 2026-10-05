// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Optional DX12 / preferred-GPU google/benchmark. Skips cleanly without
// adapter. Not part of default //:benchmark_all.
//   set RUN_FLYCUBE_GPU=1
//   ninja -C out/Debug rhi_gpu_bench && out\Debug\rhi_gpu_bench.exe

#include "render/testing/scenarios.h"

#include <cstdio>
#include <cstdlib>
#include <memory>

#include <benchmark/benchmark.h>
#include "base/process/switches.h"

namespace {

bool env_wants_gpu() {
  const char* v = base::switch_cstr("run-flycube-gpu");
  return v && v[0] == '1' && v[1] == '\0';
}

render::rhi::Device* g_device = nullptr;

void BM_gpu_resources(benchmark::State& state) {
  if (!g_device) {
    state.SkipWithError("no device");
    return;
  }
  for (auto _ : state) {
    const auto r = render::detail::run_resources(g_device);
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
}
BENCHMARK(BM_gpu_resources);

void BM_gpu_record_present(benchmark::State& state) {
  if (!g_device) {
    state.SkipWithError("no device");
    return;
  }
  for (auto _ : state) {
    const auto r = render::detail::run_record_present(g_device);
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
}
BENCHMARK(BM_gpu_record_present);

void BM_gpu_compute_smoke(benchmark::State& state) {
  if (!g_device) {
    state.SkipWithError("no device");
    return;
  }
  for (auto _ : state) {
    const auto r = render::detail::run_compute_smoke(g_device);
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
}
BENCHMARK(BM_gpu_compute_smoke);

}  // namespace

int main(int argc, char** argv) {
  if (!env_wants_gpu()) {
    std::printf("rhi_gpu_bench: skip (set RUN_FLYCUBE_GPU=1)\n");
    return 0;
  }

  const render::rhi::Backend backend = render::rhi::preferred_gpu_backend();
  std::unique_ptr<render::rhi::Device> device(
      render::detail::create_initialized_device(backend,
                                                render::rhi::DeviceDesc()));
  if (!device) {
    std::printf("rhi_gpu_bench: skip (no adapter / init failed for %s)\n",
                render::rhi::backend_display_name(backend));
    return 0;
  }
  g_device = device.get();
  std::printf("rhi_gpu_bench: backend=%s\n",
              render::rhi::backend_display_name(device->backend()));

  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    g_device = nullptr;
    device->shutdown();
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();

  g_device = nullptr;
  device->shutdown();
  return 0;
}
