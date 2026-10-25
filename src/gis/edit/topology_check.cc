// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/topology_check.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <memory>

namespace gis {
namespace {

constexpr double k_epsilon = 1e-9;

bool feature_id_less(const FeatureId& a, const FeatureId& b) {
  if (a.len != b.len) {
    return a.len < b.len;
  }
  return std::memcmp(a.bytes, b.bytes, a.len) < 0;
}

bool feature_id_equal(const FeatureId& a, const FeatureId& b) {
  return a.len == b.len && std::memcmp(a.bytes, b.bytes, a.len) == 0;
}

struct Bbox {
  double min_x = 0;
  double min_y = 0;
  double max_x = 0;
  double max_y = 0;
};

Bbox polygon_bbox(const std::vector<MapVertex>& ring) {
  Bbox box{};
  if (ring.empty()) {
    return box;
  }
  box.min_x = box.max_x = ring[0].x;
  box.min_y = box.max_y = ring[0].y;
  for (size_t i = 1; i < ring.size(); ++i) {
    box.min_x = std::min(box.min_x, ring[i].x);
    box.max_x = std::max(box.max_x, ring[i].x);
    box.min_y = std::min(box.min_y, ring[i].y);
    box.max_y = std::max(box.max_y, ring[i].y);
  }
  return box;
}

bool bbox_disjoint(const Bbox& a, const Bbox& b) {
  return a.max_x < b.min_x || b.max_x < a.min_x || a.max_y < b.min_y ||
         b.max_y < a.min_y;
}

double cross(double ax, double ay, double bx, double by) {
  return ax * by - ay * bx;
}

bool point_on_segment(double px, double py, double ax, double ay, double bx,
                      double by) {
  const double abx = bx - ax;
  const double aby = by - ay;
  const double apx = px - ax;
  const double apy = py - ay;
  if (std::abs(cross(abx, aby, apx, apy)) > k_epsilon) {
    return false;
  }
  const double dot = apx * abx + apy * aby;
  if (dot < -k_epsilon) {
    return false;
  }
  const double len_sq = abx * abx + aby * aby;
  return dot <= len_sq + k_epsilon;
}

// Ray casting; returns true for strict interior (not on boundary).
bool point_in_polygon_interior(double px, double py,
                                 const std::vector<MapVertex>& ring) {
  if (ring.size() < 3) {
    return false;
  }
  for (size_t i = 0; i < ring.size(); ++i) {
    const size_t j = (i + 1) % ring.size();
    if (point_on_segment(px, py, ring[i].x, ring[i].y, ring[j].x, ring[j].y)) {
      return false;
    }
  }
  bool inside = false;
  for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++) {
    const double xi = ring[i].x;
    const double yi = ring[i].y;
    const double xj = ring[j].x;
    const double yj = ring[j].y;
    const bool intersect =
        ((yi > py) != (yj > py)) &&
        (px < (xj - xi) * (py - yi) / (yj - yi + k_epsilon) + xi);
    if (intersect) {
      inside = !inside;
    }
  }
  return inside;
}

bool segments_properly_intersect(double ax, double ay, double bx, double by,
                                 double cx, double cy, double dx, double dy) {
  const double d1 = cross(bx - ax, by - ay, cx - ax, cy - ay);
  const double d2 = cross(bx - ax, by - ay, dx - ax, dy - ay);
  const double d3 = cross(dx - cx, dy - cy, ax - cx, ay - cy);
  const double d4 = cross(dx - cx, dy - cy, bx - cx, by - cy);
  if (((d1 > k_epsilon && d2 < -k_epsilon) ||
       (d1 < -k_epsilon && d2 > k_epsilon)) &&
      ((d3 > k_epsilon && d4 < -k_epsilon) ||
       (d3 < -k_epsilon && d4 > k_epsilon))) {
    return true;
  }
  return false;
}

bool polygons_overlap(const std::vector<MapVertex>& a,
                      const std::vector<MapVertex>& b) {
  const Bbox ba = polygon_bbox(a);
  const Bbox bb = polygon_bbox(b);
  if (bbox_disjoint(ba, bb)) {
    return false;
  }
  for (const MapVertex& v : a) {
    if (point_in_polygon_interior(v.x, v.y, b)) {
      return true;
    }
  }
  for (const MapVertex& v : b) {
    if (point_in_polygon_interior(v.x, v.y, a)) {
      return true;
    }
  }
  const size_t na = a.size();
  const size_t nb = b.size();
  for (size_t i = 0; i < na; ++i) {
    const size_t i2 = (i + 1) % na;
    for (size_t j = 0; j < nb; ++j) {
      const size_t j2 = (j + 1) % nb;
      if (segments_properly_intersect(a[i].x, a[i].y, a[i2].x, a[i2].y, b[j].x,
                                      b[j].y, b[j2].x, b[j2].y)) {
        return true;
      }
    }
  }
  return false;
}

double point_to_segment_distance(double px, double py, double ax, double ay,
                                 double bx, double by) {
  const double abx = bx - ax;
  const double aby = by - ay;
  const double len_sq = abx * abx + aby * aby;
  if (len_sq <= k_epsilon) {
    const double dx = px - ax;
    const double dy = py - ay;
    return std::sqrt(dx * dx + dy * dy);
  }
  double t = ((px - ax) * abx + (py - ay) * aby) / len_sq;
  t = std::max(0.0, std::min(1.0, t));
  const double cx = ax + t * abx;
  const double cy = ay + t * aby;
  const double dx = px - cx;
  const double dy = py - cy;
  return std::sqrt(dx * dx + dy * dy);
}

double min_boundary_distance(const std::vector<MapVertex>& a,
                             const std::vector<MapVertex>& b) {
  double best = 1e300;
  const auto consider = [&](double px, double py, double ax, double ay,
                            double bx, double by) {
    best = std::min(best, point_to_segment_distance(px, py, ax, ay, bx, by));
  };
  const size_t na = a.size();
  const size_t nb = b.size();
  for (size_t i = 0; i < na; ++i) {
    const size_t i2 = (i + 1) % na;
    for (const MapVertex& v : b) {
      consider(v.x, v.y, a[i].x, a[i].y, a[i2].x, a[i2].y);
    }
  }
  for (size_t j = 0; j < nb; ++j) {
    const size_t j2 = (j + 1) % nb;
    for (const MapVertex& v : a) {
      consider(v.x, v.y, b[j].x, b[j].y, b[j2].x, b[j2].y);
    }
  }
  return best;
}

bool polygons_have_sliver_gap(const std::vector<MapVertex>& a,
                              const std::vector<MapVertex>& b,
                              double gap_tolerance) {
  if (polygons_overlap(a, b)) {
    return false;
  }
  const Bbox ba = polygon_bbox(a);
  const Bbox bb = polygon_bbox(b);
  Bbox expanded = ba;
  expanded.min_x -= gap_tolerance;
  expanded.min_y -= gap_tolerance;
  expanded.max_x += gap_tolerance;
  expanded.max_y += gap_tolerance;
  if (bbox_disjoint(expanded, bb)) {
    return false;
  }
  const double dist = min_boundary_distance(a, b);
  return dist > k_epsilon && dist <= gap_tolerance;
}

bool is_polygon_geom(const FeatureGeom& geom) {
  return geom.kind == FeatureGeom::Kind::kPolygon && geom.points.size() >= 3;
}

}  // namespace

