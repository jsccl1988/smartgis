// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/geo/xy/xy.h"

#include "gtest/gtest.h"

TEST(GeoXyTest, ConvexHullSquareArea) {
  std::vector<geo::Xy> pts = {
      {0, 0}, {10, 0}, {10, 10}, {0, 10}, {5, 5},
  };
  const std::vector<geo::Xy> hull = geo::convex_hull_xy(std::move(pts));
  EXPECT_EQ(hull.size(), 4u);
  EXPECT_NEAR(geo::polygon_area_xy(hull), 100.0, 1e-9);
}

TEST(GeoXyTest, BboxAreaFallback) {
  std::vector<geo::Xy> pts = {{0, 0}, {10, 0}};
  EXPECT_NEAR(geo::bbox_area_xy(pts), 0.0, 1e-12);
  pts.push_back({10, 5});
  EXPECT_NEAR(geo::bbox_area_xy(pts), 50.0, 1e-9);
}

TEST(GeoXyTest, DegenerateHull) {
  std::vector<geo::Xy> pts = {{1, 1}, {1, 1}};
  const std::vector<geo::Xy> hull = geo::convex_hull_xy(std::move(pts));
  EXPECT_LE(hull.size(), 1u);
  EXPECT_EQ(geo::polygon_area_xy(hull), 0.0);
}
