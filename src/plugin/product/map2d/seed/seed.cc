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
#include "plugin/product/world3d/scene/orthogrid/solve/boundary_solve.h"
#include "plugin/runtime/host/processing/operation_result.h"

#include <rapidjson/document.h>

namespace plugin {
namespace {

bool json_get_string(std::string_view json, const char* key, std::string* out) {
  if (!out || !key || json.empty()) {
    return false;
  }
  rapidjson::Document doc;
  doc.Parse(json.data(), static_cast<rapidjson::SizeType>(json.size()));
  if (doc.HasParseError() || !doc.IsObject()) {
    return false;
  }
  const auto it = doc.FindMember(key);
  if (it == doc.MemberEnd() || !it->value.IsString()) {
    return false;
  }
  *out = std::string(it->value.GetString(), it->value.GetStringLength());
  return !out->empty();
}

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

bool seed_orthogrid(content::PluginHost* host) {
  (void)host;
  char path[MAX_PATH * 3] = {};
  const wchar_t* rels[] = {L"..\\data\\plugin\\orthogrid_sample.gridbnd",
                           L"data\\plugin\\orthogrid_sample.gridbnd"};
  if (!first_existing_rel(rels, std::size(rels), path, sizeof(path))) {
    set_operation_result("{\"error\":\"orthogrid_sample_missing\"}");
    return false;
  }
  const detail::BoundarySolve solved =
      detail::solve_grid_boundary_file(path, /*elliptic_iters=*/4);
  if (!solved.ok || solved.nx < 3 || solved.ny < 3 ||
      solved.xs.size() != static_cast<size_t>(solved.nx * solved.ny)) {
    set_operation_result(solved.message.empty()
                             ? "{\"error\":\"orthogrid_solve_failed\"}"
                             : solved.message);
    return false;
  }
  OrthogridMeshCommit commit;
  commit.nx = solved.nx;
  commit.ny = solved.ny;
  commit.xs = solved.xs.data();
  commit.ys = solved.ys.data();
  if (!solved.cell_orth.empty()) {
    commit.cell_orth = solved.cell_orth.data();
  }
  if (!solved.raster_orth.empty() && solved.raster_w > 0 &&
      solved.raster_h > 0) {
    commit.raster_w = solved.raster_w;
    commit.raster_h = solved.raster_h;
    commit.raster_min_x = solved.raster_min_x;
    commit.raster_min_y = solved.raster_min_y;
    commit.raster_max_x = solved.raster_max_x;
    commit.raster_max_y = solved.raster_max_y;
    commit.raster_orth = solved.raster_orth.data();
  }
  if (!publish_orthogrid_mesh(commit)) {
    set_operation_result("{\"error\":\"orthogrid_publish_failed\"}");
    return false;
  }
  return true;
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
  if (!host) {
    return false;
  }
  switch (mode) {
    case Map2dSeedMode::kChina:
      // present_dataset / MapScene::open_path stay on the UI thread (harness).
      // Processing runs on the utility pool and must not touch Browser HWND.
      if (!china_loaded(host)) {
        set_operation_result("{\"error\":\"china_not_loaded\"}");
        return false;
      }
      set_operation_result("{\"ok\":true,\"op\":\"map2d.seed\",\"mode\":\"china\"}");
      return true;
    case Map2dSeedMode::kAlign:
      if (!china_loaded(host) || !apply_align_style(host)) {
        return false;
      }
      set_operation_result("{\"ok\":true,\"op\":\"map2d.seed\",\"mode\":\"align\"}");
      return true;
    case Map2dSeedMode::kOrthogrid:
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
  if (!json_get_string(args_json, "mode", &mode) && !args_json.empty()) {
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
