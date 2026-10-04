// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SCENIC_RHI2D_MAP_FEATURE_PREP_H_
#define SCENIC_RHI2D_MAP_FEATURE_PREP_H_

#include <cstdint>
#include <vector>

#include "gis/envelope.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/style/style_pod.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/frame/context.h"
#include "ogrsf_frmts.h"

namespace scenic {
namespace detail {

enum class PrepKind : uint8_t { Skip, Fallback, Polygon, Line, Point, Anno };

struct PrepPart {
  std::vector<POINT> pts;
  std::vector<int> counts;  // ring counts (poly) or single polyline length
};

// CPU-prepped carto feature: device POINT parts plus stroke/fill identity
// used by draw_prepared_batch to coalesce PolyPolyline / polygon runs.
struct PreparedFeature {
  PrepKind kind = PrepKind::Skip;
  Style style;
  int feature_type = 0;
  int label_priority = 5;
  bool is_river = false;
  int road_class = 0;
  float anno_angle = 0.f;
  char anno[2000]{};
  std::vector<PrepPart> parts;

  // Ordinary lines (no road dual-pen, no river line label) share one
  // PolyPolyline under one stroke.
  bool is_plain_line() const {
    return kind == PrepKind::Line && road_class == 0 && !(is_river && anno[0]);
  }
  // Unlabeled roads of the same class share casing+fill PolyPolyline.
  bool is_plain_road() const {
    return kind == PrepKind::Line && road_class > 0 && !anno[0];
  }
  bool needs_style() const {
    return kind != PrepKind::Point && kind != PrepKind::Anno;
  }
  bool same_line_stroke(const PreparedFeature& o) const {
    const PenDesc& a = style.get_pen_desc();
    const PenDesc& b = o.style.get_pen_desc();
    return is_river == o.is_river && road_class == o.road_class &&
           style.get_style_type() == o.style.get_style_type() &&
           a.lPenColor == b.lPenColor && a.lPenStyle == b.lPenStyle &&
           a.fPenWidth == b.fPenWidth;
  }
  bool same_road_stroke(const PreparedFeature& o) const {
    return road_class == o.road_class;
  }
  bool same_polygon_fill(const PreparedFeature& o) const {
    const PenDesc& pa = style.get_pen_desc();
    const PenDesc& pb = o.style.get_pen_desc();
    const BrushDesc& ba = style.get_brush_desc();
    const BrushDesc& bb = o.style.get_brush_desc();
    return style.get_style_type() == o.style.get_style_type() &&
           pa.lPenColor == pb.lPenColor && pa.lPenStyle == pb.lPenStyle &&
           pa.fPenWidth == pb.fPenWidth && ba.brushTp == bb.brushTp &&
           ba.lBrushColor == bb.lBrushColor && ba.lBrushStyle == bb.lBrushStyle;
  }
};

// Per-layer OGR field indices (schema is stable across features).
struct PrepFieldCache {
  bool warmed = false;
  int style = -1;
  int kind = -1;
  int cls = -1;
  int fill = -1;
  int stroke = -1;
  int name = -1;
  int anno = -1;
  int text = -1;
  int adcode = -1;
  int angle = -1;
};

struct OgrLayerBatch {
  OGRLayer* layer = nullptr;
  bool culled = false;
  bool force_serial = false;
  PrepFieldCache fields;
  std::vector<OGRFeature*> feats;
  std::vector<PreparedFeature> prepared;
};

inline const char* prep_field_cstr(OGRFeature* feature, int index) {
  if (!feature || index < 0) {
    return "";
  }
  const char* v = feature->GetFieldAsString(index);
  return v ? v : "";
}

void warm_prep_fields(OGRFeature* feature, PrepFieldCache* cache);

void prepare_one_feature(OGRFeature* feature, const gis::Envelope& env_viewp,
                         const LpToDp2& xform, float fblc,
                         PrepFieldCache* fields, PreparedFeature* out);

void destroy_ogr_feats(std::vector<OGRFeature*>* feats);

bool force_serial_ogr_layer(OGRLayer* layer,
                            const std::vector<OGRFeature*>& feats);

}  // namespace detail
}  // namespace scenic

#endif  // SCENIC_RHI2D_MAP_FEATURE_PREP_H_
