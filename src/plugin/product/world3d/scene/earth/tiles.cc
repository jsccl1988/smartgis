// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/world3d/scene/earth/tiles.h"

#include <windows.h>

#include <fstream>
#include <iterator>
#include <string>

#include "content/browser/present/scene3d/scene3d_presenter.h"

namespace plugin {
namespace {

bool resolve_rel_under_exe(const wchar_t* const* rels, size_t count,
                           char* out_utf8, size_t out_cap) {
  if (!rels || !out_utf8 || out_cap == 0 || count == 0) {
    return false;
  }
  wchar_t exe_dir[MAX_PATH] = {};
  if (GetModuleFileNameW(nullptr, exe_dir, MAX_PATH) == 0) {
    return false;
  }
  wchar_t* slash = wcsrchr(exe_dir, L'\\');
  if (slash) {
    slash[1] = L'\0';
  }
  for (size_t i = 0; i < count; ++i) {
    wchar_t path_w[MAX_PATH] = {};
    if (wcscpy_s(path_w, exe_dir) != 0 || wcscat_s(path_w, rels[i]) != 0) {
      continue;
    }
    if (GetFileAttributesW(path_w) == INVALID_FILE_ATTRIBUTES) {
      continue;
    }
    if (WideCharToMultiByte(CP_UTF8, 0, path_w, -1, out_utf8,
                            static_cast<int>(out_cap), nullptr, nullptr) <= 0) {
      continue;
    }
    return true;
  }
  return false;
}

}  // namespace

World3dCityTilesResult try_attach_world3d_city_tiles(
    content::Scene3dPresenter* cam) {
  if (!cam) {
    return World3dCityTilesResult::kSkipped;
  }
  if (cam->gpu().tileset_stream()) {
    cam->gpu().clear_tileset();
  }
  char tiles_path[MAX_PATH * 3] = {};
  const wchar_t* tile_rels[] = {L"..\\data\\m3_city_tileset.json",
                                L"data\\m3_city_tileset.json"};
  if (!resolve_rel_under_exe(tile_rels, 2, tiles_path, sizeof(tiles_path))) {
    return World3dCityTilesResult::kSkipped;
  }
  std::string root = tiles_path;
  const auto slash = root.find_last_of("/\\");
  if (slash != std::string::npos) {
    root.resize(slash + 1);
  }
  const std::string glb = root + "city_root.glb";
  std::ifstream glb_in(glb, std::ios::binary);
  if (!glb_in) {
    return World3dCityTilesResult::kSkipped;
  }
  std::ifstream tin(tiles_path, std::ios::binary);
  if (!tin) {
    return World3dCityTilesResult::kSkipped;
  }
  std::string json((std::istreambuf_iterator<char>(tin)),
                   std::istreambuf_iterator<char>());
  if (json.empty()) {
    return World3dCityTilesResult::kSkipped;
  }
  cam->gpu().set_tileset_content_root(root);
  if (cam->gpu().attach_tileset_json(json.c_str(), json.size(),
                                     "showcase_city")) {
    return World3dCityTilesResult::kAttached;
  }
  return World3dCityTilesResult::kAttachFailed;
}

}  // namespace plugin
