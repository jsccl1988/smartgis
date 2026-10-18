// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef PLUGIN_WORLD3D_SCENE_LOOK_CHINA_STACK_H_
#define PLUGIN_WORLD3D_SCENE_LOOK_CHINA_STACK_H_

namespace content {
class OrbitFrame;
class Scene3dPresenter;
}  // namespace content

namespace plugin {

// Cumulative China planar face: DEM, then ocean, fog, sky, cloud.
// Later stages stay off. Not a product World3dLook.
enum class ChinaStackStage {
  kDem = 0,
  kOcean,
  kFog,
  kSky,
  kCloud,
};

// Enable mask for one stage. Earlier layers stay on; later layers stay off.
struct ChinaStackMask {
  bool ocean = false;
  bool fog = false;
  bool sky = false;
  bool cloud = false;
};

inline constexpr ChinaStackStage kChinaStackOrder[] = {
    ChinaStackStage::kDem,   ChinaStackStage::kOcean, ChinaStackStage::kFog,
    ChinaStackStage::kSky,   ChinaStackStage::kCloud,
};

inline constexpr int kChinaStackStageCount = 5;

inline ChinaStackMask china_stack_mask(ChinaStackStage stage) {
  switch (stage) {
    case ChinaStackStage::kDem:
      return {};
    case ChinaStackStage::kOcean:
      return {true, false, false, false};
    case ChinaStackStage::kFog:
      return {true, true, false, false};
    case ChinaStackStage::kSky:
      return {true, true, true, false};
    case ChinaStackStage::kCloud:
      return {true, true, true, true};
  }
  return {};
}

const char* china_stack_stage_name(ChinaStackStage stage);

// Land DEM seed plus this stage's mask. One China orbit (full-face distance
// and pitch) so a later stage cannot pass by reframing. Globe stays off.
bool apply_china_stack_stage(content::Scene3dPresenter* cam,
                             content::OrbitFrame* orbit,
                             ChinaStackStage stage);

// Reads existing ocean/fog/sky/cloud/globe/sat switches. No GPU readback.
bool verify_china_stack_stage(content::Scene3dPresenter* cam,
                              ChinaStackStage stage);

}  // namespace plugin

#endif  // PLUGIN_WORLD3D_SCENE_LOOK_CHINA_STACK_H_
