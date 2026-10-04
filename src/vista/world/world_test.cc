// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/world/world.h"

#include <cstdio>
#include <cstring>
#include <vector>

#include "gis/geo/ops/indexed_tin.h"
#include "gis/map/layer_kind.h"
#include "vista/assets/model/model.h"
#include "vista/assets/tileset/tileset.h"
#include "vista/mesh/tessellate.h"

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
  vista::World world;
  const uint64_t g0 = world.generation();
  vista::Node* a =
      world.add_node(vista::NodeKind::kModel, "a", 0, 0, 0, 1, 1, 1);
  expect(a != nullptr, "add a");
  const uint64_t id_a = a->id;
  expect(world.generation() > g0, "generation bump add");
  expect(world.node_count() == 1, "count 1");

  vista::Node* b = world.add_node(vista::NodeKind::kVectorLayer, "b",
                                       10, 10, 0, 11, 11, 1);
  expect(b != nullptr, "add b");
  const uint64_t id_b = b->id;
  expect(world.node_count() == 2, "count 2");

  std::vector<const vista::Node*> hits;
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

  OGRLineString line;
  line.addPoint(0, 0);
  line.addPoint(10, 0);
  vista::TessMesh line_mesh;
  expect(vista::tessellate_geometry(&line, line_mesh), "tess line");
  expect(line_mesh.indices.size() == 6, "line segment is one quad");

  OGRPoint pt(1, 2);
  vista::TessMesh pt_mesh;
  expect(vista::tessellate_geometry(&pt, pt_mesh), "tess point");
  expect(pt_mesh.indices.size() == 3, "point is one triangle");

  OGRLinearRing ring;
  ring.addPoint(0, 0);
  ring.addPoint(2, 0);
  ring.addPoint(1, 2);
  ring.closeRings();
  OGRPolygon poly;
  poly.addRing(&ring);
  vista::TessMesh poly_mesh;
  expect(vista::tessellate_geometry(&poly, poly_mesh), "tess poly");
  expect(poly_mesh.indices.size() == 3, "triangle poly");

  // Arc leftover is an OGRLineString.
  OGRLineString arc;
  arc.addPoint(0, 0);
  arc.addPoint(1, 1);
  arc.addPoint(2, 0);
  vista::TessMesh arc_mesh;
  expect(vista::tessellate_arc(&arc, arc_mesh), "tess arc");
  expect(arc_mesh.indices.size() == 12, "arc 2 segments = 2 quads");

  // Fan leftover is an OGRPolygon.
  OGRLinearRing fan_ring;
  fan_ring.addPoint(1, 1);
  fan_ring.addPoint(3, 0);
  fan_ring.addPoint(0, 0);
  fan_ring.closeRings();
  OGRPolygon fan;
  fan.addRing(&fan_ring);
  vista::TessMesh fan_mesh;
  expect(vista::tessellate_fan(&fan, fan_mesh), "tess fan");
  expect(fan_mesh.indices.size() == 3, "fan triangle");

  OGRTriangulatedSurface tin;
  OGRPoint ta(0, 0, 0);
  OGRPoint tb(2, 0, 0);
  OGRPoint tc(1, 2, 0);
  expect(geo::add_patch(&tin, ta, tb, tc), "tin patch");
  vista::TessMesh tin_mesh;
  expect(vista::tessellate_tin(&tin, tin_mesh), "tess tin");
  expect(tin_mesh.indices.size() == 3, "tin one triangle");

  OGRMultiPoint grid_mp;
  OGRPoint gp00(0, 0);
  OGRPoint gp10(1, 0);
  OGRPoint gp01(0, 1);
  OGRPoint gp11(1, 1);
  grid_mp.addGeometry(&gp00);
  grid_mp.addGeometry(&gp10);
  grid_mp.addGeometry(&gp01);
  grid_mp.addGeometry(&gp11);
  vista::TessMesh grid_mesh;
  expect(vista::tessellate_grid(&grid_mp, 2, 2, grid_mesh), "tess grid");
  expect(grid_mesh.indices.size() == 6, "grid 2x2 is one quad");

  vista::TessMesh ras_solid;
  expect(vista::tessellate_aabb(0, 0, 0, 10, 8, 0, ras_solid),
         "tess raster envelope");
  expect(ras_solid.indices.size() == 36, "raster aabb box");

  vista::TessMesh box;
  expect(vista::tessellate_aabb(0, 0, 0, 1, 1, 1, box), "tess aabb");
  expect(box.positions.size() == 24, "aabb 8 verts");
  expect(box.indices.size() == 36, "aabb 12 tris");

  vista::World gis;
  expect(gis.attach_tin(&tin, "tin") != nullptr, "attach tin");
  expect(gis.attach_grid(&grid_mp, 2, 2, "grid") != nullptr, "attach grid");
  expect(gis.add_node(vista::NodeKind::kRasterLayer, "ras", 0, 0, 0, 10, 8, 0) !=
             nullptr,
         "attach raster aabb");
  expect(gis.add_node(vista::NodeKind::kRasterLayer, "tiles", 0, 0, 0, 4, 2, 0) !=
             nullptr,
         "attach tile aabb");
  expect(gis.node_count() == 4, "tin+grid+raster+tile");
  expect(gis.node_at(0)->kind == vista::NodeKind::kVectorLayer &&
             gis.node_at(0)->tin == &tin,
         "tin node");
  expect(gis.node_at(1)->kind == vista::NodeKind::kVectorLayer &&
             gis.node_at(1)->grid == &grid_mp,
         "grid node");
  expect(gis.node_at(2)->kind == vista::NodeKind::kRasterLayer,
         "raster kind");
  expect(gis.node_at(3)->kind == vista::NodeKind::kRasterLayer,
         "tile kind");

  vista::ModelAsset cube;
  vista::load_unit_cube(cube);
  vista::World handles;
  expect(handles.attach_model(nullptr, "nope") == nullptr, "null model");
  vista::Node* model_node = handles.attach_model(&cube, "cube");
  expect(model_node && model_node->kind == vista::NodeKind::kModel,
         "attach model");
  expect(model_node->model == &cube && model_node->min_x == -0.5,
         "model handle");

  const char* ts_json =
      "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,2,0,0,0,2,0,0,0,2]},"
      "\"geometricError\":10,\"content\":{\"uri\":\"a.glb\"},"
      "\"children\":[{\"boundingVolume\":{\"box\":[4,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"b.glb\"}}]}}";
  vista::Tileset tileset;
  expect(vista::parse_tileset_json(ts_json, std::strlen(ts_json), tileset),
         "handle tileset parse");
  vista::Node* ts_node = handles.attach_tileset(&tileset, "ts");
  expect(ts_node && ts_node->kind == vista::NodeKind::kTileset,
         "attach tileset");
  const uint64_t gen = handles.generation();
  vista::ViewState view;
  view.eye_x = 0;
  view.eye_y = 0;
  view.eye_z = 8;
  view.sse_denominator = 1;
  std::vector<const vista::Tile*> vis;
  vista::select_tiles(tileset, view, 0, vis);
  expect(handles.apply_tileset_selection(ts_node->id, vis), "visible bump");
  expect(handles.generation() == gen + 1, "tileset generation");
  expect(!handles.apply_tileset_selection(ts_node->id, vis), "no bump if same");
  expect(handles.generation() == gen + 1, "stable generation");
  // stream_tileset re-selects; same URIs → no generation bump.
  expect(!handles.stream_tileset(ts_node->id, view, 0, 0),
         "stream same selection");
  expect(handles.generation() == gen + 1, "stream stable generation");
  // Huge SSE collapses to root only → URI set changes → bump.
  expect(handles.stream_tileset(ts_node->id, view, 1e9, 0), "stream root");
  expect(handles.find(ts_node->id)->visible_uris.size() == 1 &&
             handles.find(ts_node->id)->visible_uris[0] == "a.glb",
         "stream root uri");

  vista::Node* terrain = handles.attach_terrain("dem", 0, 0, 0, 10, 10, 4);
  expect(terrain && terrain->kind == vista::NodeKind::kTerrain,
         "terrain handle");
  const uint64_t terrain_id = terrain->id;
  vista::Node* cloud =
      handles.attach_pointcloud("cloud", 1, 1, 1, 2, 2, 2);
  expect(cloud && cloud->kind == vista::NodeKind::kPointCloud,
         "pointcloud handle");
  expect(handles.find(terrain_id) &&
             handles.find(terrain_id)->kind == vista::NodeKind::kTerrain,
         "terrain still findable");

  if (g_fails) {
    std::fprintf(stderr, "world_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "world_test: ok\n");
  return 0;
}
