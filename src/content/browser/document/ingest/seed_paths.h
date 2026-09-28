// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_DOCUMENT_INGEST_SEED_PATHS_H_
#define CONTENT_BROWSER_DOCUMENT_INGEST_SEED_PATHS_H_

#include <string>
#include <vector>

namespace content {
namespace detail {

std::string path_stem(const std::string& path);
std::vector<std::string> style_seed_relative_paths();

}  // namespace detail

// Ordered relative paths for default China seed (exe-dir / testing/data).
// Declared in content (not detail)  - public helper used by MapScene callers.
std::vector<std::string> china_seed_relative_paths();

}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_INGEST_SEED_PATHS_H_
