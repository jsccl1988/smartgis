// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/vista/frame/frame.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/parallel/for.h"
#include "base/memory/arena.h"
#include "base/trace/event/process_trace.h"
#include "gis/vista/frame/detail/collision.h"
#include "gis/present/style/paint_resolve.h"
#include "gis/present/style/style_rules.h"
#include "gis/vista/world/terrain/tessellate.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace vista {
namespace {

constexpr int kCircleSegments = 32;
constexpr float kPi = 3.14159265f;
// Tessellation grain: below this, stay serial (pool overhead). Trace showed
// china batches often sit near this floor; 2 matches visible_layer parallel_for.
constexpr size_t kParallelTessMinGeoms = 2;
// Prefer auto grain (span / (workers*4)); fixed grain=1 oversubscribed Debug.
constexpr size_t kParallelTessGrain = 0;

double world_units_per_pixel(const View& view) {
  if (view.width_px == 0 || view.max_x <= view.min_x) {
    return 0;
  }
  return (view.max_x - view.min_x) / static_cast<double>(view.width_px);
}

float device_px_per_world(const View& view) {
  const double wupp = world_units_per_pixel(view);
  if (wupp <= 0) {
    return 1.f;
  }
  return static_cast<float>(1.0 / wupp);
}

bool screen_ready(const View& view) {
  return view.width_px > 0 && view.height_px > 0 && view.max_x > view.min_x &&
         view.max_y > view.min_y;
}

struct ScreenPt {
  float x = 0;
  float y = 0;
};

// North is up in world y and down in pixel y.
ScreenPt to_screen(const View& view, double x, double y) {
  ScreenPt s;
  s.x = static_cast<float>((x - view.min_x) / (view.max_x - view.min_x) *
                           static_cast<double>(view.width_px));
  s.y = static_cast<float>((view.max_y - y) / (view.max_y - view.min_y) *
                           static_cast<double>(view.height_px));
  return s;
}

gis::style::AttrMap attrs_at(const LayerBatch& batch, size_t index) {
  if (index < batch.attrs.size()) {
    return batch.attrs[index];
  }
  return {};
}

const char* attr_cstr(const gis::style::AttrMap& attrs, const char* key) {
  const auto it = attrs.find(key);
  if (it == attrs.end() || it->second.empty()) {
    return nullptr;
  }
  return it->second.c_str();
}

const char* class_cstr(const gis::style::AttrMap& attrs) {
  if (const char* cls = attr_cstr(attrs, "class")) {
    return cls;
  }
  return attr_cstr(attrs, "cls");
}

std::string expand_tokens(const std::string& field,
                          const gis::style::AttrMap& attrs) {
  std::string out;
  out.reserve(field.size());
  for (size_t i = 0; i < field.size(); ++i) {
    if (field[i] != '{') {
      out.push_back(field[i]);
      continue;
    }
    const size_t end = field.find('}', i + 1);
    if (end == std::string::npos) {
      out.push_back(field[i]);
      continue;
    }
    const std::string key = field.substr(i + 1, end - i - 1);
    const auto it = attrs.find(key);
    if (it != attrs.end()) {
      out += it->second;
    }
    i = end;
  }
  return out;
}

std::string label_text(const gis::style::ResolvedPaint& paint,
                       const gis::style::AttrMap& attrs) {
  if (!paint.text_field.empty()) {
    return expand_tokens(paint.text_field, attrs);
  }
  if (const char* anno = attr_cstr(attrs, "anno")) {
    return anno;
  }
  if (const char* name = attr_cstr(attrs, "name")) {
    return name;
  }
  if (const char* text = attr_cstr(attrs, "text")) {
    return text;
  }
  return {};
}

const SymbolAsset* find_symbol(const LayoutInput& in, const std::string& id) {
  if (id.empty()) {
    return nullptr;
  }
  for (const SymbolAsset& asset : in.symbols) {
    if (asset.id == id) {
      return &asset;
    }
  }
  return nullptr;
}

bool layer_uses_batch(const gis::style::StyleLayer& layer,
                      const LayerBatch& batch) {
  return layer.source_layer.empty() || layer.source_layer == batch.source_layer;
}

bool next_codepoint(const std::string& text, size_t* index, uint32_t* cp) {
  if (*index >= text.size()) {
    return false;
  }
  const auto* p =
      reinterpret_cast<const unsigned char*>(text.data() + *index);
  const size_t left = text.size() - *index;
  if (p[0] < 0x80) {
    *cp = p[0];
    *index += 1;
    return true;
  }
  if ((p[0] & 0xe0) == 0xc0 && left >= 2) {
    *cp = (static_cast<uint32_t>(p[0] & 0x1f) << 6) |
          static_cast<uint32_t>(p[1] & 0x3f);
    *index += 2;
    return true;
  }
  if ((p[0] & 0xf0) == 0xe0 && left >= 3) {
    *cp = (static_cast<uint32_t>(p[0] & 0x0f) << 12) |
          (static_cast<uint32_t>(p[1] & 0x3f) << 6) |
          static_cast<uint32_t>(p[2] & 0x3f);
    *index += 3;
    return true;
  }
  if ((p[0] & 0xf8) == 0xf0 && left >= 4) {
    *cp = (static_cast<uint32_t>(p[0] & 0x07) << 18) |
          (static_cast<uint32_t>(p[1] & 0x3f) << 12) |
          (static_cast<uint32_t>(p[2] & 0x3f) << 6) |
          static_cast<uint32_t>(p[3] & 0x3f);
    *index += 4;
    return true;
  }
  *cp = p[0];
  *index += 1;
  return true;
}

void push_quad(DrawItem* item, float x, float y, float w, float h, float u0,
               float v0, float u1, float v1) {
  const uint32_t base = static_cast<uint32_t>(item->vertices.size());
  item->vertices.push_back(Vertex{x, y, 0, u0, v0});
  item->vertices.push_back(Vertex{x + w, y, 0, u1, v0});
  item->vertices.push_back(Vertex{x + w, y + h, 0, u1, v1});
  item->vertices.push_back(Vertex{x, y + h, 0, u0, v1});
  item->indices.push_back(base);
  item->indices.push_back(base + 1);
  item->indices.push_back(base + 2);
  item->indices.push_back(base);
  item->indices.push_back(base + 2);
  item->indices.push_back(base + 3);
}

DrawItem mesh_item(const gis::TessMesh& mesh, DrawKind kind, uint32_t rgba,
                   float opacity) {
  DrawItem item;
  item.kind = kind;
  item.rgba = rgba;
  item.opacity = opacity;
  item.pixel_space = false;
  const size_t count = mesh.positions.size() / 3;
  item.vertices.resize(count);
  for (size_t i = 0; i < count; ++i) {
    item.vertices[i].x = mesh.positions[i * 3];
    item.vertices[i].y = mesh.positions[i * 3 + 1];
    item.vertices[i].z = mesh.positions[i * 3 + 2];
  }
  item.indices = mesh.indices;
  return item;
}

LineCap parse_cap(const std::string& cap) {
  if (cap == "round") {
    return LineCap::kRound;
  }
  if (cap == "square") {
    return LineCap::kSquare;
  }
  return LineCap::kButt;
}

LineJoin parse_join(const std::string& join) {
  if (join == "bevel") {
    return LineJoin::kBevel;
  }
  if (join == "round") {
    return LineJoin::kRound;
  }
  return LineJoin::kMiter;
}

LineTessOptions line_options(const gis::style::ResolvedPaint& paint,
                             double wupp) {
  LineTessOptions opts;
  opts.cap = parse_cap(paint.line_cap);
  opts.join = parse_join(paint.line_join);
  opts.pixel_width = paint.line_width;
  opts.world_units_per_pixel = wupp;
  // Default round fans are expensive; thin strokes use fewer wedges.
  if (paint.line_width <= 2.f) {
    opts.round_segments = 4;
  } else if (paint.line_width <= 4.f) {
    opts.round_segments = 6;
  }
  if (wupp > 0) {
    opts.dasharray.reserve(paint.line_dasharray.size());
    for (float dash : paint.line_dasharray) {
      opts.dasharray.push_back(static_cast<double>(dash) * wupp);
    }
  }
  return opts;
}

template <class Fn>
void for_each_line(const OGRGeometry* geom, Fn&& fn) {
  if (!geom || geom->IsEmpty()) {
    return;
  }
  OGRGeometry* raw = const_cast<OGRGeometry*>(geom);
  const OGRwkbGeometryType type = wkbFlatten(raw->getGeometryType());
  if (type == wkbLineString) {
    if (auto* line = dynamic_cast<OGRLineString*>(raw)) {
      fn(line);
    }
    return;
  }
  if (type == wkbPolygon) {
    auto* poly = dynamic_cast<OGRPolygon*>(raw);
    if (!poly) {
      return;
    }
    if (OGRLinearRing* ring = poly->getExteriorRing()) {
      fn(ring);
    }
    for (int i = 0; i < poly->getNumInteriorRings(); ++i) {
      if (OGRLinearRing* ring = poly->getInteriorRing(i)) {
        fn(ring);
      }
    }
    return;
  }
  if (auto* coll = dynamic_cast<OGRGeometryCollection*>(raw)) {
    for (int i = 0; i < coll->getNumGeometries(); ++i) {
      for_each_line(coll->getGeometryRef(i), fn);
    }
  }
}

const OGRLineString* first_line(const OGRGeometry* geom) {
  const OGRLineString* found = nullptr;
  for_each_line(geom, [&](const OGRLineString* line) {
    if (!found && line && line->getNumPoints() >= 2) {
      found = line;
    }
  });
  return found;
}

bool anchor_xy(const OGRGeometry* geom, double* x, double* y) {
  if (!geom || geom->IsEmpty()) {
    return false;
  }
  if (const auto* pt = dynamic_cast<const OGRPoint*>(geom)) {
    *x = pt->getX();
    *y = pt->getY();
    return true;
  }
  if (const auto* multi = dynamic_cast<const OGRMultiPoint*>(geom)) {
    if (multi->getNumGeometries() > 0) {
      return anchor_xy(multi->getGeometryRef(0), x, y);
    }
  }
  OGREnvelope env;
  geom->getEnvelope(&env);
  *x = (env.MinX + env.MaxX) * 0.5;
  *y = (env.MinY + env.MaxY) * 0.5;
  return true;
}

void emit_circle(DrawItem* item, double cx, double cy, double radius) {
  item->vertices.resize(static_cast<size_t>(kCircleSegments) + 1);
  item->vertices[0] = Vertex{static_cast<float>(cx), static_cast<float>(cy), 0,
                             0, 0};
  for (int i = 0; i < kCircleSegments; ++i) {
    const float a =
        (2.f * kPi * static_cast<float>(i)) / static_cast<float>(kCircleSegments);
    item->vertices[static_cast<size_t>(i) + 1] = Vertex{
        static_cast<float>(cx + radius * std::cos(a)),
        static_cast<float>(cy + radius * std::sin(a)), 0, 0, 0};
  }
  item->indices.reserve(static_cast<size_t>(kCircleSegments) * 3);
  for (int i = 0; i < kCircleSegments; ++i) {
    const int next = (i + 1) % kCircleSegments;
    item->indices.push_back(0);
    item->indices.push_back(static_cast<uint32_t>(i + 1));
    item->indices.push_back(static_cast<uint32_t>(next + 1));
  }
}

struct TextAnchorOff {
  float x = 0;
  float y = 0;
};

TextAnchorOff text_origin(const std::string& anchor, float total_w,
                          float text_h) {
  TextAnchorOff o;
  const bool left = anchor.find("left") != std::string::npos;
  const bool right = anchor.find("right") != std::string::npos;
  const bool top = anchor.find("top") != std::string::npos;
  const bool bottom = anchor.find("bottom") != std::string::npos;
  if (left) {
    o.x = 0;
  } else if (right) {
    o.x = -total_w;
  } else {
    o.x = -total_w * 0.5f;
  }
  if (top) {
    o.y = 0;
  } else if (bottom) {
    o.y = -text_h;
  } else {
    o.y = -text_h * 0.5f;
  }
  return o;
}

float run_width(const std::string& text, float text_size,
                const GlyphMetrics* metrics) {
  float width = 0;
  size_t index = 0;
  uint32_t cp = 0;
  while (next_codepoint(text, &index, &cp)) {
    width += metrics->advance_px(cp, text_size);
  }
  return width;
}

detail::LabelBox icon_box(float left, float top, float right, float bottom,
                          int priority, float angle_deg) {
  detail::LabelBox box;
  box.left = static_cast<int>(std::floor(left));
  box.top = static_cast<int>(std::floor(top));
  box.right = static_cast<int>(std::ceil(right));
  box.bottom = static_cast<int>(std::ceil(bottom));
  box.priority = priority;
  if (angle_deg == 0.f) {
    return box;
  }
  const float rad = angle_deg * kPi / 180.f;
  const float cos_a = std::fabs(std::cos(rad));
  const float sin_a = std::fabs(std::sin(rad));
  const float w = static_cast<float>(box.right - box.left);
  const float h = static_cast<float>(box.bottom - box.top);
  const float rot_w = w * cos_a + h * sin_a;
  const float rot_h = w * sin_a + h * cos_a;
  const float cx = (static_cast<float>(box.left) + static_cast<float>(box.right)) * 0.5f;
  const float cy = (static_cast<float>(box.top) + static_cast<float>(box.bottom)) * 0.5f;
  detail::LabelBox rot;
  rot.left = static_cast<int>(std::floor(cx - rot_w * 0.5f));
  rot.top = static_cast<int>(std::floor(cy - rot_h * 0.5f));
  rot.right = static_cast<int>(std::ceil(cx + rot_w * 0.5f));
  rot.bottom = static_cast<int>(std::ceil(cy + rot_h * 0.5f));
  rot.priority = priority;
  return rot;
}

detail::LabelBox union_box(const detail::LabelBox& a, const detail::LabelBox& b) {
  detail::LabelBox u = a;
  u.left = (std::min)(a.left, b.left);
  u.top = (std::min)(a.top, b.top);
  u.right = (std::max)(a.right, b.right);
  u.bottom = (std::max)(a.bottom, b.bottom);
  return u;
}

struct SymbolCand {
  int priority = 5;
  int anchor_x = 0;
  int anchor_y = 0;
  float angle_deg = 0.f;
  std::string text;
  std::string text_anchor;
  float text_size = 16.f;
  float halo_width = 0.f;
  uint32_t halo_rgba = 0;
  const SymbolAsset* icon = nullptr;
  float icon_size = 1.f;
  float icon_off_x = 0.f;
  float icon_off_y = 0.f;
  bool draw_text = false;
};

int collision_text_height(const SymbolCand& cand, float fblc) {
  const int carto_h = detail::label_px(cand.priority, fblc);
  const int styled_h = static_cast<int>(std::ceil(cand.text_size));
  return (std::max)(carto_h, styled_h);
}

detail::LabelBox collision_text_box(const SymbolCand& cand, float fblc,
                                    const GlyphMetrics* metrics) {
  const int px_h = collision_text_height(cand, fblc);
  const float text_h = (std::max)(cand.text_size, static_cast<float>(px_h));
  // Prefer metrics advance so the collision box matches emit_kept_symbol.
  // Estimating with utf8_units*(3/5) under-sizes CJK runs and lets labels
  // stack on the china country frame.
  const float units_floor =
      static_cast<float>(detail::utf8_units(cand.text.c_str())) * text_h * 0.95f;
  float total_w = units_floor;
  if (metrics) {
    total_w = (std::max)(run_width(cand.text, text_h, metrics), units_floor);
  }
  const TextAnchorOff origin = text_origin(cand.text_anchor, total_w, text_h);
  const int bx =
      cand.anchor_x + static_cast<int>(std::floor(origin.x));
  const int by =
      cand.anchor_y + static_cast<int>(std::floor(origin.y));
  const int pad = 8;
  detail::LabelBox box;
  box.left = bx - pad;
  box.top = by - pad;
  box.right = bx + static_cast<int>(std::ceil(total_w)) + pad;
  box.bottom = by + static_cast<int>(std::ceil(text_h)) + pad;
  box.priority = cand.priority;
  if (cand.angle_deg != 0.f) {
    box = detail::label_box_rotated(bx, by, cand.text.c_str(), px_h,
                                    cand.priority, cand.angle_deg);
    // Widen rotated estimate to at least the metrics run width.
    const int need_w = static_cast<int>(std::ceil(total_w)) + pad * 2;
    const int have_w = box.right - box.left;
    if (have_w < need_w) {
      const int grow = (need_w - have_w + 1) / 2;
      box.left -= grow;
      box.right += grow;
    }
  }
  if (cand.halo_width > 0.f) {
    detail::expand_label_box_for_halo(&box, cand.halo_width);
  }
  return box;
}

void emit_kept_symbol(const SymbolCand& cand, const GlyphMetrics* metrics,
                      std::vector<DrawItem>* items) {
  const float ax = static_cast<float>(cand.anchor_x);
  const float ay = static_cast<float>(cand.anchor_y);
  const float angle_rad = cand.angle_deg * kPi / 180.f;
  if (cand.icon && cand.icon->width_px > 0.f && cand.icon->height_px > 0.f &&
      cand.icon_size > 0.f) {
    DrawItem icon;
    icon.kind = DrawKind::kIcon;
    icon.pixel_space = true;
    icon.symbol_id = cand.icon->id;
    icon.rgba = 0xffffffffu;
    icon.opacity = 1.f;
    icon.angle_rad = angle_rad;
    icon.anchor_x = ax;
    icon.anchor_y = ay;
    const float w = cand.icon->width_px * cand.icon_size;
    const float h = cand.icon->height_px * cand.icon_size;
    const float ox = cand.icon_off_x * cand.icon_size;
    const float oy = cand.icon_off_y * cand.icon_size;
    push_quad(&icon, ax + ox - w * 0.5f, ay + oy - h * 0.5f, w, h, 0.f, 0.f, 1.f,
              1.f);
    items->push_back(std::move(icon));
  }
  if (!cand.draw_text || !metrics || cand.text.empty() || cand.text_size <= 0.f) {
    return;
  }
  const float total_w = run_width(cand.text, cand.text_size, metrics);
  const TextAnchorOff origin =
      text_origin(cand.text_anchor, total_w, cand.text_size);
  float pen = 0.f;
  size_t index = 0;
  uint32_t cp = 0;
  while (next_codepoint(cand.text, &index, &cp)) {
    const float adv = metrics->advance_px(cp, cand.text_size);
    if (adv > 0.f) {
      DrawItem glyph;
      glyph.kind = DrawKind::kText;
      glyph.pixel_space = true;
      glyph.codepoint = cp;
      glyph.text_size_px = cand.text_size;
      glyph.halo_width_px = cand.halo_width;
      glyph.halo_rgba = cand.halo_rgba;
      glyph.rgba = 0xff000000u;
      glyph.opacity = 1.f;
      glyph.angle_rad = angle_rad;
      glyph.anchor_x = ax;
      glyph.anchor_y = ay;
      push_quad(&glyph, ax + origin.x + pen, ay + origin.y, adv, cand.text_size,
                0.f, 0.f, 0.f, 0.f);
      items->push_back(std::move(glyph));
    }
    pen += adv;
  }
}

void apply_background(const gis::style::StyleLayer& layer, double zoom,
                      MapFrame* frame) {
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, zoom, &paint);
  frame->background_rgba = paint.background_color;
  frame->background_opacity = paint.background_opacity;
}

