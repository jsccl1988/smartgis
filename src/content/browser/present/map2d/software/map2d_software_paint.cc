// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_software_painter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"
#include "content/browser/present/map2d/software/map2d_frame_gdi.h"
#include "content/browser/present/map2d/map2d_phase_profile.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "gis/tile/protocol/xyz_math.h"
#include "base/trace/event/process_trace.h"
#include "base/process/switches.h"
#include "tool/nav/camera_nav.h"

namespace content {
namespace {

using Feature = GisScene::Feature;
using Layer = GisScene::Layer;
using Vertex = GisScene::Vertex;
using GeomKind = GisScene::GeomKind;

// Writes a classic bottom-up BMP. |bits| is a top-down 32bpp buffer (row 0 =
// screen top), matching CreateDIBSection(biHeight=-H) used by map2d paint.
bool write_bmp_file(const std::string& path, int width_px, int height_px,
                    const void* bits, int stride_bytes) {
  if (path.empty() || !bits || width_px <= 0 || height_px <= 0 ||
      stride_bytes <= 0) {
    return false;
  }
  const DWORD image_bytes =
      static_cast<DWORD>(stride_bytes) * static_cast<DWORD>(height_px);
  BITMAPFILEHEADER bfh = {};
  bfh.bfType = 0x4D42;
  bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
  bfh.bfSize = bfh.bfOffBits + image_bytes;
  BITMAPINFOHEADER bih = {};
  bih.biSize = sizeof(BITMAPINFOHEADER);
  bih.biWidth = width_px;
  bih.biHeight = height_px;  // bottom-up file order
  bih.biPlanes = 1;
  bih.biBitCount = 32;
  bih.biCompression = BI_RGB;
  bih.biSizeImage = image_bytes;
  FILE* f = nullptr;
  if (fopen_s(&f, path.c_str(), "wb") != 0 || !f) {
    return false;
  }
  bool ok = std::fwrite(&bfh, 1, sizeof(bfh), f) == sizeof(bfh) &&
            std::fwrite(&bih, 1, sizeof(bih), f) == sizeof(bih);
  if (ok) {
    const auto* src = static_cast<const uint8_t*>(bits);
    for (int y = height_px - 1; y >= 0; --y) {
      if (std::fwrite(src + static_cast<size_t>(y) * static_cast<size_t>(stride_bytes),
                      1, static_cast<size_t>(stride_bytes), f) !=
          static_cast<size_t>(stride_bytes)) {
        ok = false;
        break;
      }
    }
  }
  std::fclose(f);
  return ok;
}

}  // namespace

Map2dSoftwarePainter::~Map2dSoftwarePainter() { clear_present_cache(); }

void Map2dSoftwarePainter::clear_present_cache() const {
  if (present_cache_dc_) {
    DeleteDC(present_cache_dc_);
    present_cache_dc_ = nullptr;
  }
  if (present_cache_bmp_) {
    DeleteObject(present_cache_bmp_);
    present_cache_bmp_ = nullptr;
  }
  present_cache_w_ = 0;
  present_cache_h_ = 0;
  present_cache_layout_gen_ = 0;
  present_cache_cam_ = Map2dFrameCache::CameraKey{};
}

bool Map2dSoftwarePainter::ensure_present_cache_dib(int width_px,
                                                    int height_px) const {
  if (width_px <= 0 || height_px <= 0) {
    return false;
  }
  if (present_cache_dc_ && present_cache_bmp_ && present_cache_w_ == width_px &&
      present_cache_h_ == height_px) {
    return true;
  }
  clear_present_cache();
  BITMAPINFO bmi = {};
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width_px;
  bmi.bmiHeader.biHeight = -height_px;
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;
  void* bits = nullptr;
  HBITMAP dib =
      CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!dib || !bits) {
    if (dib) {
      DeleteObject(dib);
    }
    return false;
  }
  HDC mem = CreateCompatibleDC(nullptr);
  if (!mem) {
    DeleteObject(dib);
    return false;
  }
  SelectObject(mem, dib);
  present_cache_dc_ = mem;
  present_cache_bmp_ = dib;
  present_cache_w_ = width_px;
  present_cache_h_ = height_px;
  return true;
}

