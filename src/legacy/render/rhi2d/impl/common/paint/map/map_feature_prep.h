// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_RHI2D_MAP_FEATURE_PREP_H_
#define SMT_LEGACY_RENDER_RHI2D_MAP_FEATURE_PREP_H_

#include <cstdint>
#include <vector>

#include "gis/envelope.h"
#include "legacy/gis/present/carto/style.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/draw/device_geom.h"
#include "legacy/render/rhi2d/impl/common/paint/carto/frame/context.h"
#include "ogrsf_frmts.h"

namespace render {
namespace detail {

enum class PrepKind : uint8_t { Skip, Fallback, Polygon, Line, Point, Anno };

struct PrepPart {
  std::vector<POINT> pts;
  std::vector<int> counts;  // ring counts (poly) or single polyline length
};

struct PreparedFeature {
  PrepKind kind = PrepKind::Skip;
  SmtStyle style;
  int feature_type = 0;
  int label_priority = 5;
  bool is_river = false;
  int road_class = 0;
  float anno_angle = 0.f;
  char anno[2000]{};
  std::vector<PrepPart> parts;
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

void warm_prep_fields(OGRFeature* feature, PrepFieldCache* cache);

void prepare_one_feature(OGRFeature* feature, const gis::Envelope& env_viewp,
                         const LpToDp2& xform, float fblc,
                         PrepFieldCache* fields, PreparedFeature* out);

void destroy_ogr_feats(std::vector<OGRFeature*>* feats);

bool force_serial_ogr_layer(OGRLayer* layer,
                            const std::vector<OGRFeature*>& feats);

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_RHI2D_MAP_FEATURE_PREP_H_
