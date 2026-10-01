// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/plugin/analysis_writers.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/analysis_writer_flood.h"
#include "app/views/shell/browser/plugin/analysis_writer_geochem.h"
#include "app/views/shell/browser/plugin/analysis_writer_mine.h"
#include "app/views/shell/browser/plugin/analysis_writer_orthogrid.h"
#include "app/views/shell/browser/plugin/analysis_writer_stormsurge.h"
#include "app/views/shell/browser/plugin/analysis_writer_traffic.h"
#include "app/views/shell/browser/plugin/analysis_writer_world3d.h"

namespace app {

void wire_plugin_analysis_writers(Browser* browser) {
  if (!browser) {
    return;
  }
  detail::wire_world3d_analysis_writers(browser);
  detail::wire_orthogrid_analysis_writers(browser);
  detail::wire_traffic_analysis_writers(browser);
  detail::wire_flood_analysis_writers(browser);
  detail::wire_stormsurge_analysis_writers(browser);
  detail::wire_mine_analysis_writers(browser);
  detail::wire_geochem_analysis_writers(browser);
}

}  // namespace app
