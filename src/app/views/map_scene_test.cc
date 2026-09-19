// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/map_scene.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "gis/world/land_mask.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

size_t find_rel(const std::vector<std::string>& rels, const char* needle) {
  for (size_t i = 0; i < rels.size(); ++i) {
    if (rels[i].find(needle) != std::string::npos) {
      return i;
    }
  }
  return static_cast<size_t>(-1);
}

// Two distant mainland-sized parts: keeping only the largest would leave a hole.
bool write_multipart_geojson(const char* path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  static const char kJson[] =
      "{\"type\":\"FeatureCollection\",\"name\":\"multipart_area\","
      "\"features\":[{"
      "\"type\":\"Feature\","
      "\"properties\":{\"name\":\"west-east\",\"adcode\":\"999001\","
      "\"kind\":\"area\"},"
      "\"geometry\":{\"type\":\"MultiPolygon\",\"coordinates\":["
      "[[[73.0,40.0],[75.0,40.0],[75.0,42.0],[73.0,42.0],[73.0,40.0]]],"
      "[[[85.0,40.0],[88.0,40.0],[88.0,43.0],[85.0,43.0],[85.0,40.0]]]"
      "]}}]}";
  const size_t n = std::fwrite(kJson, 1, sizeof(kJson) - 1, f);
  std::fclose(f);
  return n == sizeof(kJson) - 1;
}

bool write_line_with_siberia_stub(const char* path) {
  FILE* f = nullptr;
  if (fopen_s(&f, path, "wb") != 0 || !f) {
    return false;
  }
  // Layer name comes from file stem "line" (china_city convention).
  static const char kJson[] =
      "{\"type\":\"FeatureCollection\",\"name\":\"line\","
      "\"features\":[{"
      "\"type\":\"Feature\","
      "\"properties\":{\"name\":\"stub\",\"kind\":\"river\"},"
      "\"geometry\":{\"type\":\"LineString\",\"coordinates\":["
      "[100.0,40.0],[110.0,40.0],[110.0,58.0],[100.0,58.0]"
      "]}}]}";
  const size_t n = std::fwrite(kJson, 1, sizeof(kJson) - 1, f);
  std::fclose(f);
  return n == sizeof(kJson) - 1;
}

}  // namespace

