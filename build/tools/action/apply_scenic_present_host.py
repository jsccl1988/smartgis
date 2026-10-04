# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Rewrite scenic-hosted presenters + BUILD.gn deps; set files read-only."""

from pathlib import Path
import os
import stat

ROOT = Path(r"C:/Dev/src/gis/smartgis")

MAP_H = ROOT / "src/content/browser/present/map2d/map2d_presenter.h"
MAP_CC = ROOT / "src/content/browser/present/map2d/map2d_presenter.cc"
S3_H = ROOT / "src/content/browser/present/scene3d/scene3d_presenter.h"
S3_CC = ROOT / "src/content/browser/present/scene3d/scene3d_presenter.cc"
BUILD = ROOT / "src/content/BUILD.gn"
MAP_TEST = ROOT / "src/content/browser/present/map2d/map2d_presenter_test.cc"
S3_TEST = ROOT / "src/content/browser/present/scene3d/scene3d_presenter_test.cc"
CAPTURE = ROOT / "src/app/views/shell/harness/common/capture/scene3d_capture.cc"

FILES = [MAP_H, MAP_CC, S3_H, S3_CC, BUILD, MAP_TEST, S3_TEST, CAPTURE]


def writable(p: Path) -> None:
    if p.exists():
        os.chmod(p, stat.S_IWRITE | stat.S_IREAD)


def readonly(p: Path) -> None:
    os.chmod(p, stat.S_IREAD)


MAP_H_TEXT = r"""// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_
#define CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "content/browser/present/map2d/gpu/map2d_gpu_present.h"
#include "content/browser/present/map2d/software/map2d_software_painter.h"
#include "content/content_export.h"
#include "scenic/engine.h"
#include "ui/gfx/raster/shell_raster.h"

namespace render {
namespace rhi {
class Device;
}
}  // namespace render

namespace content {

class MapScene;
class ViewFrame;

CONTENT_EXPORT bool prefer_map2d_scenic();

class Map2dPresenter {
 public:
  Map2dPresenter();
  ~Map2dPresenter();

  Map2dPresenter(const Map2dPresenter&) = delete;
  Map2dPresenter& operator=(const Map2dPresenter&) = delete;

  void bind(const MapScene* scene, const ViewFrame* frame);

  Map2dFrameCache& frame_cache() { return cache_; }
  const Map2dFrameCache& frame_cache() const { return cache_; }
  Map2dGpuPresent& gpu() { return gpu_; }
  const Map2dGpuPresent& gpu() const { return gpu_; }
  Map2dSoftwarePainter& software() { return software_; }
  const Map2dSoftwarePainter& software() const { return software_; }

  bool hosts_scenic_present() const;

  void invalidate_frame_cache();

  bool present_gpu(render::rhi::Device* device, uint32_t width_px,
                   uint32_t height_px,
                   const ui::gfx::ShellRaster* shell = nullptr,
                   uint64_t shell_generation = 0);
  bool last_gpu_present_ok() const;
  bool last_gpu_present_drew() const;
  void note_surface_reset();
  uint64_t layout_build_count() const;
  bool last_present_reused_layout() const;

  void paint(HDC hdc, int width_px, int height_px) const;
  void paint(HDC hdc, int width_px, int height_px, bool fill_background) const;
  void paint_annotation_overlay(HDC hdc, int width_px, int height_px) const;
  void paint_flash_overlay(HDC hdc, int width_px, int height_px) const;

  bool export_bmp(const std::string& path, int width_px, int height_px) const;
  size_t basemap_tiles_drawn() const;

 private:
  void ensure_scenic() const;
  void sync_scenic(uint32_t width_px, uint32_t height_px) const;

  Map2dFrameCache cache_;
  Map2dGpuPresent gpu_;
  Map2dSoftwarePainter software_;

  const MapScene* scene_ = nullptr;
  const ViewFrame* frame_ = nullptr;
  mutable std::unique_ptr<scenic::Engine> scenic_;
  mutable std::vector<scenic::Vertex2> scenic_xy_;
  mutable std::vector<scenic::DrawItem> scenic_items_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_MAP2D_PRESENTER_H_
"""

