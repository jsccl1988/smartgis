// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/seed/mine_seed.h"

#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/showcase/plugin/common/plugin_io.h"

namespace app {
namespace detail {

bool resolve_mine_boreholes_csv(char* out_utf8, size_t out_cap) {
  const wchar_t* rels[] = {L"..\\data\\plugin\\mine_boreholes.csv",
                           L"data\\plugin\\mine_boreholes.csv"};
  return resolve_rel_under_exe(rels, 2, out_utf8, out_cap);
}

bool seed_mine_processing(Browser& browser, const char* csv_utf8) {
  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    return false;
  }

  const std::string csv_esc = json_escape_path(csv_utf8);
  const std::string interp_args =
      std::string("{\"input\":\"") + csv_esc +
      "\",\"stratum_id\":\"clay\"}";
  if (!browser.plugins()->run_processing("mine.interpolate_stratum",
                                         interp_args)) {
    plugin_showcase_mark("mine-interp-fail");
    return false;
  }
  plugin_showcase_mark("mine-ok");

  const std::string prism_args =
      std::string("{\"input\":\"") + csv_esc +
      "\",\"top_stratum_id\":\"clay\",\"bottom_stratum_id\":\"sand\"}";
  if (!browser.plugins()->run_processing("mine.prism_volume", prism_args)) {
    plugin_showcase_mark("prism-fail");
    return false;
  }
  plugin_showcase_mark("prism-ok");
  return true;
}

}  // namespace detail
}  // namespace app