bool Map2dSoftwarePainter::try_blit_present_cache(
    HDC hdc, int width_px, int height_px, uint64_t layout_gen) const {
  if (!hdc || !present_cache_dc_ || !present_cache_bmp_ ||
      present_cache_w_ != width_px || present_cache_h_ != height_px ||
      present_cache_layout_gen_ != layout_gen) {
    return false;
  }
  return BitBlt(hdc, 0, 0, width_px, height_px, present_cache_dc_, 0, 0,
                SRCCOPY) != FALSE;
}

bool Map2dSoftwarePainter::try_interactive_present_cache(
    HDC hdc, int width_px, int height_px, uint64_t layout_gen,
    const Map2dFrameCache::CameraKey& cam) const {
  if (!hdc || !present_cache_dc_ || !present_cache_bmp_ ||
      present_cache_w_ != width_px || present_cache_h_ != height_px ||
      present_cache_layout_gen_ != layout_gen ||
      !present_cache_cam_.same_pixels(cam) ||
      present_cache_cam_.zoom_bucket != cam.zoom_bucket) {
    return false;
  }
  const double old_span_x =
      present_cache_cam_.max_x - present_cache_cam_.min_x;
  const double old_span_y =
      present_cache_cam_.max_y - present_cache_cam_.min_y;
  const double new_span_x = cam.max_x - cam.min_x;
  const double new_span_y = cam.max_y - cam.min_y;
  if (!(old_span_x > 0.0) || !(old_span_y > 0.0) || !(new_span_x > 0.0) ||
      !(new_span_y > 0.0)) {
    return false;
  }

  // Ocean canvas under the shifted frame (matches BlitFrameCache::present).
  RECT full = {0, 0, width_px, height_px};
  HBRUSH bg = CreateSolidBrush(detail::rgba_to_colorref(0xFFAAD3DFu));
  FillRect(hdc, &full, bg);
  DeleteObject(bg);

  const double sx_old = static_cast<double>(width_px) / old_span_x;
  const double sy_old = static_cast<double>(height_px) / old_span_y;
  // Screen shift of a world point when the camera moves (ViewXform contract).
  const int dx = static_cast<int>(
      std::lround((present_cache_cam_.min_x - cam.min_x) * sx_old));
  const int dy = static_cast<int>(
      std::lround((cam.max_y - present_cache_cam_.max_y) * sy_old));

  const double zoom_x = old_span_x / new_span_x;
  const double zoom_y = old_span_y / new_span_y;
  const bool same_scale =
      std::fabs(zoom_x - 1.0) < 1e-4 && std::fabs(zoom_y - 1.0) < 1e-4 &&
      std::fabs(zoom_x - zoom_y) < 1e-4;
  if (same_scale) {
    const tool::BlitDestRect dest =
        tool::pan_blit_dest(width_px, height_px, dx, dy);
    SetStretchBltMode(hdc, COLORONCOLOR);
    return StretchBlt(hdc, dest.x, dest.y, dest.w, dest.h, present_cache_dc_, 0,
                      0, width_px, height_px, SRCCOPY) != FALSE;
  }

  // Zoom: scale the last frame about the view center (settle rebuild follows).
  const double factor = 0.5 * (zoom_x + zoom_y);
  if (!(factor > 0.0)) {
    return false;
  }
  const tool::BlitDestRect dest = tool::zoom_blit_dest(
      width_px, height_px, width_px / 2, height_px / 2, factor);
  SetStretchBltMode(hdc, HALFTONE);
  SetBrushOrgEx(hdc, 0, 0, nullptr);
  return StretchBlt(hdc, dest.x, dest.y, dest.w, dest.h, present_cache_dc_, 0,
                    0, width_px, height_px, SRCCOPY) != FALSE;
}

void Map2dSoftwarePainter::store_present_cache(
    HDC src, int width_px, int height_px, uint64_t layout_gen,
    const Map2dFrameCache::CameraKey& cam) const {
  if (!src || width_px <= 0 || height_px <= 0) {
    return;
  }
  if (!ensure_present_cache_dib(width_px, height_px)) {
    return;
  }
  if (!BitBlt(present_cache_dc_, 0, 0, width_px, height_px, src, 0, 0,
              SRCCOPY)) {
    return;
  }
  present_cache_layout_gen_ = layout_gen;
  present_cache_cam_ = cam;
}