MAP_CC_TEXT = r"""// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_presenter.h"

#include "base/trace/event/process_trace.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/present/host/scenic_scene_bind.h"

#include <cstdlib>
#include <cstring>

namespace content {

bool prefer_map2d_scenic() {
  const char* raw = std::getenv("SMT_MAP2D_ENGINE");
  return raw && raw[0] && _stricmp(raw, "scenic") == 0;
}

Map2dPresenter::Map2dPresenter() {
  ensure_scenic();
}

Map2dPresenter::~Map2dPresenter() = default;

void Map2dPresenter::ensure_scenic() const {
  if (!scenic_ && prefer_map2d_scenic()) {
    scenic_.reset(scenic::create_map2d_engine());
  }
}

bool Map2dPresenter::hosts_scenic_present() const {
  ensure_scenic();
  return scenic_ != nullptr;
}

void Map2dPresenter::bind(const MapScene* scene, const ViewFrame* frame) {
  scene_ = scene;
  frame_ = frame;
  cache_.bind(scene, frame);
  gpu_.bind(scene, frame, &cache_);
  software_.bind(scene, frame, &cache_);
}

void Map2dPresenter::invalidate_frame_cache() {
  gpu_.invalidate_frame_cache();
  software_.invalidate_present_cache();
}

void Map2dPresenter::sync_scenic(uint32_t width_px, uint32_t height_px) const {
  ensure_scenic();
  if (!scenic_) {
    return;
  }
  scenic::SessionDesc desc;
  desc.width_px = width_px;
  desc.height_px = height_px;
  scenic_->initialize(desc);
  const double scale = frame_ ? frame_->scale() : 1.0;
  detail::fill_scenic_draw_items(scene_, scale, &scenic_xy_, &scenic_items_);
  scenic_->bind_view(detail::scenic_view_from_frame(frame_));
  scenic_->bind_draw_items(scenic_items_.data(),
                           static_cast<uint32_t>(scenic_items_.size()));
}

bool Map2dPresenter::present_gpu(render::rhi::Device* device, uint32_t width_px,
                                 uint32_t height_px,
                                 const ui::gfx::ShellRaster* shell,
                                 uint64_t shell_generation) {
  ensure_scenic();
  if (scenic_) {
    (void)device;
    (void)shell;
    (void)shell_generation;
    sync_scenic(width_px, height_px);
    return scenic_->present();
  }
  return gpu_.present(device, width_px, height_px, shell, shell_generation);
}

bool Map2dPresenter::last_gpu_present_ok() const {
  return scenic_ ? scenic_->last_present_ok() : gpu_.last_present_ok();
}

bool Map2dPresenter::last_gpu_present_drew() const {
  return scenic_ ? scenic_->last_present_ok() : gpu_.last_present_drew();
}

void Map2dPresenter::note_surface_reset() {
  if (!scenic_) {
    gpu_.note_surface_reset();
  }
}

uint64_t Map2dPresenter::layout_build_count() const {
  return scenic_ ? 0 : cache_.layout_build_count();
}

bool Map2dPresenter::last_present_reused_layout() const {
  return scenic_ ? false : cache_.last_present_reused_layout();
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px) const {
  paint(hdc, width_px, height_px, true);
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px,
                           bool fill_background) const {
  ensure_scenic();
  if (scenic_) {
    (void)fill_background;
    sync_scenic(static_cast<uint32_t>(width_px),
                static_cast<uint32_t>(height_px));
    scenic_->paint_hdc(hdc, static_cast<uint32_t>(width_px),
                       static_cast<uint32_t>(height_px));
    return;
  }
  software_.paint(hdc, width_px, height_px, fill_background);
}

void Map2dPresenter::paint_annotation_overlay(HDC hdc, int width_px,
                                              int height_px) const {
  if (!scenic_) {
    software_.paint_annotation_overlay(hdc, width_px, height_px);
  }
}

void Map2dPresenter::paint_flash_overlay(HDC hdc, int width_px,
                                         int height_px) const {
  if (!scenic_) {
    software_.paint_flash_overlay(hdc, width_px, height_px);
  }
}

bool Map2dPresenter::export_bmp(const std::string& path, int width_px,
                                int height_px) const {
  ensure_scenic();
  if (scenic_) {
    sync_scenic(static_cast<uint32_t>(width_px),
                static_cast<uint32_t>(height_px));
    return scenic_->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                               static_cast<uint32_t>(height_px));
  }
  return software_.export_bmp(path, width_px, height_px);
}

size_t Map2dPresenter::basemap_tiles_drawn() const {
  return scenic_ ? 0 : software_.basemap_tiles_drawn();
}

}  // namespace content
"""

