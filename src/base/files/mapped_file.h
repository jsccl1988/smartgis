// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_FILES_MAPPED_FILE_H_
#define BASE_FILES_MAPPED_FILE_H_

#include <cstddef>
#include <string_view>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "base/core/log.h"

namespace base {

enum class MappedFileAdvice {
  kNormal = 0,
  kRandom = 1,
  kSequential = 2,  // FILE_FLAG_SEQUENTIAL_SCAN + PrefetchVirtualMemory
};

// Random-access read-only memory map (mogu MappedFile, Win32 CreateFileMapping).
// Not a sequential FileMMap / FileLoader scan.
class MappedFile {
 public:
  MappedFile() = default;
  ~MappedFile() { close(); }

  MappedFile(const MappedFile&) = delete;
  MappedFile& operator=(const MappedFile&) = delete;

  MappedFile(MappedFile&& other) noexcept { move_from(other); }

  MappedFile& operator=(MappedFile&& other) noexcept {
    if (this != &other) {
      close();
      move_from(other);
    }
    return *this;
  }

  // Empty file: is_open true, data()==nullptr, size()==0.
  bool open(const char* path,
            MappedFileAdvice advice = MappedFileAdvice::kNormal) {
    close();
    if (path == nullptr || !path[0]) {
      return false;
    }

    DWORD flags = FILE_ATTRIBUTE_NORMAL;
    if (advice == MappedFileAdvice::kSequential) {
      flags = FILE_FLAG_SEQUENTIAL_SCAN;
    } else if (advice == MappedFileAdvice::kRandom) {
      flags = FILE_FLAG_RANDOM_ACCESS;
    }

    file_ = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, flags, nullptr);
    if (file_ == INVALID_HANDLE_VALUE) {
      LOGGING(LOG_ERROR, "MappedFile open failed: %s", path);
      return false;
    }

    LARGE_INTEGER li = {};
    if (!GetFileSizeEx(file_, &li) || li.QuadPart < 0) {
      LOGGING(LOG_ERROR, "MappedFile GetFileSizeEx failed: %s", path);
      close();
      return false;
    }

    size_ = static_cast<size_t>(li.QuadPart);
    if (size_ == 0) {
      return true;
    }

    mapping_ = CreateFileMappingA(file_, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping_) {
      LOGGING(LOG_ERROR, "MappedFile CreateFileMapping failed: %s", path);
      close();
      return false;
    }

    addr_ = MapViewOfFile(mapping_, FILE_MAP_READ, 0, 0, 0);
    if (!addr_) {
      LOGGING(LOG_ERROR, "MappedFile MapViewOfFile failed: %s", path);
      close();
      return false;
    }

    if (advice == MappedFileAdvice::kSequential) {
      // Best-effort page-in (Win8+); ignore failure on older hosts.
      WIN32_MEMORY_RANGE_ENTRY range;
      range.VirtualAddress = addr_;
      range.NumberOfBytes = size_;
      (void)PrefetchVirtualMemory(GetCurrentProcess(), 1, &range, 0);
    }

    return true;
  }

  void close() {
    if (addr_) {
      UnmapViewOfFile(addr_);
      addr_ = nullptr;
    }
    if (mapping_) {
      CloseHandle(mapping_);
      mapping_ = nullptr;
    }
    if (file_ != INVALID_HANDLE_VALUE) {
      CloseHandle(file_);
      file_ = INVALID_HANDLE_VALUE;
    }
    size_ = 0;
  }

  bool is_open() const { return file_ != INVALID_HANDLE_VALUE; }

  const void* data() const { return addr_; }

  size_t size() const { return size_; }

  // Out-of-range -> false; do not write a valid view into *out.
  bool slice(size_t offset, size_t length, std::string_view* out) const {
    if (out == nullptr || size_ == 0 || addr_ == nullptr) {
      return false;
    }
    if (offset > size_ || length > size_ - offset) {
      return false;
    }
    *out = std::string_view(static_cast<const char*>(addr_) + offset, length);
    return true;
  }

 private:
  void move_from(MappedFile& other) noexcept {
    file_ = other.file_;
    mapping_ = other.mapping_;
    addr_ = other.addr_;
    size_ = other.size_;
    other.file_ = INVALID_HANDLE_VALUE;
    other.mapping_ = nullptr;
    other.addr_ = nullptr;
    other.size_ = 0;
  }

  HANDLE file_ = INVALID_HANDLE_VALUE;
  HANDLE mapping_ = nullptr;
  void* addr_ = nullptr;
  size_t size_ = 0;
};

}  // namespace base

#endif  // BASE_FILES_MAPPED_FILE_H_
