// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_LEGACY_AM_H_
#define PLUGIN_LEGACY_AM_H_

#include <string>
#include <string_view>

namespace plugin {

class Registry;

const char* legacy_id_from_stem(std::string_view stem);
const char* legacy_id_from_display_name(std::string_view name);
std::string legacy_id_from_stem_string(std::string_view stem);
bool scan_legacy_am(Registry* registry, const char* aux_module_dir);
bool legacy_am_start(std::string_view id);
void legacy_am_stop(std::string_view id);
void legacy_am_unload(std::string_view id);

}  // namespace plugin

#endif  // PLUGIN_LEGACY_AM_H_