S3_H_TEXT = r"""// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_PRESENTER_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_PRESENTER_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <windows.h>

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"
#include "content/browser/present/scene3d/software/scene3d_software_painter.h"
#include "content/public/map_types.h"
#include "render/rhi/rhi.h"
#include "scenic/engine.h"
#include "tool/draft/draft.h"
#include "ui/gfx/raster/shell_raster.h"

namespace content {

class MapContents;
class MapScene;
class ViewFrame;

// Thin 3D present facade: Atmosphere + GPU/software, or hosted scenic::Engine.
class Scene3dPresenter {
 public:
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
  void clear_overlay_tin_mesh();

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
  void ensure_scenic() const;
  void sync_scenic(uint32_t width_px, uint32_t height_px) const;

  AtmosphereSession atmosphere_;
  Scene3dGpuPresent gpu_;
  Scene3dSoftwarePainter software_;

  const ViewFrame* label_frame_ = nullptr;
  MapContents* contents_ = nullptr;
  const MapScene* map_scene_ = nullptr;
  uint32_t view_id_ = 0;

  mutable std::unique_ptr<scenic::Engine> scenic_;
  mutable std::vector<scenic::Vertex2> scenic_xy_;
  mutable std::vector<scenic::DrawItem> scenic_items_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_PRESENTER_H_
"""

