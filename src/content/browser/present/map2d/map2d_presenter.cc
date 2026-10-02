// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/map2d_presenter.h"

#include "base/trace/event/process_trace.h"

namespace content {

Map2dPresenter::Map2dPresenter() = default;
Map2dPresenter::~Map2dPresenter() = default;

void Map2dPresenter::bind(const MapScene* scene, const ViewFrame* frame) {
  cache_.bind(scene, frame);
  gpu_.bind(scene, frame, &cache_);
  software_.bind(scene, frame, &cache_);
}

void Map2dPresenter::invalidate_frame_cache() {
  gpu_.invalidate_frame_cache();
  software_.invalidate_present_cache();
}

bool Map2dPresenter::present_gpu(render::rhi::Device* device, uint32_t width_px,
                                 uint32_t height_px,
                                 const ui::gfx::ShellRaster* shell,
                                 uint64_t shell_generation) {
  BASE_TRACE_EVENT("presenter", "map2d");
  return gpu_.present(device, width_px, height_px, shell, shell_generation);
}

bool Map2dPresenter::last_gpu_present_ok() const {
  return gpu_.last_present_ok();
}

bool Map2dPresenter::last_gpu_present_drew() const {
  return gpu_.last_present_drew();
}

void Map2dPresenter::note_surface_reset() {
  gpu_.note_surface_reset();
}

uint64_t Map2dPresenter::layout_build_count() const {
  return cache_.layout_build_count();
}

bool Map2dPresenter::last_present_reused_layout() const {
  return cache_.last_present_reused_layout();
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px) const {
  software_.paint(hdc, width_px, height_px);
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px,
                           bool fill_background) const {
  BASE_TRACE_EVENT("presenter_paint", "map2d");
  software_.paint(hdc, width_px, height_px, fill_background);
}

void Map2dPresenter::paint_annotation_overlay(HDC hdc, int width_px,
                                              int height_px) const {
  software_.paint_annotation_overlay(hdc, width_px, height_px);
}

void Map2dPresenter::paint_flash_overlay(HDC hdc, int width_px,
                                         int height_px) const {
  software_.paint_flash_overlay(hdc, width_px, height_px);
}

void Map2dPresenter::paint_labels_projected(
    HDC hdc, int width_px, int height_px,
    const std::function<void(double lon, double lat, int* sx, int* sy)>&
        project) const {
  software_.paint_labels_projected(hdc, width_px, height_px, project);
}

bool Map2dPresenter::export_bmp(const std::string& path, int width_px,
                                int height_px) const {
  return software_.export_bmp(path, width_px, height_px);
}

size_t Map2dPresenter::basemap_tiles_drawn() const {
  return software_.basemap_tiles_drawn();
}

}  // namespace content
