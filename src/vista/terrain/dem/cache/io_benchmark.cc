// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// google/benchmark: dem bake cache IO on real out/data/cache/dem_bake blobs —
// Baseline ReadFile vs MappedFile+io_pipeline vs FileLoader.

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

struct BakeFile {
  const char* kind = nullptr;  // raster / hypso / mesh / view
  std::string path;
  size_t size = 0;
};

// One representative per bake kind (largest file of that kind under dem_bake).
std::vector<BakeFile>& bake_files() {
  static std::vector<BakeFile> files;
  static bool inited = false;
  if (inited) {
    return files;
  }
  inited = true;

  vista::detail::warmup_dem_bake_cache();
  const std::string root = vista::detail::cache_root_dir();
  if (root.empty()) {
    return files;
  }

  const char* kinds[] = {"raster", "hypso", "mesh", "view"};
  BakeFile best[4];
  for (int i = 0; i < 4; ++i) {
    best[i].kind = kinds[i];
  }

  WIN32_FIND_DATAA fd = {};
  const std::string pattern = root + "\\*.bin";
  const HANDLE h = FindFirstFileA(pattern.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    return files;
  }
  do {
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      continue;
    }
    const char* name = fd.cFileName;
    int kind = -1;
    if (std::strstr(name, "_raster.bin")) {
      kind = 0;
    } else if (std::strstr(name, "_hypso_")) {
      kind = 1;
    } else if (std::strstr(name, "_mesh_")) {
      kind = 2;
    } else if (std::strstr(name, "_view_")) {
      kind = 3;
    }
    if (kind < 0) {
      continue;
    }
    ULARGE_INTEGER sz;
    sz.HighPart = fd.nFileSizeHigh;
    sz.LowPart = fd.nFileSizeLow;
    const size_t bytes = static_cast<size_t>(sz.QuadPart);
    if (bytes == 0 || bytes <= best[kind].size) {
      continue;
    }
    best[kind].path = root + "\\" + name;
    best[kind].size = bytes;
  } while (FindNextFileA(h, &fd));
  FindClose(h);

  for (int i = 0; i < 4; ++i) {
    if (!best[i].path.empty()) {
      files.push_back(best[i]);
    }
  }
  return files;
}

// Touch one byte per page so the read cannot be DCE'd without a full-buffer
// scan dominating the IO time.
uint64_t checksum(const std::vector<uint8_t>& v) {
  uint64_t h = 14695981039346656037ull;
  for (size_t i = 0; i < v.size(); i += 4096) {
    h ^= v[i];
    h *= 1099511628211ull;
  }
  if (!v.empty()) {
    h ^= v.back();
  }
  return h;
}

bool read_loader_fixed(const std::string& path, std::vector<uint8_t>* out) {
  return vista::detail::read_all_file_loader(path, out, kLoaderParallel,
                                             kLoaderBlock, /*warmup=*/true);
}

using ReadFn = bool (*)(const std::string&, std::vector<uint8_t>*);

