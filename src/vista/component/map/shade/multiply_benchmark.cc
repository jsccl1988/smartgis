// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Scalar vs AVX2 vs the public dispatch for hillshade coverage.
// Timing excludes the memcpy that restores the source pixels.

#include "vista/component/map/shade/multiply.h"
#include "vista/component/map/shade/multiply_kern.h"

#include <benchmark/benchmark.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

constexpr float kOpacity = 0.65f;

std::vector<std::uint8_t> make_rgba(size_t pixels) {
  std::vector<std::uint8_t> rgba(pixels * 4u);
  for (size_t i = 0; i < pixels; ++i) {
    rgba[i * 4u + 0] = static_cast<std::uint8_t>(i * 13u);
    rgba[i * 4u + 1] = static_cast<std::uint8_t>(255u - (i * 7u));
    rgba[i * 4u + 2] = static_cast<std::uint8_t>(i * 19u);
    rgba[i * 4u + 3] =
        (i % 5u == 0u) ? static_cast<std::uint8_t>(100) : static_cast<std::uint8_t>(220);
  }
  return rgba;
}

bool same_bytes(const std::vector<std::uint8_t>& a,
                const std::vector<std::uint8_t>& b) {
  return a.size() == b.size() &&
         std::memcmp(a.data(), b.data(), a.size()) == 0;
}

// Public dispatch (AVX2 groups + scalar tail) must match the scalar kernel.
bool dispatch_matches_scalar(size_t pixels) {
  const std::vector<std::uint8_t> src = make_rgba(pixels);
  std::vector<std::uint8_t> scalar = src;
  std::vector<std::uint8_t> dispatched = src;
  vista::detail::apply_multiply_coverage_scalar(scalar.data(), pixels, kOpacity);
  vista::apply_multiply_coverage(dispatched, kOpacity);
  return same_bytes(scalar, dispatched);
}

void time_kernel(benchmark::State& state,
                 void (*fn)(std::uint8_t*, size_t, float)) {
  const size_t pixels = static_cast<size_t>(state.range(0));
  const std::vector<std::uint8_t> src = make_rgba(pixels);
  std::vector<std::uint8_t> dst(src.size());
  for (auto _ : state) {
    state.PauseTiming();
    std::memcpy(dst.data(), src.data(), src.size());
    state.ResumeTiming();
    fn(dst.data(), pixels, kOpacity);
    benchmark::DoNotOptimize(dst.data());
    benchmark::ClobberMemory();
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(src.size()));
}

void BM_Multiply_Scalar(benchmark::State& state) {
  time_kernel(state, &vista::detail::apply_multiply_coverage_scalar);
}

void BM_Multiply_Avx2(benchmark::State& state) {
  time_kernel(state, &vista::detail::apply_multiply_coverage_avx2);
}

void BM_Multiply_Dispatch(benchmark::State& state) {
  const size_t pixels = static_cast<size_t>(state.range(0));
  const std::vector<std::uint8_t> src = make_rgba(pixels);
  std::vector<std::uint8_t> dst(src.size());
  for (auto _ : state) {
    state.PauseTiming();
    std::memcpy(dst.data(), src.data(), src.size());
    state.ResumeTiming();
    vista::apply_multiply_coverage(dst, kOpacity);
    benchmark::DoNotOptimize(dst.data());
    benchmark::ClobberMemory();
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(src.size()));
}

void register_benches(bool avx2) {
  const int pixels[] = {64 * 64, 256 * 256, 512 * 512, 1024 * 1024};
  for (int n : pixels) {
    benchmark::RegisterBenchmark("BM_Multiply_Scalar", BM_Multiply_Scalar)
        ->Arg(n)
        ->Unit(benchmark::kMicrosecond);
    if (avx2) {
      benchmark::RegisterBenchmark("BM_Multiply_Avx2", BM_Multiply_Avx2)
          ->Arg(n)
          ->Unit(benchmark::kMicrosecond);
    }
    benchmark::RegisterBenchmark("BM_Multiply_Dispatch", BM_Multiply_Dispatch)
        ->Arg(n)
        ->Unit(benchmark::kMicrosecond);
  }
}

}  // namespace

int main(int argc, char** argv) {
  const bool avx2 = vista::detail::multiply_avx2_runtime();
  std::fprintf(stderr, "multiply_benchmark: avx2=%s\n", avx2 ? "on" : "off");
  if (!dispatch_matches_scalar(19) || !dispatch_matches_scalar(64 * 64)) {
    std::fprintf(stderr, "multiply_benchmark: dispatch != scalar\n");
    return 1;
  }
  register_benches(avx2);
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
