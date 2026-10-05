// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_SCENE3D_OVERLAYS_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_SCENE3D_OVERLAYS_H_

#include <cstddef>
#include <cstdint>
#include <vector>

#include "content/browser/present/scene3d/frame/orbit_geo_frame.h"
#include "vista/component/world/world.h"

namespace content {

// Geographic overlay buffers (point cloud + TIN) and World attach.
// Caller holds the present mutex. GPU present and GDI both consume this.
class Scene3dOverlays {
 public:
  void set_pointcloud(const float* xyz_lon_lat_elev, int point_count,
                      const uint8_t* rgba);
  void clear_pointcloud();
  void set_tin(const float* xyz_lon_lat_elev, int point_count,
               const unsigned* indices, int index_count,
               const uint8_t* albedo_rgba);
  void set_tin_drape(const uint8_t* rgba, uint32_t width, uint32_t height,
                     const float* uv, int uv_float_count);
  void clear_tin();

  void attach_pointcloud(vista::World* world, const OrbitGeoFrame& geo,
                         const std::vector<float>& local_xyz,
                         size_t dem_xyz_count, bool force);
  void attach_tin(vista::World* world, const OrbitGeoFrame& geo,
                  std::vector<float>* local_xyz,
                  std::vector<unsigned>* local_idx, size_t dem_xyz_count,
                  size_t dem_idx_count, bool force);

  const std::vector<float>& xyz_geo() const { return xyz_geo_; }
  const std::vector<uint8_t>& rgba() const { return rgba_; }
  bool tin_has_albedo() const { return tin_has_albedo_; }
  const uint8_t* tin_albedo() const { return tin_albedo_; }
  bool tin_has_drape() const {
    return tin_tex_w_ >= 8 && tin_tex_h_ >= 8 && !tin_tex_.empty() &&
           !tin_uv_.empty();
  }
  const std::vector<float>& tin_uv() const { return tin_uv_; }
  const std::vector<uint8_t>& tin_tex() const { return tin_tex_; }
  uint32_t tin_tex_w() const { return tin_tex_w_; }
  uint32_t tin_tex_h() const { return tin_tex_h_; }
  bool pointcloud_dirty() const { return pointcloud_dirty_; }
  bool tin_dirty() const { return tin_dirty_; }

 private:
  std::vector<float> xyz_geo_;
  std::vector<uint8_t> rgba_;
  std::vector<float> tin_xyz_geo_;
  std::vector<unsigned> tin_idx_;
  std::vector<float> tin_uv_;
  std::vector<uint8_t> tin_tex_;
  uint32_t tin_tex_w_ = 0;
  uint32_t tin_tex_h_ = 0;
  uint8_t tin_albedo_[4] = {46, 170, 220, 230};
  bool tin_has_albedo_ = false;
  bool pointcloud_dirty_ = false;
  bool tin_dirty_ = false;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_SCENE3D_OVERLAYS_H_
