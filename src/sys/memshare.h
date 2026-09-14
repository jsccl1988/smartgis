// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SYS_MEMSHARE_H_
#define SYS_MEMSHARE_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/core/core.h"

#if defined(SYS_EXPORTS)
#define SYS_MEMSHARE_EXPORT __declspec(dllexport)
#else
#define SYS_MEMSHARE_EXPORT __declspec(dllimport)
#endif

namespace sys {

// Minimal Win32 named shared-memory mapping (moved from src/base/core/memshare).
class SYS_MEMSHARE_EXPORT MemShare {
 public:
  MemShare(const char* map_name, int file_size = 0, bool is_server = false);
  ~MemShare();

  const void* data() const { return data_; }
  void* data() { return data_; }

 private:
  HANDLE file_map_ = nullptr;
  void* data_ = nullptr;
};

}  // namespace sys

#endif  // SYS_MEMSHARE_H_
