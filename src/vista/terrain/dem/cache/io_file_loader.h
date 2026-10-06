// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_TERRAIN_DEM_CACHE_IO_FILE_LOADER_H_
#define VISTA_TERRAIN_DEM_CACHE_IO_FILE_LOADER_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <thread>
#include <vector>

#include "base/execution/pipeline/pipeline.h"
#include "base/files/file_loader.h"

namespace vista {
namespace detail {

// Shared destination for parallel FileLoader block copies. Each pipeline
// Context holds a pointer to this (via acquire hooks); memcpy is offset by
// (block - map_base) so stage-1 reordering is safe.
struct BinaryCopyShared {
  uint8_t* dst = nullptr;
  const uint8_t* map_base = nullptr;
  size_t file_size = 0;
  std::atomic<size_t> bytes_copied{0};
};

// FileLoader BlockHandler: copy mmap blocks into an owning bake-cache buffer.
struct BinaryCopyHandler {
  using Status = base::execution::detail::Status;

  void* block = nullptr;
  int block_size = 0;
  BinaryCopyShared* shared = nullptr;

  Status on_block_complete() {
    if (!shared || !shared->dst || !shared->map_base || !block ||
        block_size <= 0) {
      return Status::FAILED;
    }
    const auto* src = static_cast<const uint8_t*>(block);
    if (src < shared->map_base) {
      return Status::FAILED;
    }
    const size_t off = static_cast<size_t>(src - shared->map_base);
    const size_t n = static_cast<size_t>(block_size);
    if (off > shared->file_size || n > shared->file_size - off) {
      return Status::FAILED;
    }
    std::memcpy(shared->dst + off, src, n);
    shared->bytes_copied.fetch_add(n, std::memory_order_relaxed);
    return Status::SUCCESS;
  }

  Status on_record_complete() { return Status::CONSUMED; }
};

// Load |path| into |out| via base::FileMMap (+ optional warmup) → FileLoader.
// |parallel_num| 0 → hardware_concurrency (min 1).
inline bool load_file_via_loader(const char* path, std::vector<uint8_t>* out,
                                 size_t parallel_num = 0,
                                 size_t block_size = 512 * 1024,
                                 bool warmup = true) {
  if (!path || !path[0] || !out) {
    return false;
  }
  if (parallel_num == 0) {
    const unsigned hw = std::thread::hardware_concurrency();
    parallel_num = hw == 0 ? 1 : static_cast<size_t>(hw);
  }

  base::FileLoader<BinaryCopyHandler> loader;
  if (!loader.create(parallel_num, path, block_size, warmup)) {
    return false;
  }
  if (!loader.file_map || !loader.pipeline) {
    return false;
  }

  const size_t n = loader.file_map->file_size();
  if (n == 0 || !loader.file_map->data()) {
    out->clear();
    return false;
  }
  out->assign(n, 0);

  auto shared = std::make_shared<BinaryCopyShared>();
  shared->dst = out->data();
  shared->map_base = static_cast<const uint8_t*>(loader.file_map->data());
  shared->file_size = n;

  loader.pipeline->set_context_hooks(
      [shared]() {
        auto* h = new BinaryCopyHandler();
        h->shared = shared.get();
        return h;
      },
      [](BinaryCopyHandler* h) { delete h; });

  if (!loader.run()) {
    loader.destroy();
    return false;
  }
  loader.destroy();
  return shared->bytes_copied.load(std::memory_order_relaxed) == n;
}

}  // namespace detail
}  // namespace vista

#endif  // VISTA_TERRAIN_DEM_CACHE_IO_FILE_LOADER_H_
