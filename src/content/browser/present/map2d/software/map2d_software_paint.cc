// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/software/map2d_software_painter.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_carto.h"
#include "content/browser/present/map2d/frame/map2d_frame_cache.h"
#include "content/browser/present/map2d/frame/map2d_tile_math.h"
#include "content/browser/present/map2d/software/map2d_frame_gdi.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include "gis/datasource/provider/impl/ogr/text/ogr_text_encoding.h"
#include "gis/present/tile/protocol/xyz_math.h"
#include "base/trace/process_trace.h"

namespace content {
namespace {

using Feature = MapScene::Feature;
using Layer = MapScene::Layer;
using Vertex = MapScene::Vertex;
using GeomKind = MapScene::GeomKind;

const char* feature_field(const Feature& f, const char* key) {
  if (!key) {
    return nullptr;
  }
  for (const MapScene::Field& field : f.fields) {
    if (field.name == key) {
      return field.value.c_str();
    }
  }
  return nullptr;
}

std::string feature_display_name(const Feature& f) {
  if (const char* anno = feature_field(f, "anno")) {
    if (anno[0]) {
      return anno;
    }
  }
  if (const char* name = feature_field(f, "name")) {
    if (name[0]) {
      return name;
    }
  }
  return {};
}

int label_importance(const Feature& f) {
  const char* cls = feature_field(f, "class");
  if (cls) {
    if (std::strcmp(cls, "title") == 0) {
      return 3;
    }
    if (std::strcmp(cls, "region_label") == 0) {
      return 2;
    }
    if (std::strcmp(cls, "river_label") == 0) {
      return 1;
    }
  }
  const std::string name = feature_display_name(f);
  const int by_name = map_scene_place_name_importance(name.c_str());
  if (by_name >= 3) {
    return 3;
  }
  if (const char* adcode = feature_field(f, "adcode")) {
    const size_t n = std::strlen(adcode);
    if (n >= 6) {
      const bool z45 = adcode[4] == '0' && adcode[5] == '0';
      const bool z23 = adcode[2] == '0' && adcode[3] == '0';
      if (z45 && z23) {
        return 3;
      }
      if (z45) {
        return 2;
      }
      if (by_name > 0) {
        return by_name;
      }
      return 1;
    }
  }
  if (by_name > 0) {
    return by_name;
  }
  if (const char* kind = feature_field(f, "kind")) {
    if (std::strcmp(kind, "city") == 0) {
      return 2;
    }
  }
  return 0;
}

size_t label_cap_for_scale(double scale) {
  if (scale < 22.0) {
    return 36;
  }
  if (scale < 48.0) {
    return 80;
  }
  if (scale < 96.0) {
    return 120;
  }
  return 160;
}

class LabelOccupancy {
 public:
  LabelOccupancy(int view_w, int view_h) : view_w_(view_w), view_h_(view_h) {}

  bool try_place(int x, int y, int w, int h, int* placed_x, int* placed_y) {
    if (!placed_x || !placed_y || w <= 0 || h <= 0) {
      return false;
    }
    const int dxs[5] = {0, 0, 10, 0, -10};
    const int dys[5] = {0, -(h + 2), 0, h + 2, 0};
    for (int i = 0; i < 5; ++i) {
      const int left = x + dxs[i];
      const int top = y + dys[i];
      MapLabelBox box{left, top, left + w, top + h};
      if (box.right < -20 || box.bottom < -12 || box.left > view_w_ + 20 ||
          box.top > view_h_ + 12) {
        continue;
      }
      MapLabelBox padded{box.left - 2, box.top - 2, box.right + 2,
                         box.bottom + 2};
      if (conflicts(padded)) {
        continue;
      }
      accepted_.push_back(padded);
      *placed_x = left;
      *placed_y = top;
      return true;
    }
    return false;
  }

 private:
  bool conflicts(MapLabelBox box) const {
    for (const MapLabelBox& prev : accepted_) {
      if (map_scene_label_boxes_overlap(prev, box)) {
        return true;
      }
    }
    return false;
  }

