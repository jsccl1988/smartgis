// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/map_session.h"

#include <cstdint>
#include <map>

#include "gis/map/map.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace {

FeatureId pack_feature_id(GIntBig id) {
  FeatureId out{};
  out.len = 4;
  const uint32_t n = static_cast<uint32_t>(id);
  out.bytes[0] = static_cast<uint8_t>(n);
  out.bytes[1] = static_cast<uint8_t>(n >> 8);
  out.bytes[2] = static_cast<uint8_t>(n >> 16);
  out.bytes[3] = static_cast<uint8_t>(n >> 24);
  return out;
}

OGRGeometry* ogr_from_geom(const FeatureGeom& geom) {
  if (geom.empty()) {
    return nullptr;
  }
  switch (geom.kind) {
    case FeatureGeom::Kind::kPoint: {
      return new OGRPoint(geom.points[0].x, geom.points[0].y);
    }
    case FeatureGeom::Kind::kLineString: {
      OGRLineString* line = new OGRLineString();
      for (const MapVertex& v : geom.points) {
        line->addPoint(v.x, v.y);
      }
      return line;
    }
    case FeatureGeom::Kind::kPolygon: {
      OGRLinearRing* ring = new OGRLinearRing();
      for (const MapVertex& v : geom.points) {
        ring->addPoint(v.x, v.y);
      }
      if (geom.points.size() >= 1) {
        const MapVertex& first = geom.points.front();
        const MapVertex& last = geom.points.back();
        if (first.x != last.x || first.y != last.y) {
          ring->addPoint(first.x, first.y);
        }
      }
      ring->closeRings();
      OGRPolygon* poly = new OGRPolygon();
      poly->addRingDirectly(ring);
      return poly;
    }
    default:
      return nullptr;
  }
}

}  // namespace

struct MapEditSession::Impl {
  uint64_t next_token = 1;
  std::map<uint64_t, OGRFeature*> features;
  TopologyLayerState topology;
  TopologyValidationResult last_topology;
  double topology_gap_tolerance = 1e-6;
};

MapEditSession::MapEditSession(Map* map)
    : commands_([this](const FeatureMutation& m, bool undo) {
        return apply_map(m, undo);
      }),
      impl_(std::make_unique<Impl>()),
      map_(map) {}

MapEditSession::~MapEditSession() = default;

void MapEditSession::bind_map(Map* map) { map_ = map; }

bool MapEditSession::undo() { return commands_.undo(); }

bool MapEditSession::redo() { return commands_.redo(); }

bool MapEditSession::can_undo() const { return commands_.can_undo(); }

bool MapEditSession::can_redo() const { return commands_.can_redo(); }

OGRFeature* MapEditSession::build_feature_from_geom(const FeatureGeom& geom) {
  if (!map_ || geom.empty()) {
    return nullptr;
  }
  OGRLayer* lyr = map_->active_ogr_layer();
  if (!lyr) {
    return nullptr;
  }
  OGRGeometry* ogr_geom = ogr_from_geom(geom);
  if (!ogr_geom) {
    return nullptr;
  }
  OGRFeature* ogr = OGRFeature::CreateFeature(lyr->GetLayerDefn());
  if (!ogr) {
    delete ogr_geom;
    return nullptr;
  }
  ogr->SetGeometryDirectly(ogr_geom);
  return ogr;
}

bool MapEditSession::commit(const FeatureMutation& mutation) {
  if (!impl_) {
    return false;
  }
  impl_->last_topology = TopologyValidationResult{};
  const TopologyValidationResult topo = validate_topology(
      impl_->topology, mutation, impl_->topology_gap_tolerance);
  if (!topo.ok()) {
    impl_->last_topology = topo;
    return false;
  }
  FeatureMutation prepared = mutation;
  uint64_t new_token = 0;
  if (!prepared.geom.empty() && prepared.op == EditOp::kAppend &&
      prepared.host_token == 0) {
    OGRFeature* ogr = build_feature_from_geom(prepared.geom);
    if (!ogr) {
      return false;
    }
    if (prepared.id.len == 0) {
      prepared.id = pack_feature_id(ogr->GetFID());
      if (prepared.id.len == 0) {
        prepared.id.len = 1;
        prepared.id.bytes[0] = 1;
      }
    }
    new_token = impl_->next_token++;
    prepared.host_token = new_token;
    impl_->features[new_token] = ogr;
  }
  if (!commands_.commit(prepared)) {
    if (new_token != 0) {
      auto it = impl_->features.find(new_token);
      if (it != impl_->features.end()) {
        OGRFeature::DestroyFeature(it->second);
        impl_->features.erase(it);
      }
    }
    return false;
  }
  apply_topology_state(&impl_->topology, prepared);
  return true;
}

const TopologyValidationResult& MapEditSession::last_topology_result() const {
  static const TopologyValidationResult k_empty{};
  if (!impl_) {
    return k_empty;
  }
  return impl_->last_topology;
}

TopologyLayerState* MapEditSession::topology_layer_state() {
  return impl_ ? &impl_->topology : nullptr;
}

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
  if (!commands_.commit(mutation)) {
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
      return undo ? map_->delete_feature(feature) : map_->append_feature(feature);
    case EditOp::kDelete:
      return undo ? map_->append_feature(feature) : map_->delete_feature(feature);
    case EditOp::kModify:
      return map_->update_feature(feature);
  }
  return false;
}

}  // namespace gis
