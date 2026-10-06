// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_GEOCHEM_PRESENT_PRESENT_H_
#define PLUGIN_GEOCHEM_PRESENT_PRESENT_H_

#include <string>

#include "plugin/product/geochem/commands.h"

namespace content {
class GisDocument;
class MapScene;
}

namespace gis {
namespace detail {
struct GeochemSampleSet;
}
}

namespace plugin {

// Map2d commit for graded samples + IDW heat raster (UI-thread present).
bool present_geochem(content::GisDocument* doc,
                     const GeochemCommit& commit,
                     std::string* err);

// Horizon layer reader still sees MapScene (no GisDocument feature walk).
bool read_geochem_active_layer(content::MapScene* doc,
                               const std::string& element,
                               gis::detail::GeochemSampleSet* out,
                               std::string* err);

}  // namespace plugin

#endif  // PLUGIN_GEOCHEM_PRESENT_PRESENT_H_
