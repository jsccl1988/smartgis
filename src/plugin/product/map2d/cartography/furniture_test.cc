// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/product/map2d/cartography/furniture.h"
#include "plugin/product/map2d/pyramid/pyramid.h"
#include "plugin/runtime/host/capability/map2d_sink.h"
#include "ui/gfx/canvas/canvas.h"
#include "ui/gfx/display_list/display_list.h"

#include "gtest/gtest.h"

namespace {

TEST(MapFurniture, ScaleBarFromViewExtent) {
  plugin::MapViewBox view;
  view.west = 116.0;
  view.south = 39.5;
  view.east = 117.0;
  view.north = 40.5;
  view.width_px = 1280;
  view.height_px = 720;
  const plugin::FurnitureLayout layout = plugin::layout_furniture(view, 96.0);
  EXPECT_TRUE(layout.valid);
  EXPECT_GT(layout.outer_w, layout.inner_w);
  EXPECT_GT(layout.inner_w, 0);
  EXPECT_GT(layout.bar_len_px, 0);
  EXPECT_GT(layout.bar_meters, 0.0);
  EXPECT_GT(layout.scale_denominator, 1.0);
  const std::string ratio = plugin::scale_ratio_label(layout.scale_denominator);
  EXPECT_EQ(ratio.compare(0, 2, "1:"), 0);
  const std::string bar = plugin::scale_bar_label(layout.bar_meters);
  EXPECT_FALSE(bar.empty());
}

TEST(MapFurniture, TinyViewSkipsBar) {
  plugin::MapViewBox view;
  view.width_px = 40;
  view.height_px = 40;
  EXPECT_FALSE(plugin::layout_furniture(view).valid);
}

plugin::MapViewBox sample_view() {
  plugin::MapViewBox view;
  view.west = 116.0;
  view.south = 39.5;
  view.east = 117.0;
  view.north = 40.5;
  view.width_px = 1280;
  view.height_px = 720;
  return view;
}

size_t paint_cmd_count(const plugin::FurnitureLayout& layout,
                       const plugin::FurnitureOptions& options) {
  ui::gfx::DisplayList list;
  ui::gfx::display_list_begin(&list);
  {
    ui::gfx::Canvas canvas(nullptr, layout.outer_w + 32, layout.outer_h + 32);
    plugin::paint_furniture(&canvas, layout, options);
    ui::gfx::display_list_end();
  }
  return list.cmd_count();
}

TEST(MapFurniture, OptionsSkipPieces) {
  const plugin::FurnitureLayout layout = plugin::layout_furniture(sample_view());
  ASSERT_TRUE(layout.valid);

  plugin::FurnitureOptions all;
  plugin::FurnitureOptions no_border = all;
  no_border.border = false;
  plugin::FurnitureOptions no_scale = all;
  no_scale.scale_bar = false;
  plugin::FurnitureOptions no_north = all;
  no_north.north_arrow = false;
  plugin::FurnitureOptions none;
  none.scale_bar = false;
  none.north_arrow = false;
  none.border = false;

  const size_t all_n = paint_cmd_count(layout, all);
  const size_t no_border_n = paint_cmd_count(layout, no_border);
  const size_t no_scale_n = paint_cmd_count(layout, no_scale);
  const size_t no_north_n = paint_cmd_count(layout, no_north);
  EXPECT_GT(all_n, 0u);
  EXPECT_EQ(paint_cmd_count(layout, none), 0u);
  EXPECT_LT(no_border_n, all_n);
  EXPECT_LT(no_scale_n, all_n);
  EXPECT_LT(no_north_n, all_n);
  EXPECT_EQ(no_border_n + no_scale_n + no_north_n, all_n * 2);

  plugin::FurnitureLayout tiny;
  tiny.valid = false;
  EXPECT_EQ(paint_cmd_count(tiny, all), 0u);
}

TEST(MapFurniture, SessionStoresFlags) {
  plugin::FurnitureOptions off;
  off.scale_bar = false;
  off.north_arrow = false;
  off.border = true;
  plugin::set_map_furniture(off);
  const plugin::FurnitureOptions got = plugin::map_furniture();
  EXPECT_FALSE(got.scale_bar);
  EXPECT_FALSE(got.north_arrow);
  EXPECT_TRUE(got.border);
  plugin::set_map_furniture({});
  const plugin::FurnitureOptions restored = plugin::map_furniture();
  EXPECT_TRUE(restored.scale_bar);
  EXPECT_TRUE(restored.north_arrow);
  EXPECT_TRUE(restored.border);
}

TEST(MapFurniture, SinkBridgesRoundTrip) {
  plugin::Map2dSink sink;
  EXPECT_TRUE(sink.furniture_scale_bar());
  EXPECT_TRUE(sink.furniture_north_arrow());
  EXPECT_TRUE(sink.furniture_border());
  EXPECT_FALSE(sink.furniture_bridges_installed());

  bool scale = true;
  bool north = true;
  bool border = true;
  int calls = 0;
  sink.set_furniture_bridges([&](bool s, bool n, bool b) {
    ++calls;
    scale = s;
    north = n;
    border = b;
  });
  EXPECT_TRUE(sink.furniture_bridges_installed());
  EXPECT_EQ(calls, 1);
  EXPECT_TRUE(scale);

  sink.set_furniture(false, true, false);
  EXPECT_EQ(calls, 2);
  EXPECT_FALSE(sink.furniture_scale_bar());
  EXPECT_TRUE(sink.furniture_north_arrow());
  EXPECT_FALSE(sink.furniture_border());
  EXPECT_FALSE(scale);
  EXPECT_TRUE(north);
  EXPECT_FALSE(border);
}

}  // namespace
