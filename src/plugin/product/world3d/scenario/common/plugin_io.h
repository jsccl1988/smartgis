// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_PRODUCT_WORLD3D_SCENARIO_COMMON_PLUGIN_IO_H_
#define PLUGIN_PRODUCT_WORLD3D_SCENARIO_COMMON_PLUGIN_IO_H_

#include <cstddef>
#include <cstdint>
#include <string>

namespace plugin {

class HarnessShell;

namespace detail {

inline constexpr uint32_t kPluginShowcasePresentW = 640;
inline constexpr uint32_t kPluginShowcasePresentH = 480;

void bind_plugin_showcase_shell(HarnessShell* shell);
HarnessShell* plugin_showcase_shell();

void plugin_showcase_mark(const char* step);

bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
                           char* out_utf8, size_t out_cap);

std::string json_escape_path(const char* path);

}  // namespace detail
}  // namespace plugin

#endif  // PLUGIN_PRODUCT_WORLD3D_SCENARIO_COMMON_PLUGIN_IO_H_
