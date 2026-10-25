// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GIS_EDIT_TOPOLOGY_CHECK_H_
#define GIS_EDIT_TOPOLOGY_CHECK_H_

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "gis/edit/mutation.h"
#include "gis/gis_export.h"

namespace gis {

enum class TopologyIssueKind {
  kOverlap = 0,
  kGap = 1,
};

// One topology rule violation; ids identify the edited feature and a neighbor.
struct TopologyIssue {
  TopologyIssueKind kind = TopologyIssueKind::kOverlap;
  FeatureId feature_id{};
  FeatureId other_id{};
  std::string message;
};

// In-memory polygon set for overlap / gap checks on one edit layer.
class GIS_EXPORT TopologyLayerState {
 public:
  TopologyLayerState();
  ~TopologyLayerState();

  TopologyLayerState(const TopologyLayerState&) = delete;
  TopologyLayerState& operator=(const TopologyLayerState&) = delete;

  void clear();
  void upsert(const FeatureId& id, const FeatureGeom& geom);
  void remove(const FeatureId& id);
  const FeatureGeom* find(const FeatureId& id) const;
  size_t feature_count() const;
  void visit_features(
      const std::function<void(const FeatureId&, const FeatureGeom&)>& visitor)
      const;

 private:
  struct FeatureIdLess {
    bool operator()(const FeatureId& a, const FeatureId& b) const;
  };

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

// Result of validate_topology; inspect issues() when ok() is false.
struct TopologyValidationResult {
  bool ok() const { return issues_.empty(); }
  const std::vector<TopologyIssue>& issues() const { return issues_; }
  void add_issue(TopologyIssue issue) { issues_.push_back(std::move(issue)); }

 private:
  std::vector<TopologyIssue> issues_;
};

// Validates |mutation| against current |state| (excluding self on modify).
// |gap_tolerance| — max boundary separation (map units) treated as a sliver gap.
GIS_EXPORT TopologyValidationResult validate_topology(
    const TopologyLayerState& state, const FeatureMutation& mutation,
    double gap_tolerance = 1e-6);

// Applies a committed mutation to |state| (append/modify upsert geom; delete removes).
GIS_EXPORT void apply_topology_state(TopologyLayerState* state,
                                     const FeatureMutation& mutation);

}  // namespace gis

#endif  // GIS_EDIT_TOPOLOGY_CHECK_H_
