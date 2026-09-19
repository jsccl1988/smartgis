// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/map_edit_session.h"

#include <cstdint>

#include "ogrsf_frmts.h"
#include "gis/map/map.h"

namespace gis {
namespace {

content::FeatureId pack_feature_id(GIntBig id) {
  content::FeatureId out{};
  out.len = 4;
  const uint32_t n = static_cast<uint32_t>(id);
  out.bytes[0] = static_cast<uint8_t>(n);
  out.bytes[1] = static_cast<uint8_t>(n >> 8);
  out.bytes[2] = static_cast<uint8_t>(n >> 16);
  out.bytes[3] = static_cast<uint8_t>(n >> 24);
  return out;
}

}  // namespace

MapEditSession::MapEditSession(gis::SmtMap* map)
    : CommandEditSession([this](const FeatureMutation& m, bool undo) {
        return apply_map(m, undo);
      }),
      map_(map) {}

void MapEditSession::bind_map(gis::SmtMap* map) { map_ = map; }

bool MapEditSession::commit_feature(EditOp op, OGRFeature* feature) {
  if (!map_ || !feature) {
    return false;
  }
  FeatureMutation mutation;
  mutation.op = op;
  mutation.id = pack_feature_id(feature->GetFID());
  if (mutation.id.len == 0) {
    mutation.id.len = 1;
    mutation.id.bytes[0] = 1;
  }
  mutation.leftover = feature;
  return commit(mutation);
}

bool MapEditSession::apply_map(const FeatureMutation& mutation, bool undo) {
  auto* feature = static_cast<OGRFeature*>(mutation.leftover);
  if (!map_ || !feature) {
    return false;
  }
  switch (mutation.op) {
    case EditOp::kAppend:
      return undo ? map_->DeleteFeature(feature) : map_->AppendFeature(feature);
    case EditOp::kDelete:
      return undo ? map_->AppendFeature(feature) : map_->DeleteFeature(feature);
    case EditOp::kModify:
      return map_->UpdateFeature(feature);
  }
  return false;
}

}  // namespace gis