void emit_raster(const gis::style::StyleLayer& layer, const LayoutInput& in,
                 MapFrame* frame) {
  gis::style::ResolvedPaint paint;
  gis::style::fill_resolved_paint(layer, nullptr, {}, in.zoom, &paint);
  for (const TileSlot& tile : in.tiles) {
    DrawItem item;
    item.kind = DrawKind::kRaster;
    item.pixel_space = false;
    item.opacity = tile.opacity * paint.raster_opacity;
    item.rgba = 0xffffffffu;
    item.codepoint = tile.texture_key;
    const float x0 = static_cast<float>(tile.min_x);
    const float x1 = static_cast<float>(tile.max_x);
    const float y0 = static_cast<float>(tile.min_y);
    const float y1 = static_cast<float>(tile.max_y);
    item.vertices.push_back(Vertex{x0, y1, 0, 0, 0});
    item.vertices.push_back(Vertex{x1, y1, 0, 1, 0});
    item.vertices.push_back(Vertex{x1, y0, 0, 1, 1});
    item.vertices.push_back(Vertex{x0, y0, 0, 0, 1});
    item.indices = {0, 1, 2, 0, 2, 3};
    frame->items.push_back(std::move(item));
  }
}

struct FillJob {
  size_t layer_ord = 0;
  const OGRGeometry* geom = nullptr;
  gis::style::ResolvedPaint paint;
  bool pattern = false;
};

