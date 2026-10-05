// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_GEOCHEM_COMMANDS_H_
#define PLUGIN_GEOCHEM_COMMANDS_H_

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

bool register_geochem(content::PluginHost* host);

}  // namespace plugin

#endif  // PLUGIN_GEOCHEM_COMMANDS_H_
