// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/present/map2d/map2d_presenter.h"

#include "app/views/camera/view_frame.h"
#include "app/views/document/map_scene.h"
#include "app/views/present/map2d/frame/map2d_carto.h"
#include "app/views/present/map2d/frame/map2d_tile_math.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "gis/datasource/ogr/text/ogr_text_encoding.h"
#include "gis/present/style/style_document.h"
#include "gis/present/style/style_rules.h"
#include "gis/present/tile/protocol/xyz_math.h"

namespace app {
namespace {

const char* field_value(const MapScene::Feature& f, const char* key) {
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

// Prefer anno (SmtFtAnno), then name, for Chinese annotation labels.
std::string feature_display_name(const MapScene::Feature& f) {
  if (const char* anno = field_value(f, "anno")) {
    if (anno[0]) {
      return anno;
    }
  }
  if (const char* name = field_value(f, "name")) {
    if (name[0]) {
      return name;
    }
  }
  return {};
}

bool ascii_icontains(const char* hay, const char* needle) {
  if (!hay || !needle || !needle[0]) {
    return false;
  }
  const size_t n = std::strlen(needle);
  for (const char* p = hay; *p; ++p) {
    size_t i = 0;
    for (; i < n; ++i) {
      unsigned char a = static_cast<unsigned char>(p[i]);
      unsigned char b = static_cast<unsigned char>(needle[i]);
      if (a == 0) {
        return false;
      }
      if (a >= 'A' && a <= 'Z') {
        a = static_cast<unsigned char>(a - 'A' + 'a');
      }
      if (b >= 'A' && b <= 'Z') {
        b = static_cast<unsigned char>(b - 'A' + 'a');
      }
      if (a != b) {
        break;
      }
    }
    if (i == n) {
      return true;
    }
  }
  return false;
}

// Cartographic importance 0 (POI) .. 3 (title / province / capital).
int label_importance(const MapScene::Feature& f) {
  const char* cls = field_value(f, "class");
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
  // Capitals and provinces stay on the country frame even when adcode is
  // only prefecture-level (DataV city packs).
  if (by_name >= 3) {
    return 3;
  }
  if (const char* adcode = field_value(f, "adcode")) {
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
  if (const char* kind = field_value(f, "kind")) {
    if (std::strcmp(kind, "city") == 0) {
      return 2;
    }
  }
  return 0;
}

int label_font_index(int importance) {
  if (importance >= 3) {
    return 2;
  }
  if (importance >= 2) {
    return 1;
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

// Glyph boxes, not a single anchor cell. A 2px pad is stored so neighbors
// do not touch. A few offsets are tried before the label is dropped.
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

void label_anchor(const MapScene::Feature& f, double* mx, double* my) {
  if (!mx || !my || f.points.empty()) {
    return;
  }
  if (f.kind == MapScene::GeomKind::kPoint ||
      f.kind == MapScene::GeomKind::kText) {
    *mx = f.points[0].x;
    *my = f.points[0].y;
    return;
  }
  double sx = 0;
  double sy = 0;
  for (const MapScene::Vertex& p : f.points) {
    sx += p.x;
    sy += p.y;
  }
  const double n = static_cast<double>(f.points.size());
  *mx = sx / n;
  *my = sy / n;
}

constexpr double kPi = 3.14159265358979323846;

COLORREF argb_to_colorref(uint32_t argb) {
  return RGB(static_cast<int>((argb >> 16) & 0xFF),
             static_cast<int>((argb >> 8) & 0xFF),
             static_cast<int>(argb & 0xFF));
}

}  // namespace

using Feature = MapScene::Feature;
using Layer = MapScene::Layer;
using Vertex = MapScene::Vertex;
using GeomKind = MapScene::GeomKind;

void Map2dPresenter::paint_basemap_underlay(HDC hdc, int width_px,
                                     int height_px) const {
  gis::tile::TileProvider* basemap =
      scene_ ? scene_->basemap_provider() : nullptr;
  basemap_tiles_drawn_ = 0;
  if (!hdc || !scene_ || !frame_ || !basemap || !basemap->is_open() || width_px <= 0 ||
      height_px <= 0) {
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
  const std::vector<gis::tile::TileImage> tiles =
      basemap->fetch_visible(vp, 2);
  basemap_tiles_drawn_ = tiles.size();
  for (const gis::tile::TileImage& tile : tiles) {
    const double lon0 = detail::merc_x_to_lon(tile.world_rect.lb.x);
    const double lon1 = detail::merc_x_to_lon(tile.world_rect.rt.x);
    const double lat0 = detail::merc_y_to_lat(tile.world_rect.lb.y);
    const double lat1 = detail::merc_y_to_lat(tile.world_rect.rt.y);
    // Map-space Y is -lat.
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
    // Encoded PNG decode is optional; a tinted quad proves underlay coverage.
    const int shade = 90 + ((tile.coord.x + tile.coord.y) & 7) * 12;
    HBRUSH brush =
        CreateSolidBrush(RGB(shade, 110 + (tile.coord.z % 5) * 8, 140));
    RECT rc = {vx0, vy0, vx1 + 1, vy1 + 1};
    FillRect(hdc, &rc, brush);
    DeleteObject(brush);
  }
}

void Map2dPresenter::paint_flash_overlay(HDC hdc, int width_px,
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

void Map2dPresenter::paint_selection_overlay(HDC hdc, int width_px,
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

void Map2dPresenter::paint_annotation_overlay(HDC hdc, int width_px,
                                        int height_px) const {
  // kText labels are items in the map2d frame. Selection stroke stays in GDI.
  paint_selection_overlay(hdc, width_px, height_px);
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px) const {
  paint(hdc, width_px, height_px, true);
}

void Map2dPresenter::paint(HDC hdc, int width_px, int height_px,
                     bool fill_background) const {
  if (!hdc || !scene_ || !frame_ || width_px <= 0 || height_px <= 0) {
    return;
  }

  // Ocean canvas (Baidu-like). Callers may have filled a dark placeholder.
  if (fill_background) {
    HBRUSH bg = CreateSolidBrush(map_scene_map_bg_color());
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  paint_basemap_underlay(hdc, width_px, height_px);

  // Reuse a few GDI objects for the whole frame. Creating Pen/Brush/Font per
  // feature leaked when early-continue skipped DeleteObject, and exhausted the
  // per-process GDI quota (~10k) on china_city (~1.4k features × 30 Hz).
  // Font heights are DIP → device px so 150%/200% displays stay readable.
  const int dpi = std::max(96, GetDeviceCaps(hdc, LOGPIXELSY));
  auto dip_px = [dpi](int px96) { return -MulDiv(px96, dpi, 96); };
  HPEN pens[4] = {};
  pens[0] =
      CreatePen(PS_SOLID, 1, map_scene_admin_stroke_color());  // polygon outline
  pens[1] = CreatePen(PS_SOLID, 2, map_scene_river_color());   // river
  pens[2] = CreatePen(PS_SOLID, 2, RGB(220, 210, 190));         // other line
  pens[3] = CreatePen(PS_SOLID, 3, RGB(200, 120, 40));         // selected
  HBRUSH land_brush = CreateSolidBrush(map_scene_area_fill_color(nullptr, 0));
  HBRUSH point_brush = CreateSolidBrush(map_scene_point_fill_color());
  HBRUSH selected_brush = CreateSolidBrush(RGB(255, 200, 80));
  // Style-driven colors: cache pens/brushes per COLORREF for this frame only.
  std::map<COLORREF, HBRUSH> style_brushes;
  std::map<uint64_t, HPEN> style_pens;
  auto style_brush = [&](COLORREF c) -> HBRUSH {
    auto it = style_brushes.find(c);
    if (it != style_brushes.end()) {
      return it->second;
    }
    HBRUSH b = CreateSolidBrush(c);
    style_brushes.emplace(c, b);
    return b;
  };
  auto style_pen = [&](COLORREF c, int width) -> HPEN {
    const uint64_t key =
        (static_cast<uint64_t>(static_cast<uint32_t>(c)) << 16) |
        static_cast<uint64_t>(static_cast<uint16_t>(std::max(1, width)));
    auto it = style_pens.find(key);
    if (it != style_pens.end()) {
      return it->second;
    }
    HPEN p = CreatePen(PS_SOLID, std::max(1, width), c);
    style_pens.emplace(key, p);
    return p;
  };
  HFONT fonts[3] = {};
  // Baidu-like hierarchy: small body / medium prefecture / large province title.
  fonts[0] = CreateFontW(dip_px(13), 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  fonts[1] = CreateFontW(dip_px(16), 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  fonts[2] = CreateFontW(dip_px(22), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                         DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                         CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                         DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
  HFONT status_font =
      CreateFontW(dip_px(13), 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE,
                  DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                  CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_SWISS,
                  L"Microsoft YaHei UI");
  HGDIOBJ stock_font = GetStockObject(DEFAULT_GUI_FONT);
  HGDIOBJ old_pen = SelectObject(hdc, pens[0] ? pens[0] : GetStockObject(BLACK_PEN));
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  HGDIOBJ old_font = SelectObject(hdc, fonts[0] ? fonts[0] : stock_font);
  SetBkMode(hdc, TRANSPARENT);

  auto draw_label = [&](int vx, int vy, const std::string& name, COLORREF color,
                        int font_idx, int dx, int dy, double angle_deg,
                        bool along_line) {
    if (name.empty()) {
      return;
    }
    // Skip labels that are far outside the viewport (cheap cull).
    if (vx + dx < -80 || vy + dy < -40 || vx + dx > width_px + 80 ||
        vy + dy > height_px + 40) {
      return;
    }
    const std::wstring w = gis::datasource::ogr_bytes_to_wide(name);
    if (w.empty()) {
      return;
    }
    HFONT font = fonts[font_idx < 0 ? 0 : (font_idx > 2 ? 2 : font_idx)];
    HFONT rotated = nullptr;
    if (along_line && std::fabs(angle_deg) > 0.5) {
      const int esc = static_cast<int>(std::lround(-angle_deg * 10.0));
      const int px96 = font_idx >= 2 ? 22 : (font_idx == 1 ? 16 : 13);
      const int weight = font_idx >= 2 ? FW_SEMIBOLD
                                        : (font_idx == 1 ? FW_MEDIUM : FW_NORMAL);
      rotated = CreateFontW(dip_px(px96), 0, esc, esc, weight, FALSE, FALSE,
                            FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                            CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei UI");
    }
    SelectObject(hdc, rotated ? rotated : (font ? font : stock_font));
    const int x = vx + dx;
    const int y = vy + dy;
    const int n = static_cast<int>(w.size());
    // Light halo for CJK on pastel fills.
    SetTextColor(hdc, RGB(255, 255, 255));
    const int halo[8][2] = {{-1, 0},  {1, 0},  {0, -1}, {0, 1},
                            {-1, -1}, {1, -1}, {-1, 1}, {1, 1}};
    for (const auto& d : halo) {
      TextOutW(hdc, x + d[0], y + d[1], w.c_str(), n);
    }
    SetTextColor(hdc, color);
    TextOutW(hdc, x, y, w.c_str(), n);
    if (rotated) {
      SelectObject(hdc, font ? font : stock_font);
      DeleteObject(rotated);
    }
  };

  // Country fit is scale ~8–16. Dense POI text waits until the view is
  // actually close; the old scale>=18 gate still stacked city names.
  const bool draw_dense_text = frame_->scale() >= 96.0;
  const int min_label_importance = map_scene_label_min_importance(frame_->scale());
  const size_t label_cap = label_cap_for_scale(frame_->scale());

  struct PendingLabel {
    int vx = 0;
    int vy = 0;
    int dx = 0;
    int dy = 0;
    int importance = 0;
    COLORREF ink = RGB(20, 18, 14);
    std::string name;
    double angle_deg = 0;
    bool along_line = false;
  };
  std::vector<PendingLabel> pending_labels;
  pending_labels.reserve(label_cap * 2);
  auto queue_place_label = [&](int vx, int vy, const Feature& f, COLORREF ink,
                               int dx, int dy) {
    const int importance = label_importance(f);
    if (importance < min_label_importance) {
      return;
    }
    std::string name = feature_display_name(f);
    if (name.empty()) {
      return;
    }
    if (vx + dx < -80 || vy + dy < -40 || vx + dx > width_px + 80 ||
        vy + dy > height_px + 40) {
      return;
    }
    pending_labels.push_back(
        {vx, vy, dx, dy, importance, ink, std::move(name)});
  };

  bool has_text_features = false;
  for (const Layer& layer : scene_->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      if (f.kind == GeomKind::kText) {
        has_text_features = true;
        break;
      }
    }
    if (has_text_features) {
      break;
    }
  }
  constexpr size_t kMaxPolyPts = 2048;
  double map_x0 = 0;
  double map_y0 = 0;
  double map_x1 = 0;
  double map_y1 = 0;
  frame_->view_to_map(-48, -48, &map_x0, &map_y0);
  frame_->view_to_map(width_px + 48, height_px + 48, &map_x1, &map_y1);
  const double view_min_x = (std::min)(map_x0, map_x1);
  const double view_max_x = (std::max)(map_x0, map_x1);
  const double view_min_y = (std::min)(map_y0, map_y1);
  const double view_max_y = (std::max)(map_y0, map_y1);
  const content::Extent2 scene_ext = scene_->world_extent();
  const bool extent_lonlat = map_scene_extent_is_lonlat(
      scene_ext.xmin, scene_ext.ymin, scene_ext.xmax, scene_ext.ymax);
  const double touch_tol = extent_lonlat ? 0.02 : 2000.0;

  struct StemHeld {
    const Feature* feature = nullptr;
    std::string key;
    double length = 0;
    double x0 = 0;
    double y0 = 0;
    double x1 = 0;
    double y1 = 0;
  };
  std::vector<StemHeld> stem_held;
  auto consider_stem = [&](const Feature& f) {
    if (f.kind != GeomKind::kLine || f.points.size() < 2) {
      return;
    }
    const char* cls = field_value(f, "class");
    if (!cls) {
      cls = field_value(f, "fclass");
    }
    if (!cls) {
      cls = field_value(f, "highway");
    }
    const char* kind = field_value(f, "kind");
    const char* line_kind = field_value(f, "line_kind");
    const char* role_kind = kind;
    if (line_kind && line_kind[0] &&
        (!kind || std::strcmp(kind, "line") == 0)) {
      role_kind = line_kind;
    }
    const MapLineRole role = map_scene_line_role(role_kind, cls);
    if (role != MapLineRole::kWater && role != MapLineRole::kRoad) {
      return;
    }
    StemHeld held;
    held.feature = &f;
    held.key = feature_display_name(f);
    if (held.key.empty()) {
      const char* rid = field_value(f, "river_id");
      if (!rid || !rid[0]) {
        rid = field_value(f, "id");
      }
      if (rid && rid[0]) {
        held.key = std::string("id:") + rid;
      }
    }
    held.x0 = f.points.front().x;
    held.y0 = f.points.front().y;
    held.x1 = f.points.back().x;
    held.y1 = f.points.back().y;
    for (size_t i = 1; i < f.points.size(); ++i) {
      held.length += std::hypot(f.points[i].x - f.points[i - 1].x,
                                f.points[i].y - f.points[i - 1].y);
    }
    stem_held.push_back(std::move(held));
  };
  for (const Layer& layer : scene_->layers()) {
    if (!layer.visible) {
      continue;
    }
    for (const Feature& f : layer.features) {
      consider_stem(f);
    }
  }
  std::vector<MapStemSpan> stem_spans(stem_held.size());
  for (size_t i = 0; i < stem_held.size(); ++i) {
    stem_spans[i] = {stem_held[i].key.c_str(), stem_held[i].length,
                     stem_held[i].x0, stem_held[i].y0, stem_held[i].x1,
                     stem_held[i].y1};
  }
  std::map<const Feature*, double> stem_raw;
  std::vector<double> stem_lens(stem_held.size(), 0.0);
  if (!stem_spans.empty()) {
    map_scene_fill_stem_lengths(stem_spans.data(), stem_spans.size(), touch_tol,
                                stem_lens.data());
  }
  for (size_t i = 0; i < stem_held.size(); ++i) {
    stem_raw[stem_held[i].feature] = stem_lens[i];
  }

  struct AlongCand {
    double rank_len = 0;
    int vx = 0;
    int vy = 0;
    double angle = 0;
    int importance = 0;
    COLORREF ink = RGB(8, 36, 72);
    std::string name;
  };
  std::map<std::string, AlongCand> along_best;

  const GeomKind order[] = {GeomKind::kPolygon, GeomKind::kLine,
                            GeomKind::kPoint, GeomKind::kText};
  for (GeomKind pass : order) {
    for (const Layer& layer : scene_->layers()) {
      if (!layer.visible) {
        continue;
      }
      for (const Feature& f : layer.features) {
        if (f.kind != pass || f.points.empty()) {
          continue;
        }

        if (f.kind == GeomKind::kText) {
          const char* cls = field_value(f, "class");
          int vx = 0;
          int vy = 0;
          frame_->map_to_view(f.points[0].x, f.points[0].y, &vx, &vy);
          COLORREF ink = RGB(20, 18, 14);
          if (cls && std::strcmp(cls, "title") == 0) {
            ink = RGB(12, 10, 8);
          } else if (cls && std::strcmp(cls, "river_label") == 0) {
            ink = RGB(8, 36, 72);
          }
          if (f.selected) {
            ink = RGB(200, 120, 40);
          }
          queue_place_label(vx, vy, f, ink, -20, -8);
          continue;
        }

        if (f.kind == GeomKind::kPoint) {
          int vx = 0;
          int vy = 0;
          frame_->map_to_view(f.points[0].x, f.points[0].y, &vx, &vy);
          if (vx < -20 || vy < -20 || vx > width_px + 20 ||
              vy > height_px + 20) {
            continue;
          }
          // Thin Baidu-like POI: white halo + soft fill (scale-aware radius).
          const int r = frame_->scale() < 48.0 ? 1 : 2;
          const int outer = r + 1;
          HBRUSH ring = CreateSolidBrush(RGB(255, 255, 255));
          SelectObject(hdc, ring);
          SelectObject(hdc, GetStockObject(NULL_PEN));
          Ellipse(hdc, vx - outer, vy - outer, vx + outer + 1, vy + outer + 1);
          DeleteObject(ring);
          COLORREF fill_c = map_scene_point_fill_color();
          COLORREF stroke_c = fill_c;
          int stroke_w = 1;
          const bool styled =
              !f.selected &&
              scene_->style_colors_for_feature(layer, f, frame_->scale(), &fill_c, &stroke_c, &stroke_w);
          HBRUSH fill_brush =
              f.selected ? selected_brush
                         : (styled ? style_brush(fill_c) : point_brush);
          SelectObject(hdc, fill_brush);
          SelectObject(hdc, f.selected ? pens[3] : pens[0]);
          Ellipse(hdc, vx - r, vy - r, vx + r + 1, vy + r + 1);
          SelectObject(hdc, GetStockObject(NULL_BRUSH));
          if (draw_dense_text && !has_text_features) {
            queue_place_label(vx, vy, f, RGB(40, 32, 20), 7, -8);
          }
          continue;
        }

        if (f.kind == GeomKind::kLine) {
          const char* cls = field_value(f, "class");
          if (!cls) {
            cls = field_value(f, "fclass");
          }
          if (!cls) {
            cls = field_value(f, "highway");
          }
          const char* kind = field_value(f, "kind");
          const char* line_kind = field_value(f, "line_kind");
          const char* role_kind = kind;
          if (line_kind && line_kind[0] &&
              (!kind || std::strcmp(kind, "line") == 0)) {
            role_kind = line_kind;
          }
          const MapLineRole role = map_scene_line_role(role_kind, cls);
          const bool major = map_scene_line_is_major_class(role_kind, cls);
          double minx = f.points[0].x;
          double maxx = minx;
          double miny = f.points[0].y;
          double maxy = miny;
          double length = 0.0;
          for (size_t i = 1; i < f.points.size(); ++i) {
            const Vertex& a = f.points[i - 1];
            const Vertex& b = f.points[i];
            length += std::hypot(b.x - a.x, b.y - a.y);
            minx = (std::min)(minx, b.x);
            maxx = (std::max)(maxx, b.x);
            miny = (std::min)(miny, b.y);
            maxy = (std::max)(maxy, b.y);
          }
          if (!f.selected &&
              (maxx < view_min_x || minx > view_max_x || maxy < view_min_y ||
               miny > view_max_y)) {
            continue;
          }
          double stem = length;
          const auto stem_it = stem_raw.find(&f);
          if (stem_it != stem_raw.end()) {
            stem = (std::max)(stem, stem_it->second);
          }
          const double gated =
              map_scene_length_as_degrees(stem, extent_lonlat);
          if (!f.selected &&
              !map_scene_line_visible_at_scale(role, gated, major, frame_->scale())) {
            continue;
          }
          COLORREF fill_c = 0;
          COLORREF stroke_c = 0;
          int style_w = 2;
          const bool styled =
              scene_->style_colors_for_feature(layer, f, frame_->scale(), &fill_c, &stroke_c, &style_w);
          HPEN pen = pens[3];
          if (!f.selected) {
            const int sw = map_scene_line_stroke_px(role, gated, frame_->scale());
            COLORREF col = RGB(220, 210, 190);
            if (role == MapLineRole::kWater) {
              const bool lake = ascii_icontains(role_kind, "lake") ||
                                ascii_icontains(cls, "lake");
              col = lake ? RGB(140, 186, 214) : map_scene_river_color();
            } else if (role == MapLineRole::kRoad) {
              col = (major || gated >= 3.0) ? map_scene_road_color()
                                            : RGB(176, 170, 158);
            } else if (styled) {
              col = stroke_c;
              (void)style_w;
            }
            pen = style_pen(col, sw);
          }
          SelectObject(hdc, pen ? pen : GetStockObject(BLACK_PEN));
          const int min_step = (!f.selected && frame_->scale() < 22.0) ? 2 : 0;
          bool first = true;
          int last_x = 0;
          int last_y = 0;
          for (size_t i = 0; i < f.points.size(); ++i) {
            int vx = 0;
            int vy = 0;
            frame_->map_to_view(f.points[i].x, f.points[i].y, &vx, &vy);
            const bool last = i + 1 == f.points.size();
            if (!first && !last && min_step > 0) {
              const int dx = vx - last_x;
              const int dy = vy - last_y;
              if (dx * dx + dy * dy < min_step * min_step) {
                continue;
              }
            }
            if (first) {
              MoveToEx(hdc, vx, vy, nullptr);
              first = false;
            } else {
              LineTo(hdc, vx, vy);
            }
            last_x = vx;
            last_y = vy;
          }
          if (role == MapLineRole::kWater || role == MapLineRole::kRoad) {
            std::string along_name = feature_display_name(f);
            if (!along_name.empty()) {
              std::vector<double> xs;
              std::vector<double> ys;
              std::vector<double> run_x;
              std::vector<double> run_y;
              auto take_run = [&]() {
                if (run_x.size() > xs.size()) {
                  xs = run_x;
                  ys = run_y;
                }
                run_x.clear();
                run_y.clear();
              };
              for (const Vertex& p : f.points) {
                const bool inside = p.x >= view_min_x && p.x <= view_max_x &&
                                    p.y >= view_min_y && p.y <= view_max_y;
                if (!inside) {
                  take_run();
                  continue;
                }
                run_x.push_back(p.x);
                run_y.push_back(p.y);
              }
              take_run();
              if (xs.size() < 2) {
                xs.clear();
                ys.clear();
                for (const Vertex& p : f.points) {
                  xs.push_back(p.x);
                  ys.push_back(p.y);
                }
              }
              const MapLineLabelAnchor anchor = map_scene_line_label_anchor(
                  xs.data(), ys.data(), xs.size());
              if (anchor.ok) {
                int lvx = 0;
                int lvy = 0;
                frame_->map_to_view(anchor.x, anchor.y, &lvx, &lvy);
                int importance = label_importance(f);
                if (importance < 1) {
                  importance = 1;
                }
                auto found = along_best.find(along_name);
                if (found == along_best.end() || gated > found->second.rank_len) {
                  AlongCand cand;
                  cand.rank_len = gated;
                  cand.vx = lvx;
                  cand.vy = lvy;
                  cand.angle = anchor.angle_deg;
                  cand.importance = importance;
                  cand.ink = role == MapLineRole::kWater ? RGB(8, 36, 72)
                                                        : RGB(90, 70, 30);
                  cand.name = std::move(along_name);
                  along_best[cand.name] = std::move(cand);
                }
              }
            }
          }
          continue;
        }

        // Polygon: fill + outline. Cap vertex count for GDI Polygon safety.
        std::vector<POINT> pts;
        pts.reserve(std::min(f.points.size(), kMaxPolyPts));
        const size_t step =
            f.points.size() > kMaxPolyPts
                ? (f.points.size() + kMaxPolyPts - 1) / kMaxPolyPts
                : 1;
        for (size_t i = 0; i < f.points.size(); i += step) {
          int vx = 0;
          int vy = 0;
          frame_->map_to_view(f.points[i].x, f.points[i].y, &vx, &vy);
          pts.push_back({vx, vy});
        }
        if (pts.size() >= 3) {
          // Do NOT CreatePen/Brush per polygon — china_city has ~476 areas and
          // the present timer repaints ~30 Hz; per-feature GDI creates exhaust
          // the process quota and crash the host even when DeleteObject is called.
          COLORREF fill_c = map_scene_area_fill_color(nullptr, 0);
          COLORREF stroke_c = map_scene_admin_stroke_color();
          int stroke_w = 1;
          const bool styled =
              !f.selected &&
              scene_->style_colors_for_feature(layer, f, frame_->scale(), &fill_c, &stroke_c, &stroke_w);
          SelectObject(hdc, f.selected
                                ? pens[3]
                                : (styled ? style_pen(stroke_c, stroke_w)
                                          : pens[0]));
          HBRUSH fill = f.selected
                            ? selected_brush
                            : (styled ? style_brush(fill_c) : land_brush);
          SelectObject(hdc, fill ? fill : GetStockObject(LTGRAY_BRUSH));
          Polygon(hdc, pts.data(), static_cast<int>(pts.size()));
          SelectObject(hdc, GetStockObject(NULL_BRUSH));
          Polyline(hdc, pts.data(), static_cast<int>(pts.size()));
          // Region name only when there is no dedicated text layer (china_plp
          // already has kind=label points). Avoid stacking the same CJK name.
          if (!has_text_features) {
            double mx = 0;
            double my = 0;
            label_anchor(f, &mx, &my);
            int lvx = 0;
            int lvy = 0;
            frame_->map_to_view(mx, my, &lvx, &lvy);
            queue_place_label(lvx, lvy, f, RGB(32, 28, 22), -12, -6);
          }
        }
      }
    }
  }

  for (auto& kv : along_best) {
    AlongCand& cand = kv.second;
    if (cand.importance < min_label_importance || cand.name.empty()) {
      continue;
    }
    PendingLabel lab;
    lab.vx = cand.vx;
    lab.vy = cand.vy;
    lab.importance = cand.importance;
    lab.ink = cand.ink;
    lab.name = std::move(cand.name);
    lab.angle_deg = cand.angle;
    lab.along_line = true;
    pending_labels.push_back(std::move(lab));
  }

  // Higher-importance labels claim occupancy first (province before county).
  std::sort(pending_labels.begin(), pending_labels.end(),
            [](const PendingLabel& a, const PendingLabel& b) {
              if (a.importance != b.importance) {
                return a.importance > b.importance;
              }
              return a.name.size() < b.name.size();
            });
  LabelOccupancy label_occ(width_px, height_px);
  size_t labels_drawn = 0;
  for (const PendingLabel& lab : pending_labels) {
    if (labels_drawn >= label_cap) {
      break;
    }
    const std::wstring w = gis::datasource::ogr_bytes_to_wide(lab.name);
    if (w.empty()) {
      continue;
    }
    const int font_idx = label_font_index(lab.importance);
    HFONT font = fonts[font_idx];
    SelectObject(hdc, font ? font : stock_font);
    SIZE text_sz = {};
    GetTextExtentPoint32W(hdc, w.c_str(), static_cast<int>(w.size()), &text_sz);
    int origin_x = lab.vx + lab.dx;
    int origin_y = lab.vy + lab.dy;
    int box_w = (std::max)(8, static_cast<int>(text_sz.cx));
    int box_h = (std::max)(8, static_cast<int>(text_sz.cy));
    if (lab.along_line) {
      const double rad = lab.angle_deg * (3.14159265358979323846 / 180.0);
      const double c = std::cos(rad);
      const double s = std::sin(rad);
      const double hx = 0.5 * static_cast<double>(text_sz.cx);
      const double hy = 0.5 * static_cast<double>(text_sz.cy);
      origin_x += static_cast<int>(std::lround(-hx * c + hy * s));
      origin_y += static_cast<int>(std::lround(-hx * s - hy * c));
      box_w = (std::max)(
          8, static_cast<int>(std::lround(std::fabs(c) * text_sz.cx +
                                         std::fabs(s) * text_sz.cy)));
      box_h = (std::max)(
          8, static_cast<int>(std::lround(std::fabs(s) * text_sz.cx +
                                         std::fabs(c) * text_sz.cy)));
    }
    int px = 0;
    int py = 0;
    if (!label_occ.try_place(origin_x, origin_y, box_w, box_h, &px, &py)) {
      continue;
    }
    draw_label(px, py, lab.name, lab.ink, font_idx, 0, 0, lab.angle_deg,
               lab.along_line);
    ++labels_drawn;
  }

  SelectObject(hdc, old_font);
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  for (HFONT font : fonts) {
    if (font) {
      DeleteObject(font);
    }
  }
  if (land_brush) {
    DeleteObject(land_brush);
  }
  if (point_brush) {
    DeleteObject(point_brush);
  }
  if (selected_brush) {
    DeleteObject(selected_brush);
  }
  for (HPEN pen : pens) {
    if (pen) {
      DeleteObject(pen);
    }
  }
  for (auto& kv : style_brushes) {
    if (kv.second) {
      DeleteObject(kv.second);
    }
  }
  for (auto& kv : style_pens) {
    if (kv.second) {
      DeleteObject(kv.second);
    }
  }

  // Opaque status strip — avoids ghosting / illegible overlap on dense labels.
  SetBkMode(hdc, TRANSPARENT);
  SelectObject(hdc, status_font ? status_font : stock_font);
  wchar_t line[160];
  swprintf_s(line, L"Layers %zu  Features %zu  scale %.4g%s", scene_->layers().size(),
             scene_->feature_count(), frame_->scale(), scene_->last_open_was_ogr() ? L"  OGR" : L"");
  const int status_y = height_px > 48 ? height_px - 36 : 12;
  SIZE text_sz = {};
  GetTextExtentPoint32W(hdc, line, lstrlenW(line), &text_sz);
  RECT status_rc = {8, status_y - 4, 16 + text_sz.cx, status_y + text_sz.cy + 4};
  HBRUSH status_bg = CreateSolidBrush(RGB(245, 243, 233));
  FillRect(hdc, &status_rc, status_bg);
  DeleteObject(status_bg);
  SetTextColor(hdc, RGB(24, 28, 32));
  TextOutW(hdc, 12, status_y, line, lstrlenW(line));
  if (status_font) {
    SelectObject(hdc, stock_font);
    DeleteObject(status_font);
  }
}

void Map2dPresenter::paint_labels_projected(
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

}  // namespace app