void run_bake_bench(benchmark::State& state, size_t idx, ReadFn read_fn,
                    const char* mode) {
  const auto& files = bake_files();
  if (idx >= files.size()) {
    state.SkipWithError("no dem_bake file (fill out/data/cache/dem_bake first)");
    return;
  }
  const BakeFile& f = files[idx];
  {
    std::vector<uint8_t> warm;
    if (!read_fn(f.path, &warm) || warm.size() != f.size) {
      state.SkipWithError("warmup read failed");
      return;
    }
    benchmark::DoNotOptimize(checksum(warm));
  }

  for (auto _ : state) {
    std::vector<uint8_t> out;
    const bool ok = read_fn(f.path, &out);
    benchmark::DoNotOptimize(ok);
    benchmark::DoNotOptimize(checksum(out));
    if (!ok || out.size() != f.size) {
      state.SkipWithError("read failed");
      break;
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(f.size));
  char buf[160] = {};
  std::snprintf(buf, sizeof(buf), "%s %.2fMiB", f.kind,
                f.size / (1024.0 * 1024.0));
  state.SetLabel(buf);
  (void)mode;
}

std::string bench_dir() {
  char module[MAX_PATH] = {};
  const DWORD n = GetModuleFileNameA(nullptr, module, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return {};
  }
  std::string dir(module, module + n);
  const size_t slash = dir.find_last_of("\\/");
  dir.resize(slash);
  dir += "\\log\\dem_io_bench";
  CreateDirectoryA(dir.c_str(), nullptr);
  return dir;
}

std::string ensure_synth(size_t bytes) {
  const std::string dir = bench_dir();
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
  std::vector<uint8_t> buf(bytes);
  for (size_t i = 0; i < bytes; i += 4096) {
    buf[i] = static_cast<uint8_t>(i);
  }
  if (!vista::detail::write_all(path, buf.data(), buf.size())) {
    return {};
  }
  return path;
}

void run_path_bench(benchmark::State& state, const std::string& path,
                    size_t bytes, ReadFn read_fn, const char* label) {
  if (path.empty()) {
    state.SkipWithError("fixture");
    return;
  }
  {
    std::vector<uint8_t> warm;
    if (!read_fn(path, &warm) || warm.size() != bytes) {
      state.SkipWithError("warmup");
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
      state.SkipWithError("read");
      break;
    }
  }
  state.SetBytesProcessed(static_cast<int64_t>(state.iterations()) *
                          static_cast<int64_t>(bytes));
  state.SetLabel(label);
}

void register_synth_benches() {
  const size_t sizes[] = {32u << 20, 64u << 20};
  const char* tags[] = {"32MiB", "64MiB"};
  for (int s = 0; s < 2; ++s) {
    const size_t bytes = sizes[s];
    const std::string path = ensure_synth(bytes);
    char n0[80] = {};
    char n1[80] = {};
    char n2[80] = {};
    std::snprintf(n0, sizeof(n0), "BM_Synth_Baseline/%s", tags[s]);
    std::snprintf(n1, sizeof(n1), "BM_Synth_Mapped/%s", tags[s]);
    std::snprintf(n2, sizeof(n2), "BM_Synth_FileLoader/%s", tags[s]);
    benchmark::RegisterBenchmark(
        n0,
        [path, bytes](benchmark::State& st) {
          run_path_bench(st, path, bytes, &vista::detail::read_all_baseline,
                         "Baseline");
        })
        ->Unit(benchmark::kMillisecond);
    benchmark::RegisterBenchmark(
        n1,
        [path, bytes](benchmark::State& st) {
          run_path_bench(st, path, bytes,
                         &vista::detail::read_all_mapped_chunked, "Mapped");
        })
        ->Unit(benchmark::kMillisecond);
    benchmark::RegisterBenchmark(
        n2,
        [path, bytes](benchmark::State& st) {
          run_path_bench(st, path, bytes, &read_loader_fixed, "FileLoader");
        })
        ->Unit(benchmark::kMillisecond);
  }
}

void register_bake_benches() {
  const auto& files = bake_files();
  for (size_t i = 0; i < files.size(); ++i) {
    char base[96] = {};
    std::snprintf(base, sizeof(base), "%s_%.0fKiB", files[i].kind,
                  files[i].size / 1024.0);

    char n0[128] = {};
    char n1[128] = {};
    char n2[128] = {};
    std::snprintf(n0, sizeof(n0), "BM_Bake_Baseline/%s", base);
    std::snprintf(n1, sizeof(n1), "BM_Bake_Mapped/%s", base);
    std::snprintf(n2, sizeof(n2), "BM_Bake_FileLoader/%s", base);

    benchmark::RegisterBenchmark(
        n0,
        [i](benchmark::State& st) {
          run_bake_bench(st, i, &vista::detail::read_all_baseline, "Baseline");
        })
        ->Unit(benchmark::kMillisecond);
    benchmark::RegisterBenchmark(
        n1,
        [i](benchmark::State& st) {
          run_bake_bench(st, i, &vista::detail::read_all_mapped_chunked,
                         "Mapped");
        })
        ->Unit(benchmark::kMillisecond);
    benchmark::RegisterBenchmark(
        n2,
        [i](benchmark::State& st) {
          run_bake_bench(st, i, &read_loader_fixed, "FileLoader");
        })
        ->Unit(benchmark::kMillisecond);
  }
}

}  // namespace

int main(int argc, char** argv) {
  register_bake_benches();
  register_synth_benches();
  ::benchmark::Initialize(&argc, argv);
  if (::benchmark::ReportUnrecognizedArguments(argc, argv)) {
    return 1;
  }
  const auto& files = bake_files();
  std::fprintf(stderr, "dem_io_benchmark: cache_root=%s files=%zu\n",
               vista::detail::cache_root_dir().c_str(), files.size());
  for (const auto& f : files) {
    std::fprintf(stderr, "  %-6s %8.2f MiB  %s\n", f.kind,
                 f.size / (1024.0 * 1024.0), f.path.c_str());
  }
  if (files.empty()) {
    std::fprintf(stderr,
                 "dem_io_benchmark: no *.bin under dem_bake — skip run\n");
    return 2;
  }
  ::benchmark::RunSpecifiedBenchmarks();
  ::benchmark::Shutdown();
  return 0;
}