// Emit one or more consecutive fill layers. Jobs from all layers share one
// parallel_for; results are appended in layer order (painter z).
void emit_fills(const std::vector<const gis::style::StyleLayer*>& fill_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapFrame* frame) {
  if (fill_layers.empty()) {
    return;
  }
  std::vector<FillJob> jobs;
  for (size_t li = 0; li < fill_layers.size(); ++li) {
    const gis::style::StyleLayer& layer = *fill_layers[li];
    for (const LayerBatch& batch : layers) {
      if (!layer_uses_batch(layer, batch)) {
        continue;
      }
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        const OGRGeometry* geom = batch.geoms[i];
        if (!geom) {
          continue;
        }
        const gis::style::AttrMap attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
        gis::style::ResolvedPaint paint;
        gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
        jobs.push_back(FillJob{li, geom, paint,
                               find_symbol(in, paint.fill_pattern) != nullptr});
      }
    }
  }
  if (jobs.empty()) {
    return;
  }
  FillTessOptions fill_opts;
  fill_opts.world_units_per_pixel = wupp;
  auto to_item = [fill_opts](const FillJob& job) -> std::pair<DrawItem, bool> {
    gis::TessMesh mesh;
    if (!gis::tessellate_geometry(job.geom, fill_opts, mesh) ||
        mesh.indices.empty()) {
      return {{}, false};
    }
    DrawItem item = mesh_item(mesh, DrawKind::kFill, job.paint.fill_color,
                              job.paint.fill_opacity);
    if (job.pattern) {
      item.symbol_id = job.paint.fill_pattern;
    }
    return {std::move(item), true};
  };
  if (jobs.size() < kParallelTessMinGeoms) {
    for (const FillJob& job : jobs) {
      auto [item, ok] = to_item(job);
      if (ok) {
        frame->items.push_back(std::move(item));
      }
    }
    return;
  }
  std::vector<DrawItem> items(jobs.size());
  std::vector<char> valid(jobs.size(), 0);
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, size_t{0}, jobs.size(),
      [&](size_t i) {
        auto [item, ok] = to_item(jobs[i]);
        if (!ok) {
          return;
        }
        items[i] = std::move(item);
        valid[i] = 1;
      },
      kParallelTessGrain);
  // Preserve layer order even if jobs were interleaved in the vector by layer.
  for (size_t li = 0; li < fill_layers.size(); ++li) {
    for (size_t i = 0; i < jobs.size(); ++i) {
      if (valid[i] && jobs[i].layer_ord == li) {
        frame->items.push_back(std::move(items[i]));
      }
    }
  }
}

