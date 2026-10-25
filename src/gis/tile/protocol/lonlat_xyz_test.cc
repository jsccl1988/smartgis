// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/tile/protocol/lonlat_xyz.h"
#include "gis/tile/provider/xyz_source.h"

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

bool contains_point(const gis::tile::LonLatBox& box, double lon, double lat) {
  return lon >= box.west - 1e-8 && lon <= box.east + 1e-8 &&
         lat <= box.north + 1e-6 && lat >= box.south - 1e-6;
}

}  // namespace

TEST(LonLatXyz, WorldTileBbox) {
  const gis::tile::LonLatBox box = gis::tile::tile_lonlat_bbox(0, 0, 0);
  EXPECT_NEAR(box.west, -180.0, 1e-9);
  EXPECT_NEAR(box.east, 180.0, 1e-9);
  EXPECT_NEAR(box.north, gis::tile::k_max_mercator_lat, 1e-6);
  EXPECT_NEAR(box.south, -gis::tile::k_max_mercator_lat, 1e-6);
}

TEST(LonLatXyz, EquatorOriginRoundTrip) {
  const gis::tile::TileIndex tile = gis::tile::lonlat_to_tile(0.0, 0.0, 3);
  EXPECT_EQ(tile.z, 3);
  EXPECT_EQ(tile.x, 4);
  EXPECT_EQ(tile.y, 4);
  const gis::tile::LonLatBox box =
      gis::tile::tile_lonlat_bbox(tile.z, tile.x, tile.y);
  EXPECT_NEAR(box.west, 0.0, 1e-9);
  EXPECT_NEAR(box.north, 0.0, 1e-6);
  EXPECT_TRUE(contains_point(box, 1.0, -1.0));
}

TEST(LonLatXyz, InteriorPointRoundTrip) {
  constexpr double lon = 116.4;
  constexpr double lat = 39.9;
  constexpr int z = 5;
  const gis::tile::TileIndex tile = gis::tile::lonlat_to_tile(lon, lat, z);
  const gis::tile::LonLatBox box =
      gis::tile::tile_lonlat_bbox(tile.z, tile.x, tile.y);
  EXPECT_TRUE(contains_point(box, lon, lat));
  const gis::tile::TileIndex again = gis::tile::lonlat_to_tile(
      (box.west + box.east) * 0.5, (box.south + box.north) * 0.5, z);
  EXPECT_EQ(again.x, tile.x);
  EXPECT_EQ(again.y, tile.y);
}

TEST(LonLatXyz, EquatorResolutionZ0) {
  const double meters = gis::tile::resolution_m_per_px(0, 0.0, 256);
  EXPECT_NEAR(meters, 156543.03392804097, 1e-4);
}

TEST(LonLatXyz, UrlTemplate) {
  EXPECT_EQ(gis::tile::format_xyz_url("https://tiles/{z}/{x}/{y}.png", 2, 1, 3),
            "https://tiles/2/1/3.png");
  EXPECT_EQ(gis::tile::format_xyz_url("{z}/{x}/{-y}", 2, 1, 0), "2/1/3");
}

TEST(LonLatXyz, ExtentIsCapped) {
  gis::tile::LonLatBox world;
  world.west = -180;
  world.south = -80;
  world.east = 180;
  world.north = 80;
  const std::vector<gis::tile::TileIndex> tiles =
      gis::tile::tiles_for_extent(world, 8, 1280, 16);
  EXPECT_EQ(tiles.size(), 16u);
  EXPECT_EQ(tiles.front().z, 8);
}

TEST(LonLatXyz, FileFetchHitsCache) {
  char dir[MAX_PATH] = {};
  ASSERT_GT(GetTempPathA(MAX_PATH, dir), 0u);
  const std::string path = std::string(dir) + "smartgis_tile_0_0_0.bin";
  {
    std::ofstream out(path, std::ios::binary);
    ASSERT_TRUE(out.good());
    out << "TILE";
  }
  gis::tile::XyzSource source;
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

TEST(LonLatXyz, NetworkFetchSkipsOffline) {
  if (!gis::tile::network_available()) {
    GTEST_SKIP() << "offline";
  }
  gis::tile::XyzSource source;
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
