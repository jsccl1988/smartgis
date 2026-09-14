// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_FILES_READ_FILE_H_
#define BASE_FILES_READ_FILE_H_

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "base/core/log.h"
#include "base/util/path.h"

namespace base {

// Read an entire file into a string (binary-safe).
inline bool read_file_to_string(const path& file, std::string* out) {
  if (!out) {
    return false;
  }
  std::ifstream ifs(file, std::ios::binary);
  if (!ifs) {
    LOGGING(LOG_ERROR, "read_file_to_string failed: %s", file.string().c_str());
    return false;
  }
  out->assign(std::istreambuf_iterator<char>(ifs),
              std::istreambuf_iterator<char>());
  return true;
}

// List regular files matching extension under dir (non-recursive).
inline std::vector<path> list_files(const path& dir,
                                    const std::string& extension) {
  std::vector<path> out;
  std::error_code ec;
  if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) {
    return out;
  }
  for (const auto& entry : fs::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    if (!extension.empty() && entry.path().extension().string() != extension) {
      continue;
    }
    out.push_back(entry.path());
  }
  return out;
}

}  // namespace base

#endif  // BASE_FILES_READ_FILE_H_
