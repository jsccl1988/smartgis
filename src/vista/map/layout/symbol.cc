// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/map/layout/symbol.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <utility>
#include <vector>

#include "gis/style/paint_resolve.h"
#include "gis/style/eval/style_rules.h"
#include "gis/style/style_types.h"
#include "vista/map/layout/attrs.h"
#include "vista/map/layout/emit.h"
#include "vista/map/layout/geom_walk.h"
#include "vista/map/layout/mesh_emit.h"
#include "vista/map/layout/view_metrics.h"
#include "ogrsf_frmts.h"

namespace vista {
namespace detail {
namespace {

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

LabelBox icon_box(float left, float top, float right, float bottom,
                  int priority, float angle_deg) {
  LabelBox box;
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
  const float cx =
      (static_cast<float>(box.left) + static_cast<float>(box.right)) * 0.5f;
  const float cy =
      (static_cast<float>(box.top) + static_cast<float>(box.bottom)) * 0.5f;
  LabelBox rot;
  rot.left = static_cast<int>(std::floor(cx - rot_w * 0.5f));
  rot.top = static_cast<int>(std::floor(cy - rot_h * 0.5f));
  rot.right = static_cast<int>(std::ceil(cx + rot_w * 0.5f));
  rot.bottom = static_cast<int>(std::ceil(cy + rot_h * 0.5f));
  rot.priority = priority;
  return rot;
}

LabelBox union_box(const LabelBox& a, const LabelBox& b) {
  LabelBox u = a;
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
  // Screen polyline for symbol-placement:line multi-slot retries (empty = point).
  std::vector<int> line_xy;
};

int collision_text_height(const SymbolCand& cand, float fblc) {
  const int carto_h = label_px(cand.priority, fblc);
  const int styled_h = static_cast<int>(std::ceil(cand.text_size));
  return (std::max)(carto_h, styled_h);
}

LabelBox collision_text_box(const SymbolCand& cand, float fblc,
                            const GlyphMetrics* metrics) {
  const int px_h = collision_text_height(cand, fblc);
  const float text_h = (std::max)(cand.text_size, static_cast<float>(px_h));
  // Prefer metrics advance so the collision box matches emit_kept_symbol.
  // Bitmap floor uses mixed-script estimate when metrics are missing/narrow.
  const float units_floor = estimate_run_width_px(cand.text.c_str(), text_h);
  float total_w = units_floor;
  if (metrics) {
    total_w = (std::max)(run_width(cand.text, text_h, metrics), units_floor);
  }
  const TextAnchorOff origin = text_origin(cand.text_anchor, total_w, text_h);
  const int bx = cand.anchor_x + static_cast<int>(std::floor(origin.x));
  const int by = cand.anchor_y + static_cast<int>(std::floor(origin.y));
  // Extra pad so CJK halos / dense east-china city clusters reject overlaps.
  const int pad = 14;
  LabelBox box;
  box.left = bx - pad;
  box.top = by - pad;
  box.right = bx + static_cast<int>(std::ceil(total_w)) + pad;
  box.bottom = by + static_cast<int>(std::ceil(text_h)) + pad;
  box.priority = cand.priority;
  if (cand.angle_deg != 0.f) {
    // Rotate the metrics-accurate upright box (not the rough utf8 estimate).
    box = rotate_label_box(box, cand.angle_deg);
  }
  if (cand.halo_width > 0.f) {
    expand_label_box_for_halo(&box, cand.halo_width);
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

}  // namespace

void emit_symbols(const gis::style::StyleLayer& layer, const LayoutInput& in,
                  const std::vector<LayerBatch>& layers, float fblc,
                  LabelGrid* grid, MapIR* frame) {
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
      if (layout_gen_stale(in)) {
        return;
      }
      const OGRGeometry* geom = batch.geoms[i];
      const gis::style::AttrMap attrs = attrs_at(batch, i);
      if (!gis::style::eval_filter(layer.filter, attrs)) {
        continue;
      }
      gis::style::ResolvedPaint paint;
      gis::style::fill_resolved_paint(layer, nullptr, attrs, in.zoom, &paint);
      SymbolCand cand;
      cand.priority = label_priority(attr_cstr(attrs, "name"),
                                     attr_cstr(attrs, "kind"), class_cstr(attrs),
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
        cand.halo_width = static_cast<float>(halo_px(cand.priority));
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
          LineLabelPose pose;
          if (line_label_pose(xy.data(), line->getNumPoints(), &pose)) {
            cand.anchor_x = pose.x;
            cand.anchor_y = pose.y;
            angle_deg = pose.angle_deg;
            cand.line_xy = std::move(xy);
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
      // Drop along-line text that cannot physically fit on the path.
      if (!cand.line_xy.empty() && cand.draw_text) {
        const int n_pts = static_cast<int>(cand.line_xy.size() / 2);
        const float path_len = line_path_length_px(cand.line_xy.data(), n_pts);
        const float text_w =
            in.metrics ? run_width(cand.text, cand.text_size, in.metrics)
                       : estimate_run_width_px(cand.text.c_str(), cand.text_size);
        if (!line_fits_label(path_len, text_w)) {
          if (!icon_ok) {
            continue;
          }
          cand.draw_text = false;
        }
      }
      cands.push_back(std::move(cand));
    }
  }

  std::stable_sort(cands.begin(), cands.end(),
                   [](const SymbolCand& a, const SymbolCand& b) {
                     if (a.priority != b.priority) {
                       return a.priority < b.priority;
                     }
                     // Stable preference among tier-1 (priority 0): 鍖椾含 before
                     // 澶╂触 so overview framing keeps the capital.
                     auto tier1_rank = [](const std::string& n) -> int {
                       static constexpr const char* kOrder[] = {
                           "\xe5\x8c\x97\xe4\xba\xac",  // 鍖椾含
                           "\xe4\xb8\x8a\xe6\xb5\xb7",  // 涓婃捣
                           "\xe5\xb9\xbf\xe5\xb7\x9e",  // 骞垮窞
                           "\xe6\xb7\xb1\xe5\x9c\xb3",  // 娣卞湷
                           "\xe9\x87\x8d\xe5\xba\x86",  // 閲嶅簡
                           "\xe5\xa4\xa9\xe6\xb4\xa5",  // 澶╂触
                       };
                       for (int i = 0; i < 6; ++i) {
                         if (n.find(kOrder[i]) != std::string::npos) {
                           return i;
                         }
                       }
                       return 100;
                     };
                     return tier1_rank(a.text) < tier1_rank(b.text);
                   });

  auto build_symbol_box = [&](const SymbolCand& cand) -> LabelBox {
    const bool icon_ok = cand.icon && cand.icon->width_px > 0.f &&
                         cand.icon->height_px > 0.f && cand.icon_size > 0.f;
    LabelBox box;
    bool have_box = false;
    if (cand.draw_text) {
      box = collision_text_box(cand, fblc, in.metrics);
      have_box = true;
    }
    if (icon_ok) {
      const float w = cand.icon->width_px * cand.icon_size;
      const float h = cand.icon->height_px * cand.icon_size;
      const float cx =
          static_cast<float>(cand.anchor_x) + cand.icon_off_x * cand.icon_size;
      const float cy =
          static_cast<float>(cand.anchor_y) + cand.icon_off_y * cand.icon_size;
      const LabelBox ib =
          icon_box(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f,
                   cand.priority, cand.angle_deg);
      box = have_box ? union_box(box, ib) : ib;
      have_box = true;
    }
    return have_box ? box : LabelBox{};
  };

  for (SymbolCand& cand : cands) {
    const bool icon_ok = cand.icon && cand.icon->width_px > 0.f &&
                         cand.icon->height_px > 0.f && cand.icon_size > 0.f;
    if (!cand.draw_text && !icon_ok) {
      continue;
    }

    bool kept = false;
    if (!cand.line_xy.empty()) {
      float slots[8];
      const int n_slots = line_label_slot_fractions(8, slots);
      const int n_pts = static_cast<int>(cand.line_xy.size() / 2);
      for (int si = 0; si < n_slots; ++si) {
        LineLabelPose pose;
        if (!line_label_pose_at(cand.line_xy.data(), n_pts, slots[si], &pose)) {
          continue;
        }
        cand.anchor_x = pose.x;
        cand.anchor_y = pose.y;
        cand.angle_deg = pose.angle_deg;
        const LabelBox box = build_symbol_box(cand);
        if (box.right <= box.left || box.bottom <= box.top) {
          continue;
        }
        if (grid->try_keep(box)) {
          kept = true;
          break;
        }
      }
    } else {
      // Point labels: eight screen nudges so a name can clear a neighbour.
      const LabelBox probe = build_symbol_box(cand);
      const int text_h =
          probe.bottom > probe.top ? (probe.bottom - probe.top) : 18;
      PointNudge nudges[8];
      const int n_nudge = point_label_nudges(text_h, nudges, 8);
      const int ox0 = cand.anchor_x;
      const int oy0 = cand.anchor_y;
      for (int oi = 0; oi < n_nudge; ++oi) {
        cand.anchor_x = ox0 + nudges[oi].dx;
        cand.anchor_y = oy0 + nudges[oi].dy;
        const LabelBox box = build_symbol_box(cand);
        if (box.right > box.left && box.bottom > box.top &&
            grid->try_keep(box)) {
          kept = true;
          break;
        }
      }
      if (!kept) {
        cand.anchor_x = ox0;
        cand.anchor_y = oy0;
      }
    }
    if (!kept) {
      continue;
    }
    emit_kept_symbol(cand, in.metrics, &frame->items);
  }
}

}  // namespace detail
}  // namespace vista
