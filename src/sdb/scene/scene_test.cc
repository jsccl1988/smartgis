// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/scene/scene.h"
#include "sdb/scene/tessellate.h"

#include "geometry.h"

#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  sdb::scene::World world;
  const uint64_t g0 = world.generation();
  sdb::scene::Node* a = world.add_node(sdb::scene::NodeKind::kModel, "a", 0, 0,
                                        0, 1, 1, 1);
  expect(a != nullptr, "add a");
  const uint64_t id_a = a->id;
  expect(world.generation() > g0, "generation bump add");
  expect(world.node_count() == 1, "count 1");

  sdb::scene::Node* b = world.add_node(sdb::scene::NodeKind::kVectorLayer, "b",
                                        10, 10, 0, 11, 11, 1);
  expect(b != nullptr, "add b");
  const uint64_t id_b = b->id;
  expect(world.node_count() == 2, "count 2");

  std::vector<const sdb::scene::Node*> hits;
  world.query_aabb(0, 0, 0, 0.5, 1, 1, hits);
  expect(hits.size() == 1, "overlap a only");
  expect(hits[0]->id == id_a, "hit a");

  world.query_aabb(100, 100, 0, 101, 101, 1, hits);
  expect(hits.empty(), "no overlap");

  const uint64_t g1 = world.generation();
  expect(world.remove_node(id_a), "remove a");
  expect(world.generation() > g1, "generation bump remove");
  expect(world.node_count() == 1, "count after remove");
  expect(world.find(id_b) != nullptr, "b still there");

  const uint64_t g2 = world.generation();
  world.attach_map(nullptr);
  expect(world.generation() == g2, "null map no bump");

  Smt_Geo::SmtLineString line;
  line.AddPoint(0, 0);
  line.AddPoint(10, 0);
  sdb::scene::TessMesh line_mesh;
  expect(sdb::scene::tessellate_geometry(&line, line_mesh), "tess line");
  expect(line_mesh.indices.size() == 6, "line segment is one quad");

  Smt_Geo::SmtPoint pt(1, 2);
  sdb::scene::TessMesh pt_mesh;
  expect(sdb::scene::tessellate_geometry(&pt, pt_mesh), "tess point");
  expect(pt_mesh.indices.size() == 3, "point is one triangle");

  Smt_Geo::SmtLinearRing ring;
  ring.AddPoint(0, 0);
  ring.AddPoint(2, 0);
  ring.AddPoint(1, 2);
  ring.CloseRings();
  Smt_Geo::SmtPolygon poly;
  poly.AddRing(&ring);
  sdb::scene::TessMesh poly_mesh;
  expect(sdb::scene::tessellate_geometry(&poly, poly_mesh), "tess poly");
  expect(poly_mesh.indices.size() == 3, "triangle poly");

  if (g_fails) {
    std::fprintf(stderr, "scene_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "scene_test: ok\n");
  return 0;
}
