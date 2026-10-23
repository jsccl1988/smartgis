// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SCRIPT_LOAD_H_
#define CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SCRIPT_LOAD_H_

#include <string>

namespace content {
namespace detail {

// Locate <suite_id>.py for run_script. Not a second session entry.
// Order: |switch_name| file (if it exists), exe sidecar, harness layouts,
// recursive harness walk, then a path beside the exe. A missing switch file
// falls through. Suite layout: testing/tools/harness/<family>/<id>/<id>.py.
bool locate_suite(const char* suite_id,
                  const char* switch_name,
                  std::wstring* out);

bool wide_path_exists(const std::wstring& path);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_GLANG_RUNTIME_SESSION_SCRIPT_LOAD_H_