void emit_fill(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapFrame* frame) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_fills(one, in, layers, wupp, frame);
}

struct LineJob {
  size_t layer_ord = 0;
  const OGRLineString* line = nullptr;
  LineTessOptions opts;
  uint32_t rgba = 0;
  float opacity = 1.f;
};

void emit_lines(const std::vector<const gis::style::StyleLayer*>& line_layers,
                const LayoutInput& in, const std::vector<LayerBatch>& layers,
                double wupp, MapFrame* frame) {
  if (line_layers.empty()) {
    return;
  }
  std::vector<LineJob> jobs;
  for (size_t li = 0; li < line_layers.size(); ++li) {
    const gis::style::StyleLayer& layer = *line_layers[li];
    for (const LayerBatch& batch : layers) {
      if (!layer_uses_batch(layer, batch)) {
        continue;
      }
      for (size_t i = 0; i < batch.geoms.size(); ++i) {
        const OGRGeometry* geom = batch.geoms[i];
        if (!geom) {
          continue;
        }
        const gis::style::AttrMap attrs = attrs_at(batch, i);
        if (!gis::style::eval_filter(layer.filter, attrs)) {
          continue;
        }
        gis::style::ResolvedPaint paint;
        gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
        const LineTessOptions opts = line_options(paint, wupp);
        for_each_line(geom, [&](const OGRLineString* line) {
          jobs.push_back(LineJob{li, line, opts, paint.line_color,
                                 paint.line_opacity});
        });
      }
    }
  }
  if (jobs.empty()) {
    return;
  }
  if (jobs.size() < kParallelTessMinGeoms) {
    for (const LineJob& job : jobs) {
      gis::TessMesh mesh;
      if (!gis::tessellate_line(job.line, job.opts, mesh) ||
          mesh.indices.empty()) {
        continue;
      }
      frame->items.push_back(
          mesh_item(mesh, DrawKind::kLine, job.rgba, job.opacity));
    }
    return;
  }
  std::vector<DrawItem> items(jobs.size());
  std::vector<char> valid(jobs.size(), 0);
  base::execution::GlobalNThreadPoolExecutor executor;
  base::execution::parallel_for(
      executor, size_t{0}, jobs.size(),
      [&](size_t i) {
        gis::TessMesh mesh;
        if (!gis::tessellate_line(jobs[i].line, jobs[i].opts, mesh) ||
            mesh.indices.empty()) {
          return;
        }
        items[i] =
            mesh_item(mesh, DrawKind::kLine, jobs[i].rgba, jobs[i].opacity);
        valid[i] = 1;
      },
      kParallelTessGrain);
  for (size_t li = 0; li < line_layers.size(); ++li) {
    for (size_t i = 0; i < jobs.size(); ++i) {
      if (valid[i] && jobs[i].layer_ord == li) {
        frame->items.push_back(std::move(items[i]));
      }
    }
  }
}

