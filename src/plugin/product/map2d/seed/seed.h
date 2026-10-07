// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_MAP2D_SEED_SEED_H_
#define PLUGIN_MAP2D_SEED_SEED_H_

#include <string_view>

namespace content {
class PluginHost;
}

namespace plugin {

// Document seed formerly inlined under app/views/harness/showcase/map2d/seed.
enum class Map2dSeedMode {
  kChina,
  kAlign,
  kOrthogrid,
};

bool parse_map2d_seed_mode(std::string_view id, Map2dSeedMode* out);
const char* map2d_seed_mode_name(Map2dSeedMode mode);

// ProcessingPool: compute (host=nullptr) then present (real host) on UI drain.
// China/align present opens china_city when empty; orthogrid solve→publish.
// Matches flood.inundate null-host compute / real-host present split.
bool seed_map2d(content::PluginHost* host, Map2dSeedMode mode);

// JSON `{"mode":"china"|"align"|"orthogrid"}` or a bare mode string.
bool seed_map2d_from_json(content::PluginHost* host, std::string_view args_json);

}  // namespace plugin

#endif  // PLUGIN_MAP2D_SEED_SEED_H_
