// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/map2d/map2d_presenter.h"

#include "app/views/camera/view_frame.h"
#include "app/views/document/map_scene.h"
#include "app/views/present/map2d/frame/map2d_batches.h"
#include "app/views/present/map2d/frame/map2d_tile_math.h"

#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include "effect/map/map_effect.h"
#include "effect/map/pass.h"
#include "gis/present/style/style_document.h"
#include "gis/vista/frame/frame.h"
#include "render/graph/frame_graph.h"
#include "render/rhi/rhi.h"

namespace app {
namespace {

bool write_bmp_file(const std::string& path, int width_px, int height_px,
                    const void* bits, int stride_bytes) {
  if (path.empty() || !bits || width_px <= 0 || height_px <= 0 ||
      stride_bytes <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride_bytes) * static_cast<DWORD>(height_px);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;  // 'BM'
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width_px;
  bih.biHeight = height_px;  // bottom-up
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
    return false;
  }
  const bool ok =
      std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
      std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih) &&
      std::fwrite(bits, 1, image_bytes, f) == image_bytes;
  std::fclose(f);
  return ok;
}

}  // namespace

Map2dPresenter::Map2dPresenter() = default;
Map2dPresenter::~Map2dPresenter() = default;

void Map2dPresenter::bind(const MapScene* scene, const ViewFrame* frame) {
  scene_ = scene;
  frame_ = frame;
  invalidate_frame_cache();
}

void Map2dPresenter::invalidate_frame_cache() {
  has_frame_cache_ = false;
  last_present_was_interactive_ = false;
  last_present_reused_layout_ = false;
  cached_frame_ = gis::vista::MapFrame{};
  cached_fp_ = ContentFingerprint{};
  cached_cam_ = CameraKey{};
}

Map2dPresenter::ContentFingerprint Map2dPresenter::make_fingerprint() const {
  ContentFingerprint fp;
  if (!scene_) {
    return fp;
  }
  fp.feature_count = scene_->feature_count();
  fp.layer_count = scene_->layer_count();
  fp.style_ptr = scene_->style_document();
  fp.use_carto = scene_->style_document() == nullptr;
  return fp;
}

Map2dPresenter::CameraKey Map2dPresenter::make_camera_key(
    uint32_t width_px, uint32_t height_px) const {
  CameraKey key;
  key.width_px = width_px;
  key.height_px = height_px;
  if (!frame_) {
    return key;
  }
  const content::Extent2 extent = frame_->view_world_extent(
      static_cast<int>(width_px), static_cast<int>(height_px));
  key.min_x = extent.xmin;
  key.min_y = extent.ymin;
  key.max_x = extent.xmax;
  key.max_y = extent.ymax;
  key.scale = frame_->scale();
  const double zoom = detail::zoom_from_scale(frame_->scale());
  key.zoom_bucket = static_cast<int>(std::floor(zoom));
  return key;
}

bool Map2dPresenter::rebuild_layout(const CameraKey& cam,
                                    const ContentFingerprint& fp) {
  if (!scene_ || !frame_ || cam.width_px == 0 || cam.height_px == 0) {
    return false;
  }

  gis::style::StyleDocument parsed_style;
  const gis::style::StyleDocument* style = scene_->style_document();
  if (!style) {
    gis::style::parse_style_document(gis::vista::default_carto_style_json(),
                                     &parsed_style);
  }

  gis::vista::Layout layout;
  gis::vista::LayoutInput in;
  in.view = {cam.width_px, cam.height_px, cam.min_x, cam.min_y, cam.max_x,
             cam.max_y};
  in.style = style ? style : &parsed_style;
  in.zoom = detail::zoom_from_scale(frame_->scale());
  effect::map::WindowsGlyphRasterizer windows_rasterizer;
  in.metrics = &windows_rasterizer;
  in.tiles = {};

  const detail::Map2dBatches layer_batches =
      detail::visible_layer_batches(scene_->layers(), style == nullptr);
  cached_frame_ = layout.build(in, layer_batches.batches);
  cached_fp_ = fp;
  cached_cam_ = cam;
  has_frame_cache_ = true;
  ++layout_build_count_;
  return true;
}

