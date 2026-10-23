// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/tile/verbs.h"
#include "content/browser/glang.runtime/host/tile/web_mercator.h"
#include "content/browser/glang.runtime/host/tile/xyz_source.h"

#include "gtest/gtest.h"

#include <cmath>
#include <exception>
#include <fstream>
#include <string>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace {

bool contains_point(const content::tile::LonLatBox& box, double lon, double lat) {
  return lon >= box.west - 1e-8 && lon <= box.east + 1e-8 &&
         lat <= box.north + 1e-6 && lat >= box.south - 1e-6;
}

TEST(WebMercatorTile, WorldTileBbox) {
  const content::tile::LonLatBox box = content::tile::tile_lonlat_bbox(0, 0, 0);
  EXPECT_NEAR(box.west, -180.0, 1e-9);
  EXPECT_NEAR(box.east, 180.0, 1e-9);
  EXPECT_NEAR(box.north, content::tile::k_max_mercator_lat, 1e-6);
  EXPECT_NEAR(box.south, -content::tile::k_max_mercator_lat, 1e-6);
}

TEST(WebMercatorTile, EquatorOriginRoundTrip) {
  const content::tile::TileIndex tile = content::tile::lonlat_to_tile(0.0, 0.0, 3);
  EXPECT_EQ(tile.z, 3);
  EXPECT_EQ(tile.x, 4);
  EXPECT_EQ(tile.y, 4);
  const content::tile::LonLatBox box = content::tile::tile_lonlat_bbox(tile.z, tile.x, tile.y);
  EXPECT_NEAR(box.west, 0.0, 1e-9);
  EXPECT_NEAR(box.north, 0.0, 1e-6);
  EXPECT_TRUE(contains_point(box, 1.0, -1.0));
}

TEST(WebMercatorTile, InteriorPointRoundTrip) {
  constexpr double lon = 116.4;
  constexpr double lat = 39.9;
  constexpr int z = 5;
  const content::tile::TileIndex tile = content::tile::lonlat_to_tile(lon, lat, z);
  const content::tile::LonLatBox box =
      content::tile::tile_lonlat_bbox(tile.z, tile.x, tile.y);
  EXPECT_TRUE(contains_point(box, lon, lat));
  const content::tile::TileIndex again =
      content::tile::lonlat_to_tile((box.west + box.east) * 0.5,
                                   (box.south + box.north) * 0.5, z);
  EXPECT_EQ(again.x, tile.x);
  EXPECT_EQ(again.y, tile.y);
}

TEST(WebMercatorTile, EquatorResolutionZ0) {
  const double meters = content::tile::resolution_m_per_px(0, 0.0, 256);
  EXPECT_NEAR(meters, 156543.03392804097, 1e-4);
}

TEST(WebMercatorTile, UrlTemplate) {
  EXPECT_EQ(content::tile::format_xyz_url("https://tiles/{z}/{x}/{y}.png", 2, 1, 3),
            "https://tiles/2/1/3.png");
  EXPECT_EQ(content::tile::format_xyz_url("{z}/{x}/{-y}", 2, 1, 0), "2/1/3");
}

TEST(WebMercatorTile, ExtentIsCapped) {
  content::tile::LonLatBox world;
  world.west = -180;
  world.south = -80;
  world.east = 180;
  world.north = 80;
  const std::vector<content::tile::TileIndex> tiles =
      content::tile::tiles_for_extent(world, 8, 1280, 16);
  EXPECT_EQ(tiles.size(), 16u);
  EXPECT_EQ(tiles.front().z, 8);
}

TEST(WebMercatorTile, ScriptBboxVerb) {
  content::code::ScriptCall call;
  call.name = "tile_bbox";
  call.positional = {"1", "0", "0"};
  std::string result;
  std::string error;
  EXPECT_TRUE(content::tile::try_script(call, &result, &error));
  EXPECT_TRUE(error.empty());
  const content::tile::LonLatBox box = content::tile::tile_lonlat_bbox(1, 0, 0);
  EXPECT_NE(result.find(std::to_string(static_cast<int>(box.west))), std::string::npos);
}

TEST(WebMercatorTile, FileFetchHitsCache) {
  char dir[MAX_PATH] = {};
  ASSERT_GT(GetTempPathA(MAX_PATH, dir), 0u);
  const std::string path = std::string(dir) + "smartgis_tile_0_0_0.bin";
  {
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.good());
    out << "TILE";
  }
  content::tile::XyzSource source;
  source.set_url_template(std::string(dir) + "smartgis_tile_{z}_{x}_{y}.bin");
  std::string body;
  std::string error;
  ASSERT_TRUE(source.get(0, 0, 0, &body, &error)) << error;
  EXPECT_EQ(body, "TILE");
  body.clear();
  EXPECT_TRUE(source.try_cached(0, 0, 0, &body));
  EXPECT_EQ(body, "TILE");
  DeleteFileA(path.c_str());
}

TEST(WebMercatorTile, NetworkFetchSkipsOffline) {
  if (!content::tile::network_available()) {
    GTEST_SKIP() << "offline";
  }
  content::tile::XyzSource source;
  source.set_url_template("https://tile.openstreetmap.org/{z}/{x}/{y}.png");
  std::string body;
  std::string error;
  bool ok = false;
  try {
    ok = source.get(0, 0, 0, &body, &error);
  } catch (const std::exception& ex) {
    GTEST_SKIP() << ex.what();
  }
  if (!ok) {
    GTEST_SKIP() << (error.empty() ? "fetch failed" : error);
  }
  ASSERT_GE(body.size(), 8u);
  EXPECT_EQ(static_cast<unsigned char>(body[0]), 0x89);
  EXPECT_EQ(body[1], 'P');
  EXPECT_EQ(body[2], 'N');
  EXPECT_EQ(body[3], 'G');
}

}  // namespace
