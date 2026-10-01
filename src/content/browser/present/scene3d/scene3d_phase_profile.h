// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_SCENE3D_PHASE_PROFILE_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_SCENE3D_PHASE_PROFILE_H_

#include <cstdint>

namespace content {

// Last equal-profile phase sample for Scene3D GPU present.
// mesh = DEM/local rebuild; sync = World→GpuScene; rebuild = GpuScene::rebuild_meshes;
// record = graph/opaque/ocean/sky; present = Device::present; ocean_prep = height upload.
struct Scene3dPhaseSample {
  int64_t mesh_ms = 0;
  int64_t sync_ms = 0;
  int64_t rebuild_ms = 0;
  int64_t ocean_prep_ms = 0;
  int64_t record_ms = 0;
  int64_t present_ms = 0;
  int rebuild_count = 0;
};

Scene3dPhaseSample scene3d_last_phase_sample();
void reset_scene3d_phase_sample();

void note_scene3d_phase_mesh(int64_t mesh_ms);
void note_scene3d_phase_sync(int64_t sync_ms);
void note_scene3d_phase_rebuild(int64_t rebuild_ms, int count);
void note_scene3d_phase_ocean_prep(int64_t ocean_prep_ms);
void note_scene3d_phase_record(int64_t record_ms);
void note_scene3d_phase_present(int64_t present_ms);

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_SCENE3D_PHASE_PROFILE_H_
