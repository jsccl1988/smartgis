// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_FILES_FILE_MMAP_H_
#define BASE_FILES_FILE_MMAP_H_

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/concurrency/queue.h"
#include "base/core/log.h"
#include "base/synchronization/latch.h"

namespace base {

// Sequential mmap block producer with optional background warmup
// (mogu FileMMap, Win32 CreateFileMapping). Binary-friendly: next() does not
// scan for newlines (dem bake / binary caches).
class FileMMap {
 public:
  // Values match execution::detail::Status for FileLoader static_cast.
  enum class Status {
    FAILED = -1,
    SUCCESS = 0,
    COMPLETE = 1,
  };

  explicit FileMMap(size_t block_size = 512 * 1024)
      : block_size_(block_size == 0 ? 512 * 1024 : block_size) {}

  ~FileMMap() { close(); }

  FileMMap(const FileMMap&) = delete;
  FileMMap& operator=(const FileMMap&) = delete;

  bool open(const char* file, bool warmup = true) {
    close();
    if (!file || !file[0]) {
      return false;
    }

    file_ = CreateFileA(file, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
      LOGGING(LOG_ERROR, "FileMMap open failed: %s", file);
      return false;
    }

    LARGE_INTEGER li = {};
    if (!GetFileSizeEx(file_, &li) || li.QuadPart < 0) {
      LOGGING(LOG_ERROR, "FileMMap GetFileSizeEx failed: %s", file);
      close();
      return false;
    }
    file_size_ = static_cast<size_t>(li.QuadPart);

    if (file_size_ > 0) {
      mapping_ =
          CreateFileMappingA(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
      if (!mapping_) {
        LOGGING(LOG_ERROR, "FileMMap CreateFileMapping failed: %s", file);
        close();
        return false;
      }
      mem_start_ = MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
      if (!mem_start_) {
        LOGGING(LOG_ERROR, "FileMMap MapViewOfFile failed: %s", file);
        close();
        return false;
      }
      WIN32_MEMORY_RANGE_ENTRY range;
      range.VirtualAddress = mem_start_;
      range.NumberOfBytes = file_size_;
      (void)PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
    }

    warmup_ = warmup;
    file_offset_ = 0;
    if (warmup_) {
      warmup_latch_ = std::make_unique<latch>(1);
      warmup_thread_ = std::make_unique<std::thread>([this] {
        warm_up();
        warmup_latch_->count_down();
      });
    }

    LOGGING(LOG_DEBUG, "FileMMap open [%s] size[%llu]", file,
            static_cast<unsigned long long>(file_size_));
    return true;
  }

  bool close() {
    if (warmup_ && warmup_thread_ && warmup_thread_->joinable()) {
      warmup_thread_->join();
    }
    warmup_thread_.reset();
    warmup_latch_.reset();

    Block discard;
    while (block_list_.pop(discard)) {
    }

    if (mem_start_) {
      UnmapViewOfFile(mem_start_);
      mem_start_ = nullptr;
    }
    if (mapping_) {
      CloseHandle(mapping_);
      mapping_ = nullptr;
    }
    if (file_ != INVALID_HANDLE_VALUE) {
      CloseHandle(file_);
      file_ = INVALID_HANDLE_VALUE;
    }
    file_size_ = 0;
    file_offset_ = 0;
    return true;
  }

  size_t file_size() const { return file_size_; }

  // Base of the mapped view (nullptr when empty / not open).
  const void* data() const { return mem_start_; }

  Status next(void** memory, int* size) {
    if (!warmup_) {
      return yield(memory, size);
    }
    if (!warmup_latch_) {
      return Status::FAILED;
    }
    warmup_latch_->wait();
    Block block;
    if (block_list_.pop(block)) {
      *memory = block.memory;
      *size = block.size;
      return Status::SUCCESS;
    }
    return Status::COMPLETE;
  }

 private:
  struct Block {
    void* memory = nullptr;
    int size = 0;
  };

  Status yield(void** memory, int* size) {
    if (!memory || !size) {
      return Status::FAILED;
    }
    if (file_offset_ >= file_size_ || !mem_start_) {
      return Status::COMPLETE;
    }
    *memory = static_cast<char*>(mem_start_) + file_offset_;
    const size_t remain = file_size_ - file_offset_;
    const size_t n = (std::min)(block_size_, remain);
    *size = static_cast<int>(n);
    file_offset_ += n;

    // Touch pages so cold disk cost lands in warmup / first yield.
    volatile char sink = 0;
    const auto* p = static_cast<const char*>(*memory);
    for (int i = 0; *size - 4096 * i > 0; ++i) {
      sink = p[4096 * i];
    }
    (void)sink;
    return Status::SUCCESS;
  }

  bool warm_up() {
    for (;;) {
      Block block;
      const Status ryield = yield(&block.memory, &block.size);
      if (ryield == Status::COMPLETE) {
        break;
      }
      if (ryield == Status::FAILED || !block_list_.push(block)) {
        LOGGING(LOG_ERROR, "FileMMap warm_up push failed");
        return false;
      }
    }
    return true;
  }

  BlockingQueue<Block> block_list_;
  size_t file_size_ = 0;
  size_t file_offset_ = 0;
  void* mem_start_ = nullptr;
  HANDLE file_ = INVALID_HANDLE_VALUE;
  HANDLE mapping_ = nullptr;
  size_t block_size_ = 512 * 1024;
  bool warmup_ = true;
  std::unique_ptr<std::thread> warmup_thread_;
  std::unique_ptr<latch> warmup_latch_;
};

}  // namespace base

#endif  // BASE_FILES_FILE_MMAP_H_
