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

  // Categorical pastel fills must differ across adcodes (SmartGis EDIT look).
  const COLORREF a = app::map_scene_area_fill_color("110000", 1);
  const COLORREF b = app::map_scene_area_fill_color("320100", 2);
  const COLORREF c = app::map_scene_area_fill_color("510100", 3);
  expect(a != b || b != c, "pastel palette varies by adcode/id");
  expect(GetRValue(a) > 120 || GetGValue(a) > 120 || GetBValue(a) > 120,
         "area fill is pastel/light, not dark blue");

  expect(app::map_scene_river_color() == RGB(64, 140, 196), "river blue");
  expect(app::map_scene_admin_stroke_color() == RGB(58, 70, 84),
         "admin stroke");
  expect(app::map_scene_point_fill_color() == RGB(20, 20, 20), "point black");
  expect(app::map_scene_map_bg_color() == RGB(255, 255, 255), "2D white bg");

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
      break;
    }
  }

  if (g_fails) {
    std::fprintf(stderr, "%d map_scene_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("map_scene_test ok\n");
  return 0;
}
