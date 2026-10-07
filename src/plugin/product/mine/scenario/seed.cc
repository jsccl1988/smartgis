// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/mine/scenario/seed.h"

#include <string>

#include "content/public/plugin_host.h"
#include "plugin/runtime/host/capability/capability.h"
#include "plugin/runtime/host/capability/scenario_shell.h"
#include "plugin/runtime/host/capability/shell.h"

namespace plugin {
namespace detail {

bool resolve_mine_boreholes_csv(char* out_utf8, size_t out_cap) {
  const wchar_t* rels[] = {L"..\\data\\plugin\\mine_boreholes.csv",
                           L"data\\plugin\\mine_boreholes.csv"};
  return resolve_rel_under_exe(rels, 2, out_utf8, out_cap);
}

bool seed_mine_processing(HarnessShell& browser, const char* csv_utf8) {
  if (!browser.plugin_host()) {
    plugin_mark("plugins-fail");
    return false;
  }

  const std::string csv_esc = json_escape_path(csv_utf8);
  const std::string interp_args =
      std::string("{\"input\":\"") + csv_esc +
      "\",\"stratum_id\":\"clay\"}";
  if (!run_processing_flushed(browser.plugin_host(), "mine.interpolate_stratum",
                              interp_args)) {
    plugin_mark("mine-interp-fail");
    return false;
  }
  plugin_mark("mine-ok");

  const std::string prism_args =
      std::string("{\"input\":\"") + csv_esc +
      "\",\"top_stratum_id\":\"clay\",\"bottom_stratum_id\":\"sand\"}";
  if (!run_processing_flushed(browser.plugin_host(), "mine.prism_volume",
                              prism_args)) {
    plugin_mark("prism-fail");
    return false;
  }
  plugin_mark("prism-ok");
  return true;
}

}  // namespace detail
}  // namespace plugin