S3_CC_TEXT = r"""// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/scene3d_presenter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/host/scenic_scene_bind.h"
#include "content/browser/present/scene3d/session/scene3d_rhi_session.h"
#include "content/public/map_contents.h"
#include "base/trace/event/process_trace.h"

#include <cstdlib>

namespace content {

Scene3dPresenter::Scene3dPresenter() {
  if (const char* env = std::getenv("SMT_SCENE3D_WIREFRAME")) {
    if (env[0] == '1' && env[1] == '\0') {
      gpu_.set_wireframe_enabled(true);
    }
  }
  atmosphere_.bind_gpu(&gpu_);
  rebind_software();
  ensure_scenic();
}

bool Scene3dPresenter::hosts_scenic_present() const {
  ensure_scenic();
  return scenic_ != nullptr;
}

void Scene3dPresenter::ensure_scenic() const {
  if (!scenic_ && prefer_scene3d_scenic()) {
    scenic_.reset(scenic::create_scene3d_engine());
    gpu_.render_engine_name = "scenic";
  }
}

void Scene3dPresenter::set_look_preset(Scene3dLookPreset preset) {
  gpu_.set_look_preset(preset);
}

Scene3dPresenter::~Scene3dPresenter() {
  if (scenic_) {
    scenic_->shutdown();
  }
  software_.release_engine_logo_overlay();
  atmosphere_.release_passes();
  gpu_.abandon(&atmosphere_);
}

void Scene3dPresenter::rebind_software() {
  software_.bind(&gpu_, &atmosphere_, gpu_.orbit(), atmosphere_.scene(),
                 label_frame_);
  software_.set_hosts_shared_scene(hosts_shared_scene());
}

void Scene3dPresenter::bind_orbit(const OrbitFrame* orbit) {
  gpu_.bind_orbit(orbit);
  rebind_software();
}

void Scene3dPresenter::bind_label_frame(const ViewFrame* frame) {
  label_frame_ = frame;
  rebind_software();
}

void Scene3dPresenter::bind_map(const MapScene* scene) {
  map_scene_ = scene;
  atmosphere_.bind_scene(scene);
  gpu_.bind_map(scene);
  rebind_software();
}

void Scene3dPresenter::bind_contents(MapContents* session, uint32_t view_id) {
  contents_ = session;
  view_id_ = view_id;
  software_.set_hosts_shared_scene(hosts_shared_scene());
}

bool Scene3dPresenter::hosts_shared_scene() const {
  return contents_ != nullptr && view_id_ != 0;
}

Extent2 Scene3dPresenter::world_extent() const {
  return gpu_.world_extent();
}

void Scene3dPresenter::abandon_mesh() {
  gpu_.abandon(&atmosphere_);
}

void Scene3dPresenter::set_overlay_pointcloud(const float* xyz_lon_lat_elev,
                                             int point_count,
                                             const uint8_t* rgba) {
  gpu_.set_overlay_pointcloud(xyz_lon_lat_elev, point_count, rgba);
}

void Scene3dPresenter::clear_overlay_pointcloud() {
  gpu_.clear_overlay_pointcloud();
}

void Scene3dPresenter::set_overlay_tin_mesh(const float* xyz_lon_lat_elev,
                                           int point_count,
                                           const unsigned* indices,
                                           int index_count,
                                           const uint8_t* albedo_rgba) {
  gpu_.set_overlay_tin_mesh(xyz_lon_lat_elev, point_count, indices,
                            index_count, albedo_rgba);
}

void Scene3dPresenter::clear_overlay_tin_mesh() {
  gpu_.clear_overlay_tin_mesh();
}

void Scene3dPresenter::reset() {
  if (gpu_.orbit()) {
    const_cast<OrbitFrame*>(gpu_.orbit())->reset();
  }
}

void Scene3dPresenter::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kKey) {
    uint32_t k = draft.key;
    if (k >= 'a' && k <= 'z') {
      k = k - ('a' - 'A');
    }
    if (k == 'K') {
      gpu_.set_wireframe_enabled(!gpu_.wireframe_enabled());
      return;
    }
    if (k == 'J') {
      gpu_.set_wireframe_enabled(false);
      return;
    }
  }
  if (gpu_.orbit()) {
    const_cast<OrbitFrame*>(gpu_.orbit())->apply_draft(draft);
  }
}

render::rhi::CameraMatrices Scene3dPresenter::camera_matrices(
    float aspect) const {
  return gpu_.camera_matrices(aspect);
}

render::rhi::CameraMatrices Scene3dPresenter::camera_matrices_ortho(
    float width_px, float height_px) const {
  return gpu_.camera_matrices_ortho(width_px, height_px);
}

void Scene3dPresenter::set_render_engine_name(const char* name) const {
  gpu_.render_engine_name = (name && name[0]) ? name : "unknown";
}

const char* Scene3dPresenter::render_engine_name() const {
  return gpu_.render_engine_name;
}

void Scene3dPresenter::sync_scenic(uint32_t width_px, uint32_t height_px) const {
  ensure_scenic();
  if (!scenic_) {
    return;
  }
  scenic::SessionDesc desc;
  desc.width_px = width_px;
  desc.height_px = height_px;
  scenic_->initialize(desc);
  scenic_->bind_orbit(
      detail::scenic_orbit_from_host(gpu_.orbit(), map_scene_));
  // Scene3d scenic GDI proof: sky/ocean/globe only (no MapScene fan-out).
  scenic_xy_.clear();
  scenic_items_.clear();
  scenic_->bind_draw_items(nullptr, 0);
}

bool Scene3dPresenter::present_gpu(render::rhi::Device* device,
                                  uint32_t width_px, uint32_t height_px,
                                  const ui::gfx::ShellRaster* shell,
                                  uint64_t shell_generation) {
  BASE_TRACE_EVENT("presenter", "scene3d");
  ensure_scenic();
  if (scenic_) {
    (void)device;
    (void)shell;
    (void)shell_generation;
    sync_scenic(width_px, height_px);
    return scenic_->present();
  }
  return gpu_.present(device, width_px, height_px, shell, shell_generation,
                      atmosphere_);
}

void Scene3dPresenter::paint(HDC hdc, int width_px, int height_px,
                            bool fill_background) const {
  BASE_TRACE_EVENT("presenter_paint", "scene3d");
  ensure_scenic();
  if (scenic_) {
    (void)fill_background;
    sync_scenic(static_cast<uint32_t>(width_px),
                static_cast<uint32_t>(height_px));
    scenic_->paint_hdc(hdc, static_cast<uint32_t>(width_px),
                       static_cast<uint32_t>(height_px));
    return;
  }
  software_.paint(hdc, width_px, height_px, fill_background);
}

bool Scene3dPresenter::export_bmp(const std::string& path, int width_px,
                                  int height_px) const {
  ensure_scenic();
  if (!scenic_ || path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  sync_scenic(static_cast<uint32_t>(width_px), static_cast<uint32_t>(height_px));
  return scenic_->export_bmp(path.c_str(), static_cast<uint32_t>(width_px),
                             static_cast<uint32_t>(height_px));
}

void Scene3dPresenter::paint_hud(HDC hdc, int width_px, int height_px) const {
  BASE_TRACE_EVENT("hud", "scene3d");
  if (!scenic_) {
    software_.paint_hud(hdc, width_px, height_px);
  }
}

void Scene3dPresenter::paint_legacy_place_labels(HDC hdc, int width_px,
                                                int height_px) const {
  if (!scenic_) {
    software_.paint_legacy_place_labels(hdc, width_px, height_px);
  }
}

}  // namespace content
"""


