// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/document/ingest/china_clip.h"

#include <cctype>
#include <string>

#include "content/browser/document/ingest/seed_paths.h"

namespace content {
namespace detail {

bool path_looks_like_china_city(const std::string& path) {
  std::string stem = path_stem(path);
  for (char& c : stem) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return stem.find("china_city") != std::string::npos;
}

}  // namespace detail
}  // namespace content
