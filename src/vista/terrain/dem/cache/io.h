// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CACHE_IO_H_
#define VISTA_TERRAIN_DEM_CACHE_IO_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "vista/terrain/dem/cache/io_pipeline.h"

namespace vista {
namespace detail {

// Source DEM file identity for process/disk cache invalidation.
struct FileStamp {
  uint64_t size = 0;
  int64_t mtime = 0;
  bool ok = false;
};

FileStamp file_stamp(const char* path);

// FNV-1a 64 — stable across processes for cache filenames.
uint64_t path_hash64(const char* path);

// Root under out/data/cache/dem_bake (or --dem-bake-cache=). Cached after
// first resolve. |warmup_dem_bake_cache| also ensures the directory exists.
std::string cache_root_dir();
bool ensure_dir(const std::string& dir);

// Resolve root + create dem_bake directory once (mogu file_load warmup).
// Safe to call from any cache try_get/put; subsequent calls are cheap.
void warmup_dem_bake_cache();

// Three load implementations (all produce an owning |out| buffer):
//
//   kBaseline      — CreateFile + serial ReadFile chunks (no mmap / prefetch /
//                    parallel copy / FileLoader warmup).
//   kMappedChunked — base::MappedFile (kSequential + PrefetchVirtualMemory)
//                    then copy_bytes_chunked (io_pipeline steal).
//   kFileLoader    — base::FileMMap warmup + dem/cache BinaryCopyHandler
//                    (execution::Pipeline produce/parse/consume).
enum class DemIoReadMode {
  kBaseline = 0,
  kMappedChunked = 1,
  kFileLoader = 2,
};

// Product default: MappedFile + io_pipeline chunk steal.
bool read_all(const std::string& path, std::vector<uint8_t>* out);

bool read_all_baseline(const std::string& path, std::vector<uint8_t>* out);

bool read_all_mapped_chunked(const std::string& path, std::vector<uint8_t>* out);

// |parallel_num| 0 → hardware_concurrency. |block_size| FileMMap yield size.
bool read_all_file_loader(const std::string& path, std::vector<uint8_t>* out,
                          size_t parallel_num = 0,
                          size_t block_size = 512 * 1024,
                          bool warmup = true);

bool read_all(const std::string& path, std::vector<uint8_t>* out,
              DemIoReadMode mode);

bool write_all(const std::string& path, const void* data, size_t bytes);

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CACHE_IO_H_