void emit_line(const gis::style::StyleLayer& layer, const LayoutInput& in,
               const std::vector<LayerBatch>& layers, double wupp,
               MapFrame* frame) {
  std::vector<const gis::style::StyleLayer*> one{&layer};
  emit_lines(one, in, layers, wupp, frame);
}

void emit_circles(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, double wupp,
                  MapFrame* frame) {
  if (wupp <= 0) {
    return;
  }
  for (const LayerBatch& batch : layers) {
    if (!layer_uses_batch(layer, batch)) {
      continue;
    }
    for (size_t i = 0; i < batch.geoms.size(); ++i) {
      const OGRGeometry* geom = batch.geoms[i];
      double x = 0;
      double y = 0;
      if (!anchor_xy(geom, &x, &y)) {
        continue;
      }
      const gis::style::AttrMap attrs = attrs_at(batch, i);
      if (!gis::style::eval_filter(layer.filter, attrs)) {
        continue;
      }
      gis::style::ResolvedPaint paint;
      gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
      const double radius = static_cast<double>(paint.circle_radius) * wupp;
      if (radius <= 0) {
        continue;
      }
      DrawItem item;
      item.kind = DrawKind::kCircle;
      item.pixel_space = false;
      item.rgba = paint.circle_color;
      item.opacity = paint.circle_opacity;
      emit_circle(&item, x, y, radius);
      frame->items.push_back(std::move(item));
    }
  }
}

