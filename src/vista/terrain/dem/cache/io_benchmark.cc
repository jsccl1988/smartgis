// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark: dem bake cache IO — three-way:
//   Baseline (serial ReadFile)
//   MappedFile + io_pipeline chunk steal
//   FileMMap + FileLoader

#include "vista/terrain/dem/cache/io.h"

#include <benchmark/benchmark.h>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr size_t kLoaderParallel = 4;
constexpr size_t kLoaderBlock = 512 << 10;

std::string scratch_dir() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  std::string dir(module, module + n);
  const size_t slash = dir.find_last_of("\\/");
  if (slash == std::string::npos) {
    return {};
  }
  dir.resize(slash);
  dir += "\\log\\dem_io_bench";
  CreateDirectoryA((dir.substr(0, dir.find_last_of("\\/"))).c_str(), nullptr);
  CreateDirectoryA(dir.c_str(), nullptr);
  return dir;
}

bool write_pattern_file(const std::string& path, size_t bytes) {
  std::vector<uint8_t> buf(bytes);
  for (size_t i = 0; i < bytes; ++i) {
    buf[i] = static_cast<uint8_t>((i * 131u + 17u) & 0xffu);
  }
  return vista::detail::write_all(path, buf.data(), buf.size());
}

std::string ensure_fixture(size_t bytes) {
  const std::string dir = scratch_dir();
  if (dir.empty()) {
    return {};
  }
  char name[64] = {};
  std::snprintf(name, sizeof(name), "fixture_%llu.bin",
                static_cast<unsigned long long>(bytes));
  const std::string path = dir + "\\" + name;
  const auto stamp = vista::detail::file_stamp(path.c_str());
  if (stamp.ok && stamp.size == bytes) {
    return path;
  }
  if (!write_pattern_file(path, bytes)) {
    return {};
  }
  return path;
}

uint64_t checksum(const std::vector<uint8_t>& v) {
  uint64_t h = 14695981039346656037ull;
  for (uint8_t b : v) {
    h ^= b;
    h *= 1099511628211ull;
  }
  return h;
}

using ReadFn = bool (*)(const std::string&, std::vector<uint8_t>*);

bool read_loader_fixed(const std::string& path, std::vector<uint8_t>* out) {
  return vista::detail::read_all_file_loader(path, out, kLoaderParallel,
                                             kLoaderBlock, /*warmup=*/true);
}

void run_read_bench(benchmark::State& state, ReadFn read_fn, const char* label) {
  const size_t bytes = static_cast<size_t>(state.range(0));
  const std::string path = ensure_fixture(bytes);
  if (path.empty()) {
    state.SkipWithError("fixture create failed");
    return;
  }
  {
    std::vector<uint8_t> warm;
    if (!read_fn(path, &warm) || warm.size() != bytes) {
      state.SkipWithError("warmup read failed");
      return;
    }
    benchmark::DoNotOptimize(checksum(warm));
  }

  for (auto _ : state) {
    std::vector<uint8_t> out;
    const bool ok = read_fn(path, &out);
    benchmark::DoNotOptimize(ok);
    benchmark::DoNotOptimize(checksum(out));
    if (!ok || out.size() != bytes) {
      state.SkipWithError("read failed");
      break;
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(bytes));
  state.SetLabel(label);
}

void BM_DemIo_Baseline(benchmark::State& state) {
  run_read_bench(state, &vista::detail::read_all_baseline,
                 "Baseline ReadFile");
}

void BM_DemIo_MappedChunked(benchmark::State& state) {
  run_read_bench(state, &vista::detail::read_all_mapped_chunked,
                 "MappedFile+io_pipeline");
}

void BM_DemIo_FileLoader(benchmark::State& state) {
  run_read_bench(state, &read_loader_fixed, "FileLoader p=4 blk=512KiB");
}

// Head-to-head sizes: 1 / 8 / 32 / 64 MiB (warm page cache).
#define DEM_IO_SIZE_ARGS          \
  ->Arg(1 << 20)                  \
      ->Arg(8 << 20)              \
      ->Arg(32 << 20)             \
      ->Arg(64 << 20)             \
      ->Unit(benchmark::kMillisecond)

BENCHMARK(BM_DemIo_Baseline) DEM_IO_SIZE_ARGS;
BENCHMARK(BM_DemIo_MappedChunked) DEM_IO_SIZE_ARGS;
BENCHMARK(BM_DemIo_FileLoader) DEM_IO_SIZE_ARGS;

}  // namespace
