// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_LOOK_PRESET_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_LOOK_PRESET_H_

#include <string>
#include <vector>

namespace content {

// Product look for Scene3D: default atmosphere vs leftover stereo parity.
enum class Scene3dLookPreset {
  kAtmosphere = 0,
  kLegacyStereo = 1,
};

// Screen-space / orbit place-name for leftover-style stereo labels.
// |priority| matches MapLabelBatch: lower wins occupancy (0 = municipality).
struct Scene3dLegacyLabel {
  std::string text;
  double lon = 0;
  double lat = 0;
  int priority = 2;
};

// Best-effort China place-names (UTF-8). Idempotent append if |out| is empty.
void fill_china_legacy_labels(std::vector<Scene3dLegacyLabel>* out);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_LOOK_PRESET_H_