void emit_symbols(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, float fblc,
                  detail::LabelGrid* grid, MapFrame* frame) {
  if (!screen_ready(in.view)) {
    return;
  }
  const bool along_line = [&] {
    gis::style::ResolvedPaint paint;
    gis::style::fill_resolved_paint(layer, nullptr, {}, in.zoom, &paint);
    return paint.symbol_placement == "line";
  }();

  std::vector<SymbolCand> cands;
  for (const LayerBatch& batch : layers) {
    if (!layer_uses_batch(layer, batch)) {
      continue;
    }
    for (size_t i = 0; i < batch.geoms.size(); ++i) {
      const OGRGeometry* geom = batch.geoms[i];
      const gis::style::AttrMap attrs = attrs_at(batch, i);
      if (!gis::style::eval_filter(layer.filter, attrs)) {
        continue;
      }
      gis::style::ResolvedPaint paint;
      gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
      SymbolCand cand;
      cand.priority = detail::label_priority(attr_cstr(attrs, "name"),
                                             attr_cstr(attrs, "kind"),
                                             class_cstr(attrs),
                                             attr_cstr(attrs, "adcode"));
      cand.text = label_text(paint, attrs);
      cand.text_anchor = paint.text_anchor.empty() ? "center" : paint.text_anchor;
      cand.text_size = paint.text_size;
      cand.icon = find_symbol(in, paint.icon_image);
      cand.icon_size = paint.icon_size;
      cand.icon_off_x = paint.icon_offset_x;
      cand.icon_off_y = paint.icon_offset_y;
      cand.draw_text = in.metrics && !cand.text.empty();
      if (layer.paint.find("text-halo-width") != layer.paint.end()) {
        cand.halo_width = paint.text_halo_width;
      } else if (cand.draw_text) {
        cand.halo_width = static_cast<float>(detail::halo_px(cand.priority));
      }
      if (layer.paint.find("text-halo-color") != layer.paint.end()) {
        cand.halo_rgba = paint.text_halo_color;
      } else if (cand.halo_width > 0.f) {
        cand.halo_rgba = 0xffffffffu;
      }
      double wx = 0;
      double wy = 0;
      float angle_deg = 0.f;
      bool placed = false;
      if (along_line) {
        if (const OGRLineString* line = first_line(geom)) {
          std::vector<int> xy;
          xy.reserve(static_cast<size_t>(line->getNumPoints()) * 2);
          for (int p = 0; p < line->getNumPoints(); ++p) {
            const ScreenPt s = to_screen(in.view, line->getX(p), line->getY(p));
            xy.push_back(static_cast<int>(std::lround(s.x)));
            xy.push_back(static_cast<int>(std::lround(s.y)));
          }
          detail::LineLabelPose pose;
          if (detail::line_label_pose(xy.data(), line->getNumPoints(), &pose)) {
            cand.anchor_x = pose.x;
            cand.anchor_y = pose.y;
            angle_deg = pose.angle_deg;
            placed = true;
          }
        }
      }
      if (!placed) {
        if (!anchor_xy(geom, &wx, &wy)) {
          continue;
        }
        const ScreenPt s = to_screen(in.view, wx, wy);
        cand.anchor_x = static_cast<int>(std::lround(s.x));
        cand.anchor_y = static_cast<int>(std::lround(s.y));
      }
      cand.angle_deg = angle_deg;
      const bool icon_ok = cand.icon && cand.icon->width_px > 0.f &&
                           cand.icon->height_px > 0.f && cand.icon_size > 0.f;
      if (!cand.draw_text && !icon_ok) {
        continue;
      }
      cands.push_back(std::move(cand));
    }
  }

  std::stable_sort(cands.begin(), cands.end(),
                   [](const SymbolCand& a, const SymbolCand& b) {
                     return a.priority < b.priority;
                   });

  for (const SymbolCand& cand : cands) {
    const bool icon_ok = cand.icon && cand.icon->width_px > 0.f &&
                         cand.icon->height_px > 0.f && cand.icon_size > 0.f;
    bool have_box = false;
    detail::LabelBox box;
    if (cand.draw_text) {
      box = collision_text_box(cand, fblc, in.metrics);
      have_box = true;
    }
    if (icon_ok) {
      const float w = cand.icon->width_px * cand.icon_size;
      const float h = cand.icon->height_px * cand.icon_size;
      const float cx = static_cast<float>(cand.anchor_x) +
                       cand.icon_off_x * cand.icon_size;
      const float cy = static_cast<float>(cand.anchor_y) +
                       cand.icon_off_y * cand.icon_size;
      const detail::LabelBox ib =
          icon_box(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f,
                   cand.priority, cand.angle_deg);
      box = have_box ? union_box(box, ib) : ib;
      have_box = true;
    }
    if (!have_box || !grid->try_keep(box)) {
      continue;
    }
    emit_kept_symbol(cand, in.metrics, &frame->items);
  }
}

}  // namespace

