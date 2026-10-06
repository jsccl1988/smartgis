// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_PRESENTER_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_PRESENTER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <memory>
#include <string>
#include <windows.h>

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"
#include "content/browser/present/scene3d/scenic_engine_host.h"
#include "content/browser/present/scene3d/software/scene3d_software_painter.h"
#include "content/public/map_layer_types.h"
#include "render/rhi/rhi.h"
#include "tool/draft/draft.h"
#include "ui/gfx/raster/shell_raster.h"

namespace content {

class MapContents;
class MapScene;
class ViewFrame;

// Thin 3D present facade: Atmosphere + GPU/software, or hosted scenic::Engine.
class Scene3dPresenter {
 public:
  // Heap-allocate in this TU so BrowserSession (exe source_set) does not
  // embed a sizeof that can skew vs AtmosphereSession / GpuPresent under
  // parallel ninja — that overflow is STATUS_HEAP_CORRUPTION on the next
  // CRT malloc (Workspace::register_builtins).
  struct Deleter {
    void operator()(Scene3dPresenter* p) const;
  };
  using Ptr = std::unique_ptr<Scene3dPresenter, Deleter>;
  static Ptr create();

  Scene3dPresenter();
  ~Scene3dPresenter();

  Scene3dPresenter(const Scene3dPresenter&) = delete;
  Scene3dPresenter& operator=(const Scene3dPresenter&) = delete;

  AtmosphereSession& atmosphere_session() { return atmosphere_; }
  const AtmosphereSession& atmosphere_session() const { return atmosphere_; }
  Scene3dGpuPresent& gpu() { return gpu_; }
  const Scene3dGpuPresent& gpu() const { return gpu_; }
  Scene3dSoftwarePainter& software() { return software_; }
  const Scene3dSoftwarePainter& software() const { return software_; }

  bool hosts_scenic_present() const;

  void set_look_preset(Scene3dLookPreset preset);
  Scene3dLookPreset look_preset() const { return gpu_.look_preset(); }
  // Out-of-line so app TUs do not compute gpu_ from a stale AtmosphereSession
  // size (3D tab AV in MapScene::feature_count).
  bool ensure_legacy_overlays();

  void bind_orbit(const OrbitFrame* orbit);
  void bind_map(const MapScene* scene);
  void bind_label_frame(const ViewFrame* frame);
  void bind_contents(MapContents* session, uint32_t view_id);
  void reset();
  void apply_draft(const tool::Draft& draft);

  Extent2 world_extent() const;
  bool hosts_shared_scene() const;
  void abandon_mesh();

  void set_overlay_pointcloud(const float* xyz_lon_lat_elev, int point_count,
                              const uint8_t* rgba);
  void clear_overlay_pointcloud();

  void set_overlay_tin_mesh(const float* xyz_lon_lat_elev, int point_count,
                            const unsigned* indices, int index_count,
                            const uint8_t* albedo_rgba = nullptr);
  // Optional RGBA8 atlas + per-vertex UV (2 floats / vert) for lithology drapes.
  void set_overlay_tin_drape(const uint8_t* rgba, uint32_t width, uint32_t height,
                             const float* uv, int uv_float_count);
  void clear_overlay_tin_mesh();
  void set_dem_drape_rgba(const uint8_t* rgba, uint32_t width, uint32_t height);
  void clear_dem_drape();

  render::rhi::CameraMatrices camera_matrices(float aspect) const;
  render::rhi::CameraMatrices camera_matrices_ortho(float width_px,
                                                    float height_px) const;

  void paint(HDC hdc, int width_px, int height_px,
             bool fill_background = true) const;
  void paint_hud(HDC hdc, int width_px, int height_px) const;
  void paint_legacy_place_labels(HDC hdc, int width_px, int height_px) const;

  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px,
                   const ui::gfx::ShellRaster* shell = nullptr,
                   uint64_t shell_generation = 0);

  // Scenic GDI export (UTF-8 path). Used when HWND PrintWindow is black.
  bool export_bmp(const std::string& path, int width_px, int height_px) const;

  const char* render_engine_name() const;
  void set_render_engine_name(const char* name) const;

 private:
  void rebind_software();

  AtmosphereSession atmosphere_;
  Scene3dGpuPresent gpu_;
  Scene3dSoftwarePainter software_;
  mutable detail::ScenicScene3dHost scenic_host_;

  const ViewFrame* label_frame_ = nullptr;
  MapContents* contents_ = nullptr;
  const MapScene* map_scene_ = nullptr;
  uint32_t view_id_ = 0;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_PRESENTER_H_