bool Map2dPresenter::present_frame(render::rhi::Device* device,
                                   const CameraKey& cam, bool record_all,
                                   const ui::gfx::ShellRaster* shell,
                                   uint64_t shell_generation) {
  if (!device || !has_frame_cache_) {
    return false;
  }
  if (!map2d_pass_) {
    map2d_pass_ = std::make_unique<effect::map::Pass>();
  }

  effect::map::WindowsGlyphRasterizer windows_rasterizer;
  gis::vista::View view{cam.width_px, cam.height_px, cam.min_x, cam.min_y,
                        cam.max_x,    cam.max_y};
  const render::rhi::CameraMatrices camera = render::rhi::make_ortho_camera(
      static_cast<float>(cam.min_x), static_cast<float>(cam.max_x),
      static_cast<float>(cam.min_y), static_cast<float>(cam.max_y), -1.f, 1.f);

  // Interactive reuse presents world meshes only so stale pixel-space labels
  // do not stick to the old screen positions. GDI overlay can still label.
  effect::map::MapEffect map_effect(
      render::graph::EffectSlot::kOpaque, map2d_pass_.get(), &cached_frame_,
      &view, &windows_rasterizer, {}, {}, record_all);
  if (shell && shell->bgra && shell->width_px != 0 && shell->height_px != 0) {
    shell_overlay_.bind(*shell, shell_generation);
  } else {
    shell_overlay_.clear();
  }
  render::graph::ViewInput view_input;
  view_input.width_px = cam.width_px;
  view_input.height_px = cam.height_px;
  view_input.camera = &camera;
  view_input.effects.push_back(&map_effect);
  if (shell_overlay_.has_shell()) {
    view_input.effects.push_back(&shell_overlay_);
  }
  return render::graph::present(device, view_input);
}

bool Map2dPresenter::export_bmp(const std::string& path, int width_px,
                                int height_px) const {
  if (path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  HDC screen = GetDC(nullptr);
  if (!screen) {
    return false;
  }
  HDC mem = CreateCompatibleDC(screen);
  if (!mem) {
    ReleaseDC(nullptr, screen);
    return false;
  }
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = height_px;  // bottom-up for BMP write
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(mem, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    DeleteDC(mem);
    ReleaseDC(nullptr, screen);
    return false;
  }
  HGDIOBJ old = SelectObject(mem, dib);
  paint(mem, width_px, height_px, true);
  const int stride = ((width_px * 32 + 31) / 32) * 4;
  const bool ok = write_bmp_file(path, width_px, height_px, bits, stride);
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return ok;
}

bool Map2dPresenter::present_gpu(render::rhi::Device* device, uint32_t width_px,
                                 uint32_t height_px,
                                 const ui::gfx::ShellRaster* shell,
                                 uint64_t shell_generation) {
  last_gpu_present_ok_ = false;
  last_present_reused_layout_ = false;
  if (!device || !scene_ || !frame_ || width_px == 0 || height_px == 0) {
    return false;
  }

  const ContentFingerprint fp = make_fingerprint();
  const CameraKey cam = make_camera_key(width_px, height_px);

  const bool content_dirty =
      !has_frame_cache_ || !(fp == cached_fp_) ||
      !cam.same_pixels(cached_cam_) ||
      cam.zoom_bucket != cached_cam_.zoom_bucket;
  const bool camera_changed =
      has_frame_cache_ && !cam.same_camera(cached_cam_);

  bool ok = false;
  if (content_dirty) {
    if (!rebuild_layout(cam, fp)) {
      return false;
    }
    ok = present_frame(device, cam, /*record_all=*/true, shell,
                       shell_generation);
    last_present_was_interactive_ = false;
    last_present_reused_layout_ = false;
  } else if (camera_changed) {
    // Interactive: reuse world meshes; skip Layout::build.
    cached_cam_ = cam;
    ok = present_frame(device, cam, /*record_all=*/false, shell,
                       shell_generation);
    last_present_was_interactive_ = true;
    last_present_reused_layout_ = true;
  } else if (last_present_was_interactive_) {
    // Just settled: rebuild so GPU labels match the final viewport.
    if (!rebuild_layout(cam, fp)) {
      return false;
    }
    ok = present_frame(device, cam, /*record_all=*/true, shell,
                       shell_generation);
    last_present_was_interactive_ = false;
    last_present_reused_layout_ = false;
  } else {
    // Static repeat: present cached full frame without rebuild.
    ok = present_frame(device, cam, /*record_all=*/true, shell,
                       shell_generation);
    last_present_was_interactive_ = false;
    last_present_reused_layout_ = true;
  }

  last_gpu_present_ok_ = ok;
  return ok;
}

}  // namespace app
