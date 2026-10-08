// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_MESH_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_MESH_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace content {

class AtmosphereSession;
class OrbitFrame;
class Scene3dGpuPresent;

namespace detail {

// Cached full-mesh AABB + DEM elev range; keyed by xyz identity so soft
// paint / BMP export skip re-scanning every vertex each frame.
struct Scene3dMeshPrepCache {
  const float* xyz_ptr = nullptr;
  size_t xyz_n = 0;
  size_t dem_idx_end = 0;
  // Cheap content stamp (first/mid/last samples) so in-place mesh edits
  // invalidate without a full rescan every frame.
  uint64_t stamp = 0;
  float minx = 0.f;
  float maxx = 0.f;
  float miny = 0.f;
  float maxy = 0.f;
  float minz = 0.f;
  float maxz = 0.f;
  float elev_min = 0.f;
  float elev_max = 0.f;
  bool elev_ok = false;
  bool aabb_ok = false;
};

COLORREF hypsometric_rgb(float t01);

void refresh_mesh_prep_cache(Scene3dMeshPrepCache* cache,
                             Scene3dGpuPresent* gpu, size_t dem_idx_end);

// Soft DEM body after mesh rebuild: ocean plane, hypsometric tris, overlay
// TIN (water/hex), and borehole beads. Caller holds gpu mutex.
void paint_soft_scene_body(HDC hdc, int width_px, int height_px,
                           Scene3dGpuPresent* gpu,
                           AtmosphereSession* atmosphere,
                           const OrbitFrame* orbit,
                           Scene3dMeshPrepCache* mesh_prep);

void paint_soft_wireframe_edges(HDC hdc, int width_px, int height_px,
                                Scene3dGpuPresent* gpu,
                                const OrbitFrame* orbit);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_MESH_H_
