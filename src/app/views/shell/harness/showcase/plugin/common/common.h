// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_COMMON_H_
#define APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_COMMON_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace app {
namespace detail {

// Shared present size for Scene3D plugin showcase HWND captures.
inline constexpr uint32_t kPluginShowcasePresentW = 640;
inline constexpr uint32_t kPluginShowcasePresentH = 480;

void plugin_showcase_mark(const char* step);

// Resolve the first existing path under the exe directory (UTF-8 out).
bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
                           char* out_utf8, size_t out_cap);

// Escape backslash and quote for JSON processing args.
std::string json_escape_path(const char* path);

}  // namespace detail
}  // namespace app

#endif  // APP_VIEWS_SHELL_HARNESS_SHOWCASE_PLUGIN_COMMON_H_
