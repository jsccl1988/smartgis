// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/seed/stormsurge_seed.h"

#include <windows.h>

#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/showcase/plugin/common/common.h"
#include "app/views/shell/util/exe_sidecar_path.h"

namespace app {
namespace detail {

bool resolve_stormsurge_sample(const wchar_t* leaf, char* out_utf8,
                               size_t out_cap) {
  if (!leaf || !out_utf8 || out_cap < 2) {
    return false;
  }
  wchar_t rel0[MAX_PATH] = {};
  wchar_t rel1[MAX_PATH] = {};
  if (wcscpy_s(rel0, L"..\\data\\plugin\\") != 0 || wcscat_s(rel0, leaf) != 0 ||
      wcscpy_s(rel1, L"data\\plugin\\") != 0 || wcscat_s(rel1, leaf) != 0) {
    return false;
  }
  const wchar_t* rels[] = {rel0, rel1};
  return resolve_rel_under_exe(rels, 2, out_utf8, out_cap);
}

bool resolve_stormsurge_inputs(char* dem_utf8, size_t dem_cap,
                               char* coast_utf8, size_t coast_cap) {
  if (!resolve_stormsurge_sample(L"stormsurge_dem_sample.tif", dem_utf8,
                                 dem_cap) ||
      !resolve_stormsurge_sample(L"stormsurge_coast_sample.geojson", coast_utf8,
                                 coast_cap)) {
    plugin_showcase_mark("stormsurge-sample-fail");
    return false;
  }
  plugin_showcase_mark("sample-ok");
  return true;
}

bool resolve_stormsurge_mask_output(char* out_utf8, size_t out_cap) {
  if (!out_utf8 || out_cap < 2) {
    return false;
  }
  wchar_t out_w[MAX_PATH] = {};
  if (!exe_sidecar_path(out_w, MAX_PATH,
                        L"..\\data\\plugin\\stormsurge_mask.tif")) {
    plugin_showcase_mark("stormsurge-out-fail");
    return false;
  }
  if (WideCharToMultiByte(CP_UTF8, 0, out_w, -1, out_utf8,
                          static_cast<int>(out_cap), nullptr, nullptr) <= 0) {
    plugin_showcase_mark("stormsurge-out-fail");
    return false;
  }
  return true;
}

bool seed_stormsurge_processing(Browser& browser, const char* dem_utf8,
                                const char* coast_utf8, const char* out_utf8) {
  if (!browser.plugins() || !browser.plugins()->ensure_builtins()) {
    plugin_showcase_mark("plugins-fail");
    return false;
  }

  // Schematic coast DEM drives analysis + Scene3D (via dem path override).
  const std::string dem_esc = json_escape_path(dem_utf8);
  const std::string coast_esc = json_escape_path(coast_utf8);
  const std::string out_esc = json_escape_path(out_utf8);
  const std::string coast_args =
      std::string("{\"coast\":\"") + coast_esc + "\"}";
  if (!browser.plugins()->run_processing("stormsurge.load_coast", coast_args)) {
    plugin_showcase_mark("stormsurge-coast-fail");
    return false;
  }
  // Mid inundation: tide into the basin/channel so free-surface TIN spans a
  // readable water body (not a single highlight AABB).
  const std::string run_args =
      std::string("{\"dem\":\"") + dem_esc + "\",\"coast\":\"" + coast_esc +
      "\",\"output\":\"" + out_esc +
      "\",\"seed_x\":114.30,\"seed_y\":30.55,\"tide_level\":58.0,\"frames\":8}";
  if (!browser.plugins()->run_processing("stormsurge.run", run_args)) {
    plugin_showcase_mark("stormsurge-run-fail");
    return false;
  }
  plugin_showcase_mark("stormsurge-ok");
  return true;
}

}  // namespace detail
}  // namespace app