void Map2dSoftwarePainter::bind(const GisScene* scene, const ViewFrame* frame,
                                 Map2dFrameCache* cache) {
  scene_ = scene;
  frame_ = frame;
  cache_ = cache;
  basemap_tiles_drawn_ = 0;
  clear_present_cache();
}

void Map2dSoftwarePainter::paint_basemap_underlay(HDC hdc, int width_px,
                                                   int height_px) const {
  gis::tile::TileProvider* basemap =
      scene_ ? scene_->basemap_provider() : nullptr;
  basemap_tiles_drawn_ = 0;
  if (!hdc || !scene_ || !frame_ || !basemap || !basemap->is_open() ||
      width_px <= 0 || height_px <= 0) {
    return;
  }
  const content::Extent2 world = frame_->view_world_extent(width_px, height_px);
  gis::tile::Viewport vp;
  vp.min_x = detail::lon_to_merc_x(world.xmin);
  vp.max_x = detail::lon_to_merc_x(world.xmax);
  vp.min_y = detail::lat_to_merc_y(world.ymin);
  vp.max_y = detail::lat_to_merc_y(world.ymax);
  if (vp.min_x > vp.max_x) {
    std::swap(vp.min_x, vp.max_x);
  }
  if (vp.min_y > vp.max_y) {
    std::swap(vp.min_y, vp.max_y);
  }
  const std::vector<gis::tile::TileImage> tiles = basemap->fetch_visible(vp, 2);
  basemap_tiles_drawn_ = tiles.size();
  for (const gis::tile::TileImage& tile : tiles) {
    const double lon0 = detail::merc_x_to_lon(tile.world_rect.MinX);
    const double lon1 = detail::merc_x_to_lon(tile.world_rect.MaxX);
    const double lat0 = detail::merc_y_to_lat(tile.world_rect.MinY);
    const double lat1 = detail::merc_y_to_lat(tile.world_rect.MaxY);
    int vx0 = 0;
    int vy0 = 0;
    int vx1 = 0;
    int vy1 = 0;
    frame_->map_to_view(lon0, -lat1, &vx0, &vy0);
    frame_->map_to_view(lon1, -lat0, &vx1, &vy1);
    if (vx0 > vx1) {
      std::swap(vx0, vx1);
    }
    if (vy0 > vy1) {
      std::swap(vy0, vy1);
    }
    if (vx1 < 0 || vy1 < 0 || vx0 > width_px || vy0 > height_px) {
      continue;
    }
    const int shade = 90 + ((tile.coord.x + tile.coord.y) & 7) * 12;
    HBRUSH brush =
        CreateSolidBrush(RGB(shade, 110 + (tile.coord.z % 5) * 8, 140));
    RECT rc = {vx0, vy0, vx1 + 1, vy1 + 1};
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
  }
}

