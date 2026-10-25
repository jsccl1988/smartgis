// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gis/edit/memory_session.h"
#include "gis/edit/topology_check.h"

#include "gtest/gtest.h"

namespace {

gis::FeatureId make_id(uint8_t v) {
  gis::FeatureId id{};
  id.len = 1;
  id.bytes[0] = v;
  return id;
}

gis::FeatureGeom make_rect(double x0, double y0, double x1, double y1) {
  gis::FeatureGeom geom;
  geom.kind = gis::FeatureGeom::Kind::kPolygon;
  geom.points = {{x0, y0}, {x1, y0}, {x1, y1}, {x0, y1}, {x0, y0}};
  return geom;
}

}  // namespace

TEST(TopologyCheck, OverlapFixtureFailsCommit) {
  gis::MemoryEditSession session;

  gis::FeatureMutation first{};
  first.op = gis::EditOp::kAppend;
  first.id = make_id(1);
  first.geom = make_rect(0, 0, 2, 2);
  ASSERT_TRUE(session.commit(first));
  EXPECT_EQ(session.last_status(), gis::CommitStatus::kOk);

  gis::FeatureMutation second{};
  second.op = gis::EditOp::kAppend;
  second.id = make_id(2);
  second.geom = make_rect(1, 1, 3, 3);
  EXPECT_FALSE(session.commit(second));
  EXPECT_EQ(session.last_status(), gis::CommitStatus::kTopology);
  const gis::TopologyValidationResult& topo = session.last_topology_result();
  ASSERT_FALSE(topo.ok());
  ASSERT_EQ(topo.issues().size(), 1u);
  EXPECT_EQ(topo.issues()[0].kind, gis::TopologyIssueKind::kOverlap);
}

TEST(TopologyCheck, CleanFixtureCommitsOk) {
  gis::MemoryEditSession session;

  gis::FeatureMutation first{};
  first.op = gis::EditOp::kAppend;
  first.id = make_id(10);
  first.geom = make_rect(0, 0, 1, 1);
  ASSERT_TRUE(session.commit(first));

  gis::FeatureMutation second{};
  second.op = gis::EditOp::kAppend;
  second.id = make_id(11);
  second.geom = make_rect(2, 0, 3, 1);
  EXPECT_TRUE(session.commit(second));
  EXPECT_EQ(session.last_status(), gis::CommitStatus::kOk);
  EXPECT_TRUE(session.last_topology_result().ok());
  EXPECT_EQ(session.topology_layer_state()->feature_count(), 2u);
}

TEST(TopologyCheck, GapFixtureFailsWhenToleranceExceeded) {
  gis::TopologyLayerState state;
  gis::FeatureMutation seed{};
  seed.op = gis::EditOp::kAppend;
  seed.id = make_id(1);
  seed.geom = make_rect(0, 0, 1, 1);
  apply_topology_state(&state, seed);

  gis::FeatureMutation neighbor{};
  neighbor.op = gis::EditOp::kAppend;
  neighbor.id = make_id(2);
  neighbor.geom = make_rect(1.05, 0, 2.05, 1);

  const gis::TopologyValidationResult result =
      validate_topology(state, neighbor, 0.1);
  ASSERT_FALSE(result.ok());
  EXPECT_EQ(result.issues()[0].kind, gis::TopologyIssueKind::kGap);
}