  int view_w_;
  int view_h_;
  std::vector<MapLabelBox> accepted_;
};

void label_anchor(const Feature& f, double* mx, double* my) {
  if (!mx || !my || f.points.empty()) {
    return;
  }
  if (f.kind == GeomKind::kPolygon && f.points.size() >= 3) {
    double sx = 0;
    double sy = 0;
    for (const Vertex& p : f.points) {
      sx += p.x;
      sy += p.y;
    }
    *mx = sx / static_cast<double>(f.points.size());
    *my = sy / static_cast<double>(f.points.size());
    return;
  }
  *mx = f.points.front().x;
  *my = f.points.front().y;
}

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
  bih.biHeight = height_px;
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

void Map2dSoftwarePainter::bind(const MapScene* scene, const ViewFrame* frame,
                                 Map2dFrameCache* cache) {
  scene_ = scene;
  frame_ = frame;
  cache_ = cache;
  basemap_tiles_drawn_ = 0;
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
    const double lon0 = detail::merc_x_to_lon(tile.world_rect.lb.x);
    const double lon1 = detail::merc_x_to_lon(tile.world_rect.rt.x);
    const double lat0 = detail::merc_y_to_lat(tile.world_rect.lb.y);
    const double lat1 = detail::merc_y_to_lat(tile.world_rect.rt.y);
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
  BASE_TRACE_EVENT("gdi", "map2d.gdi");
  if (!hdc || !scene_ || !frame_ || width_px <= 0 || height_px <= 0) {
    return;
  }

  {
    BASE_TRACE_EVENT("basemap", "map2d.gdi");
    paint_basemap_underlay(hdc, width_px, height_px);
  }

  bool painted_frame = false;
  if (cache_) {
    gis::vista::MapFrame frame_copy;
    gis::vista::View view;
    {
      // Dual-speed: reuse layout on pan (same as GPU present). ensure_full
      // forced a rebuild every GDI paint and dominated the outer gdi span.
      BASE_TRACE_EVENT("ensure", "map2d.gdi");
      std::lock_guard<std::recursive_mutex> lock(cache_->mutex());
      Map2dFrameCache::PresentAction action =
          Map2dFrameCache::PresentAction::kStaticReuse;
      if (cache_->prepare_for_present(static_cast<uint32_t>(width_px),
                                      static_cast<uint32_t>(height_px),
                                      &action)) {
        frame_copy = cache_->frame();
        const Map2dFrameCache::CameraKey& cam = cache_->camera();
        view = {cam.width_px, cam.height_px, cam.min_x, cam.min_y, cam.max_x,
                cam.max_y};
        painted_frame = true;
        cache_->note_present_outcome(action);
      }
    }
    if (painted_frame) {
      BASE_TRACE_EVENT("frame_paint", "map2d.gdi");
      detail::paint_map_frame_gdi(hdc, frame_copy, view, fill_background);
    }
  }
  if (!painted_frame && fill_background) {
    HBRUSH bg = CreateSolidBrush(map_scene_map_bg_color());
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  {
    BASE_TRACE_EVENT("selection", "map2d.gdi");
    paint_selection_overlay(hdc, width_px, height_px);
  }
}

void Map2dSoftwarePainter::paint_labels_projected(
    HDC hdc, int width_px, int height_px,
    const std::function<void(double lon, double lat, int* sx, int* sy)>&
        project) const {
  if (!hdc || !scene_ || !frame_ || width_px <= 0 || height_px <= 0 || !project) {
    return;
  }
  HFONT font = CreateFontW(16, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                           DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                           CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                           DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  HGDIOBJ old_font =
      SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);

  auto draw_label = [&](int vx, int vy, const std::string& name) {
    if (name.empty()) {
      return;
    }
    if (vx < -80 || vy < -40 || vx > width_px + 80 || vy > height_px + 40) {
      return;
    }
    const std::wstring w = gis::datasource::ogr_bytes_to_wide(name);
    if (w.empty()) {
      return;
    }
    const int n = static_cast<int>(w.size());
    SetTextColor(hdc, RGB(20, 24, 32));
    const int halo[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                            {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (const auto& d : halo) {
      TextOutW(hdc, vx + d[0], vy + d[1], w.c_str(), n);
    }
    SetTextColor(hdc, RGB(245, 248, 252));
    TextOutW(hdc, vx, vy, w.c_str(), n);
  };

  const int min_imp = map_scene_label_min_importance(frame_->scale());
  const size_t cap = label_cap_for_scale(frame_->scale());
  bool has_text = false;
  for (const Layer& layer : scene_->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (f.kind == GeomKind::kText) {
        has_text = true;
        break;
      }
    }
    if (has_text) {
      break;
    }
  }

  struct ProjLabel {
    int sx = 0;
    int sy = 0;
    int importance = 0;
    std::string name;
  };
  std::vector<ProjLabel> pending;
  auto consider = [&](const Feature& f) {
    const int importance = label_importance(f);
    if (importance < min_imp) {
      return;
    }
    std::string name = feature_display_name(f);
    if (name.empty()) {
      return;
    }
    double mx = 0;
    double my = 0;
    label_anchor(f, &mx, &my);
    int sx = 0;
    int sy = 0;
    project(mx, -my, &sx, &sy);
    if (sx < -80 || sy < -40 || sx > width_px + 80 || sy > height_px + 40) {
      return;
    }
    pending.push_back({sx, sy, importance, std::move(name)});
  };

  for (const Layer& layer : scene_->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (has_text) {
        if (f.kind == GeomKind::kText && !f.points.empty()) {
          consider(f);
        }
      } else if (f.kind == GeomKind::kPolygon && f.points.size() >= 3) {
        consider(f);
      }
    }
  }

  std::sort(pending.begin(), pending.end(),
            [](const ProjLabel& a, const ProjLabel& b) {
              if (a.importance != b.importance) {
                return a.importance > b.importance;
              }
              return a.name.size() < b.name.size();
            });

  LabelOccupancy label_occ(width_px, height_px);
  size_t drawn = 0;
  for (const ProjLabel& lab : pending) {
    if (drawn >= cap) {
      break;
    }
    const std::wstring w = gis::datasource::ogr_bytes_to_wide(lab.name);
    if (w.empty()) {
      continue;
    }
    int bw = 4;
    for (wchar_t ch : w) {
      bw += (ch < 128) ? 8 : 16;
    }
    int px = 0;
    int py = 0;
    if (!label_occ.try_place(lab.sx, lab.sy - 8, bw, 18, &px, &py)) {
      continue;
    }
    draw_label(px, py, lab.name);
    ++drawn;
  }

  SelectObject(hdc, old_font);
  if (font) {
    DeleteObject(font);
  }
}

bool Map2dSoftwarePainter::export_bmp(const std::string& path, int width_px,
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
  bmi.bmiHeader.biHeight = height_px;
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

}  // namespace content
