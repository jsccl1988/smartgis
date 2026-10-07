// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_SCENIC_ENGINE_HOST_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_SCENIC_ENGINE_HOST_H_

#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "scenic/engine.h"

namespace content {

class GisScene;
class OrbitFrame;
class ViewFrame;

namespace detail {

// Opt-in scenic::Engine host (--scene3d-engine=scenic only).
// Product scene3d SoT is Vista WorldPass; ensure() returns false on the
// default product path and tears down any sticky scenic engine.
class ScenicScene3dHost {
 public:
  ScenicScene3dHost();
  ~ScenicScene3dHost();

  ScenicScene3dHost(const ScenicScene3dHost&) = delete;
  ScenicScene3dHost& operator=(const ScenicScene3dHost&) = delete;

  bool is_live() const;
  // True when a live engine exists. Drops the host if SoT is no longer scenic.
  bool ensure();

  bool present(uint32_t width_px, uint32_t height_px, const OrbitFrame* orbit,
               const GisScene* scene, const ViewFrame* labels);
  bool paint_hdc(HDC hdc, int width_px, int height_px, const OrbitFrame* orbit,
                 const GisScene* scene, const ViewFrame* labels);
  bool export_bmp(const std::string& path, int width_px, int height_px,
                  const OrbitFrame* orbit, const GisScene* scene,
                  const ViewFrame* labels);

  void shutdown();

 private:
  bool ensure_locked();
  void sync_locked(uint32_t width_px, uint32_t height_px,
                   const OrbitFrame* orbit, const GisScene* scene,
                   const ViewFrame* labels);
  bool sync_inputs_unchanged(uint32_t width_px, uint32_t height_px,
                             const OrbitFrame* orbit, const GisScene* scene,
                             const ViewFrame* labels) const;
  void remember_sync_inputs(uint32_t width_px, uint32_t height_px,
                            const OrbitFrame* orbit, const GisScene* scene,
                            const ViewFrame* labels);

  mutable std::mutex mu_;
  std::unique_ptr<scenic::Engine> engine_;
  std::vector<scenic::Vertex2> xy_;
  std::vector<scenic::DrawItem> items_;

  // Last successful sync fingerprint — skip fill_scenic_draw_items + rebind
  // when present/paint repeats the same viewport/orbit/scene (matrix warm).
  uint32_t sync_w_ = 0;
  uint32_t sync_h_ = 0;
  const OrbitFrame* sync_orbit_ = nullptr;
  float sync_yaw_ = 0.f;
  float sync_pitch_ = 0.f;
  float sync_distance_ = 0.f;
  const GisScene* sync_scene_ = nullptr;
  const ViewFrame* sync_labels_ = nullptr;
  double sync_label_scale_ = 0.0;
  bool sync_valid_ = false;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_SCENIC_ENGINE_HOST_H_
