// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_PLUGIN_ADAPTER_AM_H_
#define LEGACY_PLUGIN_ADAPTER_AM_H_

#include <string>
#include <string_view>

namespace plugin {

// Physical home: src/legacy/plugin/adapter/ (GN
// //src/legacy/plugin/adapter:am). API stays in namespace plugin. Scans
// leftover *.am modules into Registry.

class Registry;

const char* am_id_from_stem(std::string_view stem);
const char* am_id_from_display_name(std::string_view name);
std::string am_id_from_stem_string(std::string_view stem);
bool scan_am(Registry* registry, const char* aux_module_dir);
bool am_start(std::string_view id);
void am_stop(std::string_view id);
void am_unload(std::string_view id);

}  // namespace plugin

#endif  // LEGACY_PLUGIN_ADAPTER_AM_H_
