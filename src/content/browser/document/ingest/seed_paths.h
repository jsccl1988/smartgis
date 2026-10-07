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
// Declared in content (not detail) — public helper used by GisScene callers.
// Same candidate policy as content::sample_gis_relative_paths(); the .cc
// forwards to that function and does not keep a second path table.
std::vector<std::string> china_seed_relative_paths();

// First existing regular file under |exe_dir| (trailing separator ok).
// Stat-only — does not open GDAL. Preferred order matches china_seed_relative_paths().
bool try_resolve_china_seed_path(const std::string& exe_dir,
                                 std::string* out_path);

}  // namespace content

#endif  // CONTENT_BROWSER_DOCUMENT_INGEST_SEED_PATHS_H_
