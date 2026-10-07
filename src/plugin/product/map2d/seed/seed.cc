// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/seed/seed.h"

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "plugin/product/world3d/commands.h"
#include "plugin/product/world3d/scene/orthogrid/session/session.h"
#include "plugin/product/world3d/scene/orthogrid/solve/boundary_solve.h"
#include "plugin/runtime/host/processing/args_json.h"
#include "plugin/runtime/host/processing/operation_result.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

bool exe_dir_slash(wchar_t* out, size_t cap) {
  if (!out || cap < 4) {
    return false;
  }
  if (!GetModuleFileNameW(nullptr, out, static_cast<DWORD>(cap))) {
    return false;
  }
  wchar_t* slash = wcsrchr(out, L'\\');
  if (!slash) {
    slash = wcsrchr(out, L'/');
  }
  if (!slash) {
    return false;
  }
  slash[1] = L'\0';
  return true;
}

bool first_existing_rel(const wchar_t* const* rels, size_t count, char* utf8,
                        size_t utf8_cap) {
  if (!rels || !utf8 || utf8_cap < 8) {
    return false;
  }
  wchar_t base[MAX_PATH] = {};
  if (!exe_dir_slash(base, MAX_PATH)) {
    return false;
  }
  for (size_t i = 0; i < count; ++i) {
    if (!rels[i]) {
      continue;
    }
    wchar_t full[MAX_PATH] = {};
    if (wcscpy_s(full, base) != 0 || wcscat_s(full, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(full) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, full, -1, utf8,
                            static_cast<int>(utf8_cap), nullptr, nullptr) <=
        0) {
      continue;
    }
    return true;
  }
  return false;
}

bool read_file_utf8(const char* path, std::string* out) {
  if (!path || !out) {
    return false;
  }
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return false;
  }
  out->assign(std::istreambuf_iterator<char>(in),
              std::istreambuf_iterator<char>());
  return !out->empty();
}

bool china_loaded(content::PluginHost* host) {
  content::GisDocument* gis = host ? host->gis_document() : nullptr;
  return gis && gis->feature_count() >= 3;
}

// UI-thread only (ProcessingPool present phase / direct seed). Opens
// out/data/china_city.gpkg when the document is still empty so IL can stay
// atomic (run_processing map2d.seed without a prior open_map).
bool ensure_china_sample(content::PluginHost* host) {
  if (china_loaded(host)) {
    return true;
  }
  if (!host) {
    set_operation_result("{\"error\":\"china_host_missing\"}");
    return false;
  }
  char path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\china_city.gpkg",
                           L"..\\data\\china_city.geojson",
                           L"..\\data\\china_plp.geojson",
                           L"data\\china_city.gpkg",
                           L"data\\china_city.geojson",
                           L"data\\china_plp.geojson"};
  if (!first_existing_rel(rels, std::size(rels), path, sizeof(path))) {
    set_operation_result("{\"error\":\"china_sample_missing\"}");
    return false;
  }
  // Public PluginHost seam (GisDocument has no open_path).
  if (!host->present_dataset("smartgis.map2d", path, 0) ||
      !china_loaded(host)) {
    set_operation_result("{\"error\":\"china_open_failed\"}");
    return false;
  }
  return true;
}

bool apply_align_style(content::PluginHost* host) {
  if (!host || !host->gis_document()) {
    return false;
  }
  char path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {
      L"maplibre\\example\\style_align.json",
      L"..\\maplibre\\example\\style_align.json",
      L"..\\..\\third_party\\maplibre\\example\\style_align.json",
      L"third_party\\maplibre\\example\\style_align.json",
  };
  if (!first_existing_rel(rels, std::size(rels), path, sizeof(path))) {
    set_operation_result("{\"error\":\"align_style_missing\"}");
    return false;
  }
  std::string json;
  if (!read_file_utf8(path, &json)) {
    set_operation_result("{\"error\":\"align_style_read\"}");
    return false;
  }
  if (!host->gis_document()->apply_style_json(json)) {
    set_operation_result("{\"error\":\"align_style_apply\"}");
    return false;
  }
  return true;
}

// ProcessingPool compute (host=nullptr) / present (real host) split — same
// shape as flood.inundate. Solve on the utility thread; publish on UI drain.
detail::BoundarySolve g_orthogrid_solved;

bool seed_orthogrid_compute() {
  char path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\plugin\\orthogrid_sample.gridbnd",
                           L"data\\plugin\\orthogrid_sample.gridbnd"};
  if (!first_existing_rel(rels, std::size(rels), path, sizeof(path))) {
    set_operation_result("{\"error\":\"orthogrid_sample_missing\"}");
    g_orthogrid_solved = {};
    return false;
  }
  g_orthogrid_solved =
      detail::solve_grid_boundary_file(path, /*elliptic_iters=*/4);
  if (!g_orthogrid_solved.ok || g_orthogrid_solved.nx < 3 ||
      g_orthogrid_solved.ny < 3 ||
      g_orthogrid_solved.xs.size() !=
          static_cast<size_t>(g_orthogrid_solved.nx * g_orthogrid_solved.ny)) {
    set_operation_result(g_orthogrid_solved.message.empty()
                             ? "{\"error\":\"orthogrid_solve_failed\"}"
                             : g_orthogrid_solved.message);
    g_orthogrid_solved = {};
    return false;
  }
  set_operation_result(
      "{\"ok\":true,\"op\":\"map2d.seed\",\"mode\":\"orthogrid\",\"phase\":"
      "\"compute\"}");
  return true;
}

