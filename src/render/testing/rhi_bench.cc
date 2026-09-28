// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark for Null RHI scenarios.

#include "render/testing/scenarios.h"

#include <memory>

#include <benchmark/benchmark.h>

namespace {

void BM_null_device_lifecycle(benchmark::State& state) {
  for (auto _ : state) {
    const auto r =
        render::detail::run_device_lifecycle(render::rhi::Backend::kNull);
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
}
BENCHMARK(BM_null_device_lifecycle);

void BM_null_resources(benchmark::State& state) {
  std::unique_ptr<render::rhi::Device> device(
      render::detail::create_initialized_device(render::rhi::Backend::kNull,
                                                render::rhi::DeviceDesc()));
  if (!device) {
    state.SkipWithError("null device init failed");
    return;
  }
  for (auto _ : state) {
    const auto r = render::detail::run_resources(device.get());
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
  device->shutdown();
}
BENCHMARK(BM_null_resources);

void BM_null_record_present(benchmark::State& state) {
  std::unique_ptr<render::rhi::Device> device(
      render::detail::create_initialized_device(render::rhi::Backend::kNull,
                                                render::rhi::DeviceDesc()));
  if (!device) {
    state.SkipWithError("null device init failed");
    return;
  }
  for (auto _ : state) {
    const auto r = render::detail::run_record_present(device.get());
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
  device->shutdown();
}
BENCHMARK(BM_null_record_present);

void BM_null_graph_present(benchmark::State& state) {
  std::unique_ptr<render::rhi::Device> device(
      render::detail::create_initialized_device(render::rhi::Backend::kNull,
                                                render::rhi::DeviceDesc()));
  if (!device) {
    state.SkipWithError("null device init failed");
    return;
  }
  for (auto _ : state) {
    const auto r = render::detail::run_graph_present(device.get());
    if (!r.ok) {
      state.SkipWithError(r.message.c_str());
      return;
    }
  }
  device->shutdown();
}
BENCHMARK(BM_null_graph_present);

}  // namespace
