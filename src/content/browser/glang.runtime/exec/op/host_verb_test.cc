// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/glang.runtime/language/code/op.h"

#include "gtest/gtest.h"

namespace {

TEST(HostVerbSlot, LanguageDoesNotNameSceneCommands) {
  EXPECT_EQ(content::code::find_op("invalidate_map2d"), nullptr);
  EXPECT_EQ(content::code::find_op("capture_browse_still"), nullptr);
  EXPECT_EQ(content::code::find_op("fps_bench"), nullptr);
  EXPECT_EQ(content::code::find_op("fit_scene_box"), nullptr);
  EXPECT_EQ(content::code::find_op("camera_fly"), nullptr);
  EXPECT_EQ(content::code::find_op("browse_stress"), nullptr);
  EXPECT_EQ(content::code::find_op("wait_viewport"), nullptr);
  EXPECT_EQ(content::code::find_op("expect_scene_visible"), nullptr);
  EXPECT_EQ(content::code::find_op("expect_orbit_moved"), nullptr);
  EXPECT_EQ(content::code::find_op("expect_map_hwnd_sync"), nullptr);
  EXPECT_NE(content::code::find_op("tool"), nullptr);
  EXPECT_NE(content::code::find_op("buffer"), nullptr);
  EXPECT_NE(content::code::find_op("wait_ready"), nullptr);
}

}  // namespace