bool seed_orthogrid_present(content::PluginHost* host) {
  if (!host) {
    set_operation_result("{\"error\":\"orthogrid_host_missing\"}");
    return false;
  }
  if (!g_orthogrid_solved.ok || g_orthogrid_solved.nx < 3 ||
      g_orthogrid_solved.ny < 3) {
    set_operation_result("{\"error\":\"orthogrid_compute_missing\"}");
    return false;
  }
  // map2d.seed may run before world3d orthogrid register; publish needs a
  // bound present host (visual_review #6 orthogrid-ok miss).
  bind_orthogrid_present_host(host);
  OrthogridMeshCommit commit;
  commit.nx = g_orthogrid_solved.nx;
  commit.ny = g_orthogrid_solved.ny;
  commit.xs = g_orthogrid_solved.xs.data();
  commit.ys = g_orthogrid_solved.ys.data();
  if (!g_orthogrid_solved.cell_orth.empty()) {
    commit.cell_orth = g_orthogrid_solved.cell_orth.data();
  }
  if (!g_orthogrid_solved.raster_orth.empty() &&
      g_orthogrid_solved.raster_w > 0 && g_orthogrid_solved.raster_h > 0) {
    commit.raster_w = g_orthogrid_solved.raster_w;
    commit.raster_h = g_orthogrid_solved.raster_h;
    commit.raster_min_x = g_orthogrid_solved.raster_min_x;
    commit.raster_min_y = g_orthogrid_solved.raster_min_y;
    commit.raster_max_x = g_orthogrid_solved.raster_max_x;
    commit.raster_max_y = g_orthogrid_solved.raster_max_y;
    commit.raster_orth = g_orthogrid_solved.raster_orth.data();
  }
  if (!publish_orthogrid_mesh(commit)) {
    set_operation_result("{\"error\":\"orthogrid_publish_failed\"}");
    return false;
  }
  return true;
}

bool seed_orthogrid(content::PluginHost* host) {
  if (!host) {
    return seed_orthogrid_compute();
  }
  // Direct (non-pool) callers: compute+present in one shot on the UI thread.
  if (!seed_orthogrid_compute()) {
    return false;
  }
  return seed_orthogrid_present(host);
}

}  // namespace

bool parse_map2d_seed_mode(std::string_view id, Map2dSeedMode* out) {
  if (!out || id.empty()) {
    return false;
  }
  if (id == "china") {
    *out = Map2dSeedMode::kChina;
    return true;
  }
  if (id == "align") {
    *out = Map2dSeedMode::kAlign;
    return true;
  }
  if (id == "orthogrid" || id == "baogrid") {
    *out = Map2dSeedMode::kOrthogrid;
    return true;
  }
  return false;
}

const char* map2d_seed_mode_name(Map2dSeedMode mode) {
  switch (mode) {
    case Map2dSeedMode::kChina:
      return "china";
    case Map2dSeedMode::kAlign:
      return "align";
    case Map2dSeedMode::kOrthogrid:
      return "orthogrid";
  }
  return "unknown";
}

bool seed_map2d(content::PluginHost* host, Map2dSeedMode mode) {
  // ProcessingPool: compute with host=nullptr, then present with the real
  // host on the UI drain thread (see attach_host_processing / flood.inundate).
  if (!host) {
    switch (mode) {
      case Map2dSeedMode::kChina:
      case Map2dSeedMode::kAlign:
        // Document open / style apply stay on the present (UI) phase.
        set_operation_result(
            "{\"ok\":true,\"op\":\"map2d.seed\",\"phase\":\"compute\"}");
        return true;
      case Map2dSeedMode::kOrthogrid:
        return seed_orthogrid_compute();
    }
    return false;
  }
  switch (mode) {
    case Map2dSeedMode::kChina:
      // present_dataset / MapScene::open_path stay on the UI thread (harness).
      if (!ensure_china_sample(host)) {
        return false;
      }
      set_operation_result("{\"ok\":true,\"op\":\"map2d.seed\",\"mode\":\"china\"}");
      return true;
    case Map2dSeedMode::kAlign:
      if (!ensure_china_sample(host) || !apply_align_style(host)) {
        return false;
      }
      set_operation_result("{\"ok\":true,\"op\":\"map2d.seed\",\"mode\":\"align\"}");
      return true;
    case Map2dSeedMode::kOrthogrid:
      // Pool present reuses g_orthogrid_solved when compute already ran;
      // direct C++ callers recompute inside seed_orthogrid(host).
      if (!seed_orthogrid(host)) {
        return false;
      }
      set_operation_result(
          "{\"ok\":true,\"op\":\"map2d.seed\",\"mode\":\"orthogrid\"}");
      return true;
  }
  return false;
}

bool seed_map2d_from_json(content::PluginHost* host, std::string_view args_json) {
  std::string mode;
  rapidjson::Document args;
  if (parse_args_json(args_json, &args)) {
    (void)args_json_string(args, "mode", &mode);
  }
  if (mode.empty() && !args_json.empty() && args_json.front() != '{') {
    mode.assign(args_json);
  }
  Map2dSeedMode parsed = Map2dSeedMode::kChina;
  if (!parse_map2d_seed_mode(mode, &parsed)) {
    set_operation_result("{\"error\":\"bad_mode\"}");
    return false;
  }
  return seed_map2d(host, parsed);
}

}  // namespace plugin