void Map2dSoftwarePainter::paint_flash_overlay(HDC hdc, int width_px,
                                               int height_px) const {
  if (!hdc || !scene_ || !frame_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  const Feature* feature = scene_->selected_feature();
  if (!feature || feature->points.empty()) {
    return;
  }
  HPEN pen = CreatePen(PS_SOLID, 3, RGB(255, 220, 40));
  HGDIOBJ old_pen = SelectObject(hdc, pen ? pen : GetStockObject(BLACK_PEN));
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  std::vector<POINT> pts;
  pts.reserve(feature->points.size());
  for (const Vertex& p : feature->points) {
    int vx = 0;
    int vy = 0;
    frame_->map_to_view(p.x, p.y, &vx, &vy);
    if (vx < -64 || vy < -64 || vx > width_px + 64 || vy > height_px + 64) {
      continue;
    }
    pts.push_back(POINT{vx, vy});
  }
  if (pts.size() >= 2) {
    Polyline(hdc, pts.data(), static_cast<int>(pts.size()));
  } else if (pts.size() == 1) {
    Ellipse(hdc, pts[0].x - 6, pts[0].y - 6, pts[0].x + 6, pts[0].y + 6);
  }
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  if (pen) {
    DeleteObject(pen);
  }
}

void Map2dSoftwarePainter::paint_selection_overlay(HDC hdc, int width_px,
                                                   int height_px) const {
  if (!hdc || !scene_ || !frame_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  HPEN sel_pen = CreatePen(PS_SOLID, 2, RGB(200, 120, 40));
  HGDIOBJ old_pen =
      SelectObject(hdc, sel_pen ? sel_pen : GetStockObject(BLACK_PEN));
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));

  for (const Layer& layer : scene_->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (!f.selected || f.points.empty()) {
        continue;
      }
      std::vector<POINT> pts;
      pts.reserve(f.points.size());
      for (const Vertex& p : f.points) {
        int vx = 0;
        int vy = 0;
        frame_->map_to_view(p.x, p.y, &vx, &vy);
        pts.push_back(POINT{vx, vy});
      }
      if (f.kind == GeomKind::kPolygon && pts.size() >= 3) {
        Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
      } else if (f.kind == GeomKind::kLine && pts.size() >= 2) {
        Polyline(hdc, pts.data(), static_cast<int>(pts.size()));
      } else if (f.kind == GeomKind::kPoint || f.kind == GeomKind::kText) {
        Ellipse(hdc, pts.front().x - 5, pts.front().y - 5, pts.front().x + 5,
                pts.front().y + 5);
      }
    }
  }

  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  if (sel_pen) {
    DeleteObject(sel_pen);
  }
}

void Map2dSoftwarePainter::paint_annotation_overlay(HDC hdc, int width_px,
                                                    int height_px) const {
  paint_selection_overlay(hdc, width_px, height_px);
}

void Map2dSoftwarePainter::paint(HDC hdc, int width_px, int height_px) const {
  paint(hdc, width_px, height_px, true);
}

