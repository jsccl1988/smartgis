// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/model/edit/map_edit/map_edit_session.h"

#include <cstdint>
#include <map>

#include "ogrsf_frmts.h"
#include "gis/model/map/map.h"

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

struct MapEditSession::Impl {
  uint64_t next_token = 1;
  std::map<uint64_t, OGRFeature*> features;
};

MapEditSession::MapEditSession(gis::SmtMap* map)
    : CommandEditSession([this](const FeatureMutation& m, bool undo) {
        return apply_map(m, undo);
      }),
      impl_(std::make_unique<Impl>()),
      map_(map) {}

MapEditSession::~MapEditSession() = default;

void MapEditSession::bind_map(gis::SmtMap* map) { map_ = map; }

bool MapEditSession::commit_feature(EditOp op, OGRFeature* feature) {
  if (!map_ || !feature || !impl_) {
    return false;
  }
  FeatureMutation mutation;
  mutation.op = op;
  mutation.id = pack_feature_id(feature->GetFID());
  if (mutation.id.len == 0) {
    mutation.id.len = 1;
    mutation.id.bytes[0] = 1;
  }
  const uint64_t token = impl_->next_token++;
  mutation.host_token = token;
  impl_->features[token] = feature;
  if (!commit(mutation)) {
    impl_->features.erase(token);
    return false;
  }
  return true;
}

bool MapEditSession::apply_map(const FeatureMutation& mutation, bool undo) {
  if (!map_ || !impl_) {
    return false;
  }
  const auto it = impl_->features.find(mutation.host_token);
  if (it == impl_->features.end() || !it->second) {
    return false;
  }
  OGRFeature* feature = it->second;
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