def patch_build_gn(text: str) -> str:
    import re

    text = re.sub(
        r"[ \t]*#[^\n]*scenic[^\n]*\n[ \t]*assert_no_deps = \[ \"//src/scenic:\*\" \]\n",
        "",
        text,
    )
    text = re.sub(
        r"[ \t]*assert_no_deps = \[ \"//src/scenic:\*\" \]\n",
        "",
        text,
    )
    if "scenic_scene_bind.cc" not in text:
        text = text.replace(
            '    "browser/present/host/blit_frame_cache.h",\n',
            '    "browser/present/host/blit_frame_cache.h",\n'
            '    "browser/present/host/scenic_scene_bind.cc",\n'
            '    "browser/present/host/scenic_scene_bind.h",\n',
        )
    if text.count("//src/scenic:scenic") < 2:
        # map_present deps
        needle = '    "//src/render/graph:graph",\n    "//src/ui/gfx:gfx_headers",\n    "//third_party:gdal",\n'
        repl = (
            '    "//src/render/graph:graph",\n'
            '    "//src/scenic:scenic",\n'
            '    "//src/ui/gfx:gfx_headers",\n'
            '    "//third_party:gdal",\n'
        )
        if "//src/scenic:scenic" not in text.split("source_set(\"map_present\")")[1].split("source_set(\"scene3d_present\")")[0]:
            text = text.replace(needle, repl, 1)
        # scene3d_present deps
        needle2 = (
            '    "//src/render/graph:graph",\n'
            '    "//src/tool:dispatch",\n'
            '    "//src/ui/gfx:gfx_headers",\n'
        )
        repl2 = (
            '    "//src/render/graph:graph",\n'
            '    "//src/scenic:scenic",\n'
            '    "//src/tool:dispatch",\n'
            '    "//src/ui/gfx:gfx_headers",\n'
        )
        if needle2 in text and "//src/scenic:scenic" not in text.split("source_set(\"scene3d_present\")")[1][:1200]:
            text = text.replace(needle2, repl2, 1)
    return text


def patch_tests() -> None:
    mt = MAP_TEST.read_text(encoding="utf-8", errors="replace")
    mt = mt.replace(
        'expect(!on.hosts_scenic_present(),\n'
        '           "map2d env scenic does not load scenic.dll");',
        'expect(on.hosts_scenic_present(),\n'
        '           "map2d env scenic hosts scenic.dll");',
    )
    mt = mt.replace(
        'expect(!on.hosts_scenic_present(),\n'
        '           "map2d env scenic does not load scenic.dll");',
        'expect(on.hosts_scenic_present(),\n'
        '           "map2d env scenic hosts scenic.dll");',
    )
    MAP_TEST.write_text(mt, encoding="utf-8", newline="\n")

    st = S3_TEST.read_text(encoding="utf-8", errors="replace")
    st = st.replace(
        'expect(!cam.hosts_scenic_present(),\n'
        '             "Scenic env/API does not load scenic.dll");',
        'expect(cam.hosts_scenic_present(),\n'
        '             "Scenic env/API hosts scenic.dll");',
    )
    S3_TEST.write_text(st, encoding="utf-8", newline="\n")


