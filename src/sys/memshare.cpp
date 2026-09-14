// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sys/memshare.h"

namespace sys {

MemShare::MemShare(const char* map_name, int file_size, bool is_server)
    : file_map_(nullptr), data_(nullptr) {
  if (is_server) {
    file_map_ = CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE,
                                   0, static_cast<DWORD>(file_size), map_name);
  } else {
    file_map_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, map_name);
  }
  if (file_map_) {
    data_ = MapViewOfFile(file_map_, FILE_MAP_ALL_ACCESS, 0, 0, 0);
  }
}

MemShare::~MemShare() {
  if (data_) {
    UnmapViewOfFile(data_);
    data_ = nullptr;
  }
  if (file_map_) {
    CloseHandle(file_map_);
    file_map_ = nullptr;
  }
}

}  // namespace sys
