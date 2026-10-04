// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/look_preset.h"

namespace content {

void fill_china_legacy_labels(std::vector<Scene3dLegacyLabel>* out) {
  if (!out || !out->empty()) {
    return;
  }
  // UTF-8 as \x escapes into const char* (no u8 / char8_t); paint uses CP_UTF8.
  static const struct {
    const char* text;
    double lon;
    double lat;
  } kCities[] = {
      {"\xe4\xb9\x8c\xe9\xb2\x81\xe6\x9c\xa8\xe9\xbd\x90", 87.62, 43.83},
      {"\xe6\x8b\x89\xe8\x90\xa8", 91.11, 29.97},
      {"\xe8\xa5\xbf\xe5\xae\x81", 101.78, 36.62},
      {"\xe5\x85\xb0\xe5\xb7\x9e", 103.83, 36.06},
      {"\xe9\x93\xb6\xe5\xb7\x9d", 106.27, 38.47},
      {"\xe5\x91\xbc\xe5\x92\x8c\xe6\xb5\xa9\xe7\x89\xb9", 111.75, 40.84},
      {"\xe5\x93\x88\xe5\xb0\x94\xe6\xbb\xa8", 126.53, 45.80},
      {"\xe9\x95\xbf\xe6\x98\xa5", 125.32, 43.88},
      {"\xe6\xb2\x88\xe9\x98\xb3", 123.43, 41.80},
      {"\xe5\x8c\x97\xe4\xba\xac", 116.40, 39.90},
      {"\xe5\xa4\xa9\xe6\xb4\xa5", 117.20, 39.08},
      {"\xe7\x9f\xb3\xe5\xae\xb6\xe5\xba\x84", 114.51, 38.04},
      {"\xe5\xa4\xaa\xe5\x8e\x9f", 112.55, 37.87},
      {"\xe6\xb5\x8e\xe5\x8d\x97", 117.00, 36.65},
      {"\xe9\x83\x91\xe5\xb7\x9e", 113.62, 34.75},
      {"\xe8\xa5\xbf\xe5\xae\x89", 108.94, 34.34},
      {"\xe5\x8d\x97\xe4\xba\xac", 118.78, 32.06},
      {"\xe4\xb8\x8a\xe6\xb5\xb7", 121.47, 31.23},
      {"\xe6\x9d\xad\xe5\xb7\x9e", 120.15, 30.28},
      {"\xe5\x90\x88\xe8\x82\xa5", 117.28, 31.86},
      {"\xe7\xa6\x8f\xe5\xb7\x9e", 119.30, 26.08},
      {"\xe5\x8d\x97\xe6\x98\x8c", 115.86, 28.68},
      {"\xe6\xad\xa6\xe6\xb1\x89", 114.31, 30.57},
      {"\xe9\x95\xbf\xe6\xb2\x99", 112.98, 28.19},
      {"\xe5\xb9\xbf\xe5\xb7\x9e", 113.26, 23.13},
      {"\xe5\x8d\x97\xe5\xae\x81", 108.37, 22.82},
      {"\xe6\xb5\xb7\xe5\x8f\xa3", 110.35, 20.02},
      {"\xe6\x88\x90\xe9\x83\xbd", 104.06, 30.67},
      {"\xe9\x87\x8d\xe5\xba\x86", 106.55, 29.56},
      {"\xe8\xb4\xb5\xe9\x98\xb3", 106.63, 26.65},
      {"\xe6\x98\x86\xe6\x98\x8e", 102.71, 25.04},
      {"\xe5\x8f\xb0\xe5\x8c\x97", 121.56, 25.04},
  };
  out->reserve(sizeof(kCities) / sizeof(kCities[0]));
  for (const auto& c : kCities) {
    out->push_back(Scene3dLegacyLabel{c.text, c.lon, c.lat});
  }
}

}  // namespace content