MapFrame Layout::build(const LayoutInput& in,
                       const std::vector<LayerBatch>& layers) const {
  // Reset TLS hybrid scratch for this build so label/token temps reuse
  // arena blocks instead of churning the process heap across frames.
  if (base::MemoryResource* tls = base::tls_memory_resource()) {
    tls->clear(base::Arena::kInitialSize);
  }
  if (base::trace::tracing_enabled()) {
    gis::reset_tess_trace_stats();
  }
  MapFrame frame;
  if (!in.style) {
    return frame;
  }
  const double wupp = world_units_per_pixel(in.view);
  const float fblc = device_px_per_world(in.view);
  // Constructed here so reset() (inline) uses this TU's LabelGrid layout.
  detail::LabelGrid grid;
  if (screen_ready(in.view)) {
    grid.reset(fblc, static_cast<int>(in.view.width_px),
               static_cast<int>(in.view.height_px));
  }

  std::vector<const gis::style::StyleLayer*> pending_fills;
  std::vector<const gis::style::StyleLayer*> pending_lines;
  auto flush_fills = [&]() {
    if (pending_fills.empty()) {
      return;
    }
    BASE_TRACE_EVENT("emit_fill", "map2d.layout");
    emit_fills(pending_fills, in, layers, wupp, &frame);
    pending_fills.clear();
  };
  auto flush_lines = [&]() {
    if (pending_lines.empty()) {
      return;
    }
    BASE_TRACE_EVENT("emit_line", "map2d.layout");
    emit_lines(pending_lines, in, layers, wupp, &frame);
    pending_lines.clear();
  };

  for (const gis::style::StyleLayer& layer : in.style->layers) {
    if (!gis::style::layer_matches_zoom(layer, in.zoom)) {
      continue;
    }
    switch (layer.type) {
      case gis::style::LayerType::kBackground:
        flush_fills();
        flush_lines();
        apply_background(layer, in.zoom, &frame);
        break;
      case gis::style::LayerType::kRaster:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_raster", "map2d.layout");
          emit_raster(layer, in, &frame);
        }
        break;
      case gis::style::LayerType::kFill:
        flush_lines();
        pending_fills.push_back(&layer);
        break;
      case gis::style::LayerType::kLine:
        flush_fills();
        pending_lines.push_back(&layer);
        break;
      case gis::style::LayerType::kCircle:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_circle", "map2d.layout");
          emit_circles(layer, in, layers, wupp, &frame);
        }
        break;
      case gis::style::LayerType::kSymbol:
        flush_fills();
        flush_lines();
        {
          BASE_TRACE_EVENT("emit_symbol", "map2d.layout");
          emit_symbols(layer, in, layers, fblc, &grid, &frame);
        }
        break;
      default:
        flush_fills();
        flush_lines();
        break;
    }
  }
  flush_fills();
  flush_lines();
  if (base::trace::tracing_enabled()) {
    gis::flush_tess_trace_stats();
  }
  return frame;
}

}  // namespace vista
}  // namespace gis
