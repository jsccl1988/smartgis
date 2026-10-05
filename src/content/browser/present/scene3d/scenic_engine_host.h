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

class MapScene;
class OrbitFrame;
class ViewFrame;

namespace detail {

// Hosts content scenic::Engine (HWND-free MemFrame) when
// --scene3d-engine=scenic. Overlay TIN with albedo stays on FlyCube/GDI.
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
               const MapScene* scene, const ViewFrame* labels);
  bool paint_hdc(HDC hdc, int width_px, int height_px, const OrbitFrame* orbit,
                 const MapScene* scene, const ViewFrame* labels);
  bool export_bmp(const std::string& path, int width_px, int height_px,
                  const OrbitFrame* orbit, const MapScene* scene,
                  const ViewFrame* labels);

  void shutdown();

 private:
  bool ensure_locked();
  void sync_locked(uint32_t width_px, uint32_t height_px,
                   const OrbitFrame* orbit, const MapScene* scene,
                   const ViewFrame* labels);

  mutable std::mutex mu_;
  std::unique_ptr<scenic::Engine> engine_;
  std::vector<scenic::Vertex2> xy_;
  std::vector<scenic::DrawItem> items_;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_SCENIC_ENGINE_HOST_H_