def patch_capture() -> None:
    text = CAPTURE.read_text(encoding="utf-8")
    if "export_bmp" in text and "hosts_scenic_present()" in text and "WideCharToMultiByte" in text:
        return
    old = """  (void)cam->present_gpu(device, opts.present_w, opts.present_h);
  if (cam->hosts_scenic_present() && capture_hwnd && IsWindow(capture_hwnd)) {
    HDC dc = GetDC(capture_hwnd);
    if (dc) {
      cam->paint(dc, static_cast<int>(opts.present_w),
                 static_cast<int>(opts.present_h), true);
      ReleaseDC(capture_hwnd, dc);
    }
  }
  wait_before_capture(capture_hwnd, opts.pre_capture_pump_ms,
                      opts.sleep_instead_of_pump);

  const CaptureOpts capture = make_capture_opts(opts);
  if (!capture_hwnd_bmp(capture_hwnd, bmp_path, capture)) {
"""
    new = """  (void)cam->present_gpu(device, opts.present_w, opts.present_h);
  // Scenic GDI does not stick in the present HWND (no WM_PAINT owner-draw).
  // Export via scenic::Engine memory DIB instead of PrintWindow.
  if (cam->hosts_scenic_present()) {
    char utf8[MAX_PATH * 3] = {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, bmp_path, -1, utf8,
                                      static_cast<int>(sizeof(utf8)), nullptr,
                                      nullptr);
    if (n > 0 &&
        cam->export_bmp(utf8, static_cast<int>(opts.present_w),
                        static_cast<int>(opts.present_h))) {
      int bw = 0;
      int bh = 0;
      BmpFileCheckOpts check;
      check.require_color_diversity = opts.require_color_diversity;
      const bool signal =
          bmp_file_has_visible_signal(bmp_path, &bw, &bh, check);
      std::fwprintf(stderr, L"%S: wrote %ls (%dx%d signal=%d scenic_export=1)\\n",
                    opts.log_prefix ? opts.log_prefix : "scene3d-capture",
                    bmp_path, bw, bh, signal ? 1 : 0);
      if (signal) {
        mark_step(opts.mark, opts.mark_ok);
        if (opts.after_ok) {
          (void)opts.after_ok(bmp_path, bw, bh, opts.after_ok_user);
        }
        return true;
      }
      mark_step(opts.mark, opts.mark_black);
      return !want_gpu;
    }
  }
  if (cam->hosts_scenic_present() && capture_hwnd && IsWindow(capture_hwnd)) {
    HDC dc = GetDC(capture_hwnd);
    if (dc) {
      cam->paint(dc, static_cast<int>(opts.present_w),
                 static_cast<int>(opts.present_h), true);
      ReleaseDC(capture_hwnd, dc);
    }
  }
  wait_before_capture(capture_hwnd, opts.pre_capture_pump_ms,
                      opts.sleep_instead_of_pump);

  const CaptureOpts capture = make_capture_opts(opts);
  if (!capture_hwnd_bmp(capture_hwnd, bmp_path, capture)) {
"""
    if old not in text:
        raise SystemExit("capture block not found")
    CAPTURE.write_text(text.replace(old, new), encoding="utf-8", newline="\n")


def main() -> None:
    for p in FILES:
        writable(p)

    MAP_H.write_text(MAP_H_TEXT, encoding="utf-8", newline="\n")
    MAP_CC.write_text(MAP_CC_TEXT, encoding="utf-8", newline="\n")
    S3_H.write_text(S3_H_TEXT, encoding="utf-8", newline="\n")
    S3_CC.write_text(S3_CC_TEXT, encoding="utf-8", newline="\n")

    build = BUILD.read_text(encoding="utf-8")
    BUILD.write_text(patch_build_gn(build), encoding="utf-8", newline="\n")
    patch_tests()
    patch_capture()

    for p in (MAP_H, MAP_CC, S3_H, S3_CC):
        readonly(p)

    assert "create_map2d_engine" in MAP_CC.read_text(encoding="utf-8")
    assert "create_scene3d_engine" in S3_CC.read_text(encoding="utf-8")
    assert "export_bmp" in S3_H.read_text(encoding="utf-8")
    assert "scenic_export=1" in CAPTURE.read_text(encoding="utf-8")
    print("apply_scenic_present: ok (presenters read-only)")


if __name__ == "__main__":
    main()