void Map2dSoftwarePainter::paint(HDC hdc, int width_px, int height_px,
                                 bool fill_background) const {
  if (!hdc || !scene_ || !frame_ || width_px <= 0 || height_px <= 0) {
    return;
  }

  Map2dFrameCache::PresentAction action =
      Map2dFrameCache::PresentAction::kStaticReuse;
  uint64_t layout_gen = 0;
  Map2dFrameCache::CameraKey cam_key{};
  bool prepared = false;
  if (cache_) {
    // Do not hold cache mutex across prepare/rebuild (async batches + layout).
    // Keep prepare outside the map2d.gdi span — rebuild_layout dominates cold
    // bootstrap and is not HDC paint (frame_paint is the GDI rasterizer).
    BASE_TRACE_EVENT("prepare", "map2d.gdi");
    prepared = cache_->prepare_for_present(static_cast<uint32_t>(width_px),
                                           static_cast<uint32_t>(height_px),
                                           &action);
    if (prepared) {
      std::lock_guard<std::recursive_mutex> lock(cache_->mutex());
      cam_key = cache_->camera();
      layout_gen = cache_->layout_build_count();
      cache_->note_present_outcome(action);
    }
  }

  BASE_TRACE_EVENT("gdi", "map2d.gdi");
  const auto paint_t0 = std::chrono::steady_clock::now();
  const bool can_reuse_pixels =
      prepared &&
      action == Map2dFrameCache::PresentAction::kStaticReuse &&
      cam_key.same_camera(present_cache_cam_) &&
      try_blit_present_cache(hdc, width_px, height_px, layout_gen);
  if (can_reuse_pixels) {
    BASE_TRACE_EVENT("cache_blit", "map2d.gdi");
    paint_selection_overlay(hdc, width_px, height_px);
    note_map2d_phase_software_paint(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - paint_t0)
            .count());
    return;
  }
  // Pan/zoom while layout is reused: shift the last composite as one bitmap
  // so land / hillshade / labels stay locked. Never replay MapIR under a
  // drifted View (pixel_space labels + raster AABB desync).
  if (prepared &&
      action == Map2dFrameCache::PresentAction::kInteractiveReuse &&
      try_interactive_present_cache(hdc, width_px, height_px, layout_gen,
                                    cam_key)) {
    BASE_TRACE_EVENT("interactive_blit", "map2d.gdi");
    paint_selection_overlay(hdc, width_px, height_px);
    note_map2d_phase_software_paint(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - paint_t0)
            .count());
    return;
  }

  {
    BASE_TRACE_EVENT("basemap", "map2d.gdi");
    paint_basemap_underlay(hdc, width_px, height_px);
  }

  bool painted_frame = false;
  if (prepared && cache_) {
    painted_frame = true;
    BASE_TRACE_EVENT("frame_paint", "map2d.gdi");
    // Paint from cached MapIR by reference — deep-copying china tessellation
    // for every present was ~100–900 ms with frame_paint still ~50 ms.
    // InteractiveReuse must use the layout camera (present_cache / publish
    // cam), not the live ViewFrame cam — otherwise pixel_space labels and
    // hillshade quads stay at the bake footprint while world fills move.
    std::lock_guard<std::recursive_mutex> lock(cache_->mutex());
    Map2dFrameCache::CameraKey paint_cam = cache_->camera();
    // prepare_for_present already wrote the live camera into cache_->camera()
    // for InteractiveReuse; MapIR still matches the last published layout cam
    // (kept in present_cache_cam_ after a full paint).
    if (action == Map2dFrameCache::PresentAction::kInteractiveReuse &&
        present_cache_dc_ && present_cache_layout_gen_ == layout_gen &&
        present_cache_cam_.same_pixels(paint_cam)) {
      paint_cam = present_cache_cam_;
    }
    const vista::View view = {paint_cam.width_px, paint_cam.height_px,
                              paint_cam.min_x, paint_cam.min_y, paint_cam.max_x,
                              paint_cam.max_y};
    detail::paint_map_frame_gdi(
        hdc, cache_->frame(), view, fill_background,
        [this](uint32_t texture_key, std::vector<uint8_t>* rgba, int* w,
               int* h) {
          return cache_ && cache_->load_raster(texture_key, rgba, w, h);
        },
        [this](uint32_t texture_key, const uint8_t** rgba, int* w, int* h) {
          return cache_ && cache_->borrow_raster(texture_key, rgba, w, h);
        });
  }
  if (!painted_frame && fill_background) {
    // Style background #aad3df. COLORREF only at the HDC edge.
    HBRUSH bg = CreateSolidBrush(detail::rgba_to_colorref(0xFFAAD3DFu));
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  if (painted_frame) {
    store_present_cache(hdc, width_px, height_px, layout_gen, cam_key);
  } else {
    clear_present_cache();
  }

  {
    BASE_TRACE_EVENT("selection", "map2d.gdi");
    paint_selection_overlay(hdc, width_px, height_px);
  }
  note_map2d_phase_software_paint(
      std::chrono::duration_cast<std::chrono::milliseconds>(
          std::chrono::steady_clock::now() - paint_t0)
          .count());
}


bool Map2dSoftwarePainter::export_bmp(const std::string& path, int width_px,
                                      int height_px) const {
  if (path.empty() || width_px <= 0 || height_px <= 0) {
    return false;
  }
  // Default: clear present cache so export always replays MapIR (faithful
  // carto / hillshade). Bench-only MAP2D_EXPORT_REUSE=1 keeps a matching
  // cam+size present-cache blit (equal-profile paint_ms).
  const bool export_reuse = []() {
    const char* e = base::switch_cstr("map2d-export-reuse");
    return e && e[0] == '1' && e[1] == '\0';
  }();
  if (!export_reuse) {
    clear_present_cache();
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
  // Top-down DIB: matches present-cache HDCs and DibSurface::top_down=true
  // (tessellated line meshes write via fill_tri_solid). A bottom-up export
  // DIB used to invert those software rows vs GDI text/raster.
  bmi.bmiHeader.biHeight = -height_px;
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
  const auto io_t0 = std::chrono::steady_clock::now();
  const bool ok = write_bmp_file(path, width_px, height_px, bits, stride);
  note_map2d_phase_bmp_io(std::chrono::duration_cast<std::chrono::milliseconds>(
                              std::chrono::steady_clock::now() - io_t0)
                              .count());
  SelectObject(mem, old);
  DeleteObject(dib);
  DeleteDC(mem);
  ReleaseDC(nullptr, screen);
  return ok;
}

}  // namespace content
