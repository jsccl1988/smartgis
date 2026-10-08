// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_PAINTER_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_PAINTER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/present/scene3d/hdc/scene3d_hdc_mesh.h"

namespace content {

class AtmosphereSession;
class GisScene;
class OrbitFrame;
class Scene3dGpuPresent;
class ViewFrame;

// Opt-in HDC paint / HUD / engine-logo overlay for 3D present.
// Orchestrates detail::scene3d_hdc_{mesh,hud,logo}; keep this facade thin.
class Scene3dHdcPainter {
 public:
  Scene3dHdcPainter() = default;
  ~Scene3dHdcPainter();

  Scene3dHdcPainter(const Scene3dHdcPainter&) = delete;
  Scene3dHdcPainter& operator=(const Scene3dHdcPainter&) = delete;

  void bind(Scene3dGpuPresent* gpu, AtmosphereSession* atmosphere,
            const OrbitFrame* orbit, const GisScene* scene,
            const ViewFrame* label_frame);
  void set_hosts_shared_scene(bool on) { hosts_shared_scene_ = on; }

  void paint(HDC hdc, int width_px, int height_px,
             bool fill_background = true) const;
  void paint_hud(HDC hdc, int width_px, int height_px) const;
  // Leftover-style place-names (white + black outline). Safe on any HDC;
  // DXGI flip surfaces need this drawn onto a capture BMP or layered child.
  void paint_legacy_place_labels(HDC hdc, int width_px, int height_px) const;

  static void paint_engine_logo(HDC hdc, int width_px, int height_px,
                                const char* engine_name);

  void release_engine_logo_overlay() const;

 private:
  Scene3dGpuPresent* gpu_ = nullptr;
  AtmosphereSession* atmosphere_ = nullptr;
  const OrbitFrame* orbit_ = nullptr;
  const GisScene* scene_ = nullptr;
  const ViewFrame* label_frame_ = nullptr;
  bool hosts_shared_scene_ = false;
  mutable HWND logo_hwnd_ = nullptr;
  mutable HFONT place_label_font_ = nullptr;
  mutable detail::Scene3dMeshPrepCache mesh_prep_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_HDC_SCENE3D_HDC_PAINTER_H_
