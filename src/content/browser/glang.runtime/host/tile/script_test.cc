// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/host/tile/verbs.h"
#include "gis/tile/protocol/lonlat_xyz.h"

#include "gtest/gtest.h"

#include <string>

TEST(TileScript, BboxVerb) {
  content::code::ScriptCall call;
  call.name = "tile_bbox";
  call.positional = {"1", "0", "0"};
  std::string result;
  std::string error;
  EXPECT_TRUE(content::tile::try_script(call, &result, &error));
  EXPECT_TRUE(error.empty());
  const gis::tile::LonLatBox box = gis::tile::tile_lonlat_bbox(1, 0, 0);
  EXPECT_NE(result.find(std::to_string(static_cast<int>(box.west))),
            std::string::npos);
}