struct TopologyLayerState::Impl {
  std::map<FeatureId, FeatureGeom, FeatureIdLess> features;
};

bool TopologyLayerState::FeatureIdLess::operator()(const FeatureId& a,
                                                     const FeatureId& b) const {
  return feature_id_less(a, b);
}

TopologyLayerState::TopologyLayerState() : impl_(std::make_unique<Impl>()) {}

TopologyLayerState::~TopologyLayerState() = default;

void TopologyLayerState::clear() { impl_->features.clear(); }

void TopologyLayerState::upsert(const FeatureId& id, const FeatureGeom& geom) {
  impl_->features[id] = geom;
}

void TopologyLayerState::remove(const FeatureId& id) {
  impl_->features.erase(id);
}

const FeatureGeom* TopologyLayerState::find(const FeatureId& id) const {
  const auto it = impl_->features.find(id);
  if (it == impl_->features.end()) {
    return nullptr;
  }
  return &it->second;
}

size_t TopologyLayerState::feature_count() const {
  return impl_->features.size();
}

void TopologyLayerState::visit_features(
    const std::function<void(const FeatureId&, const FeatureGeom&)>& visitor)
    const {
  for (const auto& entry : impl_->features) {
    visitor(entry.first, entry.second);
  }
}

TopologyValidationResult validate_topology(const TopologyLayerState& state,
                                           const FeatureMutation& mutation,
                                           double gap_tolerance) {
  TopologyValidationResult out;
  if (mutation.op == EditOp::kDelete || !is_polygon_geom(mutation.geom)) {
    return out;
  }

  state.visit_features([&](const FeatureId& id, const FeatureGeom& geom) {
    if (mutation.op == EditOp::kModify && feature_id_equal(id, mutation.id)) {
      return;
    }
    if (!is_polygon_geom(geom)) {
      return;
    }
    if (polygons_overlap(mutation.geom.points, geom.points)) {
      TopologyIssue issue;
      issue.kind = TopologyIssueKind::kOverlap;
      issue.feature_id = mutation.id;
      issue.other_id = id;
      issue.message = "polygon overlap with existing feature";
      out.add_issue(std::move(issue));
    } else if (polygons_have_sliver_gap(mutation.geom.points, geom.points,
                                        gap_tolerance)) {
      TopologyIssue issue;
      issue.kind = TopologyIssueKind::kGap;
      issue.feature_id = mutation.id;
      issue.other_id = id;
      issue.message = "boundary gap below tolerance with neighbor";
      out.add_issue(std::move(issue));
    }
  });
  return out;
}

void apply_topology_state(TopologyLayerState* state,
                          const FeatureMutation& mutation) {
  if (!state) {
    return;
  }
  switch (mutation.op) {
    case EditOp::kDelete:
      state->remove(mutation.id);
      break;
    case EditOp::kAppend:
    case EditOp::kModify:
      if (is_polygon_geom(mutation.geom)) {
        state->upsert(mutation.id, mutation.geom);
      }
      break;
  }
}

}  // namespace gis