int main() {
  const std::vector<std::string> rels = app::china_seed_relative_paths();
  expect(!rels.empty(), "seed relatives non-empty");

  const size_t city_gpkg = find_rel(rels, "china_city.gpkg");
  const size_t city_json = find_rel(rels, "china_city.geojson");
  const size_t plp = find_rel(rels, "china_plp.geojson");
  expect(city_gpkg != static_cast<size_t>(-1), "lists china_city.gpkg");
  expect(city_json != static_cast<size_t>(-1), "lists china_city.geojson");
  expect(plp != static_cast<size_t>(-1), "lists china_plp fallback");
  expect(city_gpkg < plp, "china_city.gpkg before china_plp");
  expect(city_json < plp, "china_city.geojson before china_plp");
  expect(city_gpkg < city_json, "gpkg before geojson twin");

  // Unified Baidu land wash — fills collapse across adcodes.
  const COLORREF a = app::map_scene_area_fill_color("110000", 1);
  const COLORREF b = app::map_scene_area_fill_color("320100", 2);
  const COLORREF c = app::map_scene_area_fill_color("510100", 3);
  expect(a == b && b == c, "area fill is unified land wash");
  expect(a == RGB(245, 243, 233), "Baidu cream land");

  expect(app::map_scene_river_color() == RGB(100, 160, 208), "river blue");
  expect(app::map_scene_admin_stroke_color() == RGB(196, 190, 176),
         "admin stroke");
  expect(app::map_scene_point_fill_color() == RGB(90, 110, 130), "point soft");
  expect(app::map_scene_map_bg_color() == RGB(170, 211, 223), "ocean bg");

  // MultiPolygon must expand every part (Xinjiang/Qinghai holes otherwise).
  {
    char tmp[MAX_PATH] = {};
    const DWORD n = GetTempPathA(MAX_PATH, tmp);
    expect(n > 0 && n < MAX_PATH, "temp path");
    std::string path = std::string(tmp) + "map_scene_multipart_area.geojson";
    expect(write_multipart_geojson(path.c_str()), "write multipart geojson");
    app::MapScene scene;
    expect(scene.open_path(path), "open multipart geojson");
    expect(scene.last_open_was_ogr(), "multipart open via OGR");
    expect(scene.feature_count() >= 2,
           "MultiPolygon expands to one feature per part");
    std::vector<gis::LonLatRing> rings;
    scene.export_land_rings(&rings);
    expect(rings.size() >= 2, "land rings cover every MultiPolygon part");
    DeleteFileA(path.c_str());
  }

  // Prefecture pack: area rings must exceed OGR feature count (370 MultiPolygons
  // expand to ~1000+ exterior parts). Skip quietly when data is not beside cwd.
  {
    const char* city_candidates[] = {
        "china_city.gpkg",
        "china_city.geojson",
        "out\\china_city.gpkg",
        "out\\china_city.geojson",
        "testing\\data\\china_city.gpkg",
        "testing\\data\\china_city.geojson",
    };
    for (const char* cand : city_candidates) {
      app::MapScene scene;
      if (!scene.open_path(cand) || !scene.last_open_was_ogr()) {
        continue;
      }
      std::vector<gis::LonLatRing> rings;
      scene.export_land_rings(&rings);
      expect(rings.size() > 400,
             "china_city MultiPolygon parts expanded (>370)");
      // Paint expanded multiparts to a memory DC (catches GDI AVs early).
      HDC screen = GetDC(nullptr);
      HDC mem = CreateCompatibleDC(screen);
      BITMAPINFO bmi = {};
      bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
      bmi.bmiHeader.biWidth = 800;
      bmi.bmiHeader.biHeight = -600;
      bmi.bmiHeader.biPlanes = 1;
      bmi.bmiHeader.biBitCount = 32;
      bmi.bmiHeader.biCompression = BI_RGB;
      void* bits = nullptr;
      HBITMAP dib = CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
      expect(dib != nullptr, "CreateDIBSection for paint");
      HGDIOBJ old = SelectObject(mem, dib);
      scene.fit_extent(800, 600);
      scene.paint(mem, 800, 600);
      SelectObject(mem, old);
      if (dib) {
        DeleteObject(dib);
      }
      DeleteDC(mem);
      ReleaseDC(nullptr, screen);
      expect(scene.feature_count() > 1500,
             "china_city total features after Multi* expand");
      // Mainland framing: rivers / SCS must not zoom past leftover envelope.
      // 800x600 letterboxes the 62x36 deg China box, so lat span expands;
      // lon stays tight to 73–135 because width is the limiting axis.
      const content::Extent2 view = scene.view_world_extent(800, 600);
      expect(view.xmin >= 72.0 && view.xmax <= 136.0,
             "fit lon stays near mainland");
      expect(view.xmax - view.xmin <= 70.0, "fit lon span near mainland width");
      expect(view.ymin < 25.0 && view.ymax > 45.0,
             "fit covers mainland core latitudes");
      break;
    }
  }

  // china_city "line" layer: Siberia stub must be clipped to mainland.
  {
    char tmp[MAX_PATH] = {};
    expect(GetTempPathA(MAX_PATH, tmp) > 0, "temp path for line clip");
    std::string dir = std::string(tmp) + "map_scene_line_clip_dir";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::string path = dir + "\\line.geojson";
    expect(write_line_with_siberia_stub(path.c_str()), "write line.geojson");
    app::MapScene scene;
    expect(scene.open_path(path), "open line.geojson");
    expect(scene.last_open_was_ogr(), "line clip via OGR");
    const content::Extent2 world = scene.world_extent();
    expect(world.ymax <= 54.5, "Siberia stub clipped from line layer");
    expect(world.ymin >= 18.0 && world.ymax <= 54.0,
           "remaining run is mainland lat");
    DeleteFileA(path.c_str());
    RemoveDirectoryA(dir.c_str());
  }

  if (g_fails) {
    std::fprintf(stderr, "%d map_scene_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("map_scene_test ok\n");
  return 0;
}
