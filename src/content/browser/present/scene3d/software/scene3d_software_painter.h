// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_SOFTWARE_SCENE3D_SOFTWARE_PAINTER_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_SOFTWARE_SCENE3D_SOFTWARE_PAINTER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstddef>
#include <cstdint>

namespace content {

class AtmosphereSession;
class MapScene;
class OrbitFrame;
class Scene3dGpuPresent;
class ViewFrame;

// GDI software paint / HUD / engine-logo overlay for 3D present.
class Scene3dSoftwarePainter {
 public:
  Scene3dSoftwarePainter() = default;
  ~Scene3dSoftwarePainter();

  Scene3dSoftwarePainter(const Scene3dSoftwarePainter&) = delete;
  Scene3dSoftwarePainter& operator=(const Scene3dSoftwarePainter&) = delete;

  void bind(Scene3dGpuPresent* gpu, AtmosphereSession* atmosphere,
            const OrbitFrame* orbit, const MapScene* scene,
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
  // Cached full-mesh AABB + DEM elev range; keyed by xyz identity so soft
  // paint / BMP export skip re-scanning every vertex each frame.
  struct MeshPrepCache {
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

  void project(float x, float y, float z, int width_px, int height_px, int* sx,
               int* sy) const;
  void project_lon_lat(double lon, double lat, int width_px, int height_px,
                       int* sx, int* sy) const;
  void paint_wind_arrows(HDC hdc, int width_px, int height_px) const;
  void paint_wireframe_edges(HDC hdc, int width_px, int height_px) const;
  void sync_engine_logo_overlay(HWND parent, int width_px,
                                int height_px) const;
  void hide_engine_logo_overlay() const;
  void refresh_mesh_prep_cache(size_t dem_idx_end) const;
  HFONT ensure_place_label_font() const;

  Scene3dGpuPresent* gpu_ = nullptr;
  AtmosphereSession* atmosphere_ = nullptr;
  const OrbitFrame* orbit_ = nullptr;
  const MapScene* scene_ = nullptr;
  const ViewFrame* label_frame_ = nullptr;
  bool hosts_shared_scene_ = false;
  mutable HWND logo_hwnd_ = nullptr;
  mutable HFONT place_label_font_ = nullptr;
  mutable MeshPrepCache mesh_prep_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_SOFTWARE_SCENE3D_SOFTWARE_PAINTER_H_
