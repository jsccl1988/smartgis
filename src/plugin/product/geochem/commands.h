// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_GEOCHEM_COMMANDS_H_
#define PLUGIN_GEOCHEM_COMMANDS_H_

#include <functional>
#include <string>

#include "gis/analysis/geochem/grade.h"
#include "gis/analysis/geochem/idw.h"
#include "gis/analysis/geochem/samples.h"
#include "gis/analysis/geochem/stats.h"

namespace content {
class PluginHost;
}

namespace plugin {

// Payload for map2d graded samples + full-extent IDW heat raster.
struct GeochemCommit {
  gis::detail::GeochemSampleSet samples;
  gis::detail::GeochemGradeLegend legend;
  gis::detail::GeochemElementStats stats;
  gis::detail::GeochemCorrelation correlation;
  gis::detail::GeochemIdwResult idw;
  std::string element;
  bool has_idw = false;
  bool has_stats = false;
};

using GeochemWriter =
    std::function<bool(const GeochemCommit& commit, std::string* err)>;

// Optional: read Point features from the active map session layer.
using GeochemLayerReader = std::function<bool(
    const std::string& element, gis::detail::GeochemSampleSet* out,
    std::string* err)>;

void set_geochem_writer(GeochemWriter writer);
void set_geochem_layer_reader(GeochemLayerReader reader);

bool register_geochem(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_GEOCHEM_COMMANDS_H_
