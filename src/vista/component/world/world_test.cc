// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/world.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "gis/geo/ops/indexed_tin.h"
#include "gis/map/layer_kind.h"
#include "vista/assets/model/model.h"
#include "vista/assets/tileset/tileset.h"
#include "vista/component/world/terrain/grid.h"
#include "vista/component/world/terrain/policy.h"
#include "vista/component/world/terrain/seed.h"
#include "vista/mesh/tessellate.h"
#include "vista/terrain/dem/dem_raster.h"

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

  expect(vista::select_nested_grid_tiles(0, 0, 0, 1, 0.5f, nullptr) == 0,
         "nested reject null out");
  {
    std::vector<vista::NestedGridTile> far_tiles;
    expect(vista::select_nested_grid_tiles(0, 0, 8, 8, 5.0f, &far_tiles) == 1,
           "far nested is one ring");
    expect(far_tiles[0].ring == 0 && far_tiles[0].maxx > far_tiles[0].minx,
           "far nested covers box");
    std::vector<vista::NestedGridTile> near_tiles;
    const size_t near_n =
        vista::select_nested_grid_tiles(0, 0, 8, 8, 0.5f, &near_tiles);
    expect(near_n > far_tiles.size(), "near nested more tiles");
    expect(vista::terrain_lod_nested_rings(0.5f) == 4, "near four rings");
    expect(near_tiles[0].ring == 0, "first tile inner ring");
    int outer_edge = near_tiles[0].max_edge;
    for (const auto& t : near_tiles) {
      if (t.ring > 0 && t.max_edge < outer_edge) {
        outer_edge = t.max_edge;
      }
    }
    expect(near_tiles[0].max_edge > outer_edge,
           "same seed inner max_edge denser than outer");
    expect(vista::terrain_lod_patch_distance(0.5f, 0, 0, 8, 8, 3, 3, 5, 5) <
               vista::terrain_lod_patch_distance(0.5f, 0, 0, 8, 8, 0, 0, 8, 1),
           "center patch closer than edge patch");
  }
  {
    vista::DemRaster dem;
    std::vector<float> heights(65 * 65, 80.f);
    for (int i = 0; i < 65 * 65; ++i) {
      heights[static_cast<size_t>(i)] = 40.f + static_cast<float>(i % 65);
    }
    std::vector<uint8_t> land(65 * 65, 1);
    expect(dem.adopt_bake_cache(65, 65, 0, 0, 8, 8, 40.f, 72.f, 1.f,
                                std::move(heights), std::move(land), ""),
           "nested fixture dem");
    vista::World nested_far;
    vista::World nested_near;
    const size_t far_n = vista::seed_dem_nested_grid_into_world(
        &nested_far, dem, 0, 0, 8, 8, 5.0f, 65536, "f");
    const size_t near_n = vista::seed_dem_nested_grid_into_world(
        &nested_near, dem, 0, 0, 8, 8, 0.5f, 65536, "n");
    expect(far_n == 1, "far nested seed one tile");
    expect(near_n > far_n, "near nested seed more rings");
    expect(nested_near.node_at(0) &&
               nested_near.node_at(0)->terrain.source ==
                   vista::TerrainSource::kRaster,
           "nested raster source");
    std::vector<vista::NestedGridTile> seeded_tiles;
    vista::select_nested_grid_tiles(0, 0, 8, 8, 0.5f, &seeded_tiles);
    expect(!seeded_tiles.empty() && nested_near.node_at(0) &&
               nested_near.node_at(0)->terrain.lod_key ==
                   vista::terrain_lod_nested_tile_key(
                       seeded_tiles[0].ring, seeded_tiles[0].max_edge),
           "nested inner tile key");
    int inner_edge = 0;
    int outer_ring = -1;
    int outer_edge = 0;
    for (size_t i = 0; i < nested_near.node_count(); ++i) {
      const vista::Node* n = nested_near.node_at(i);
      if (!n || !n->has_terrain_mesh()) {
        continue;
      }
      const int key = n->terrain.lod_key - 2000000;
      const int ring = key / 10000;
      const int edge = key % 10000;
      if (ring == 0) {
        inner_edge = edge;
      } else if (ring > outer_ring) {
        outer_ring = ring;
        outer_edge = edge;
      } else if (ring == outer_ring && edge < outer_edge) {
        outer_edge = edge;
      }
    }
    expect(inner_edge > 0 && outer_edge > 0 && inner_edge > outer_edge,
           "seeded inner patch denser max_edge than outer");
    bool have_morph = false;
    bool have_skirt = false;
    for (size_t i = 0; i < nested_near.node_count(); ++i) {
      const vista::Node* n = nested_near.node_at(i);
      if (!n || !n->has_terrain_mesh()) {
        continue;
      }
      if (n->terrain.morph.size() == n->terrain.positions.size() / 3 &&
          !n->terrain.morph.empty()) {
        have_morph = true;
      }
      float ymin = n->terrain.positions[1];
      float ymax = ymin;
      for (size_t k = 1; k < n->terrain.positions.size(); k += 3) {
        ymin = (std::min)(ymin, n->terrain.positions[k]);
        ymax = (std::max)(ymax, n->terrain.positions[k]);
      }
      if (ymin < 39.5f) {
        have_skirt = true;
      }
    }
    expect(have_morph, "nested CPU morph weights");
    expect(have_skirt, "nested edge skirts drop Y");
    std::vector<vista::NestedGridTile> focused;
    vista::select_nested_grid_tiles(0, 0, 8, 8, 0.5f, 1.0, 1.0, &focused);
    expect(!focused.empty() && focused[0].ring == 0, "focus inner ring");
    const double fcx = 0.5 * (focused[0].minx + focused[0].maxx);
    const double fcy = 0.5 * (focused[0].miny + focused[0].maxy);
    expect(std::hypot(fcx - 1.0, fcy - 1.0) < std::hypot(fcx - 4.0, fcy - 4.0),
           "focus inner closer to camera than AABB center");
    expect(focused[0].morph_weight >= 0.f && focused[0].morph_weight <= 1.f,
           "tile morph in range");
  }
  {
    std::vector<float> xyz = {0.f, 2.f, 0.f, 1.f, 2.f, 0.f, 1.f, 2.f, 1.f,
                              0.f, 2.f, 1.f};
    std::vector<uint32_t> idx = {0, 1, 2, 0, 2, 3};
    std::vector<float> uvs = {0.f, 0.f, 1.f, 0.f, 1.f, 1.f, 0.f, 1.f};
    const size_t added =
        vista::append_terrain_edge_skirts(&xyz, &idx, &uvs, 0.5f);
    expect(added > 0 && xyz.size() > 12 && idx.size() > 6, "skirt extra tris");
    bool dropped = false;
    for (size_t i = 1; i < xyz.size(); i += 3) {
      if (xyz[i] < 1.6f) {
        dropped = true;
      }
    }
    expect(dropped, "skirt verts lower Y");
    expect(uvs.size() == (xyz.size() / 3) * 2u, "skirt uvs follow verts");
  }

  if (g_fails) {
    std::fprintf(stderr, "world_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "world_test: ok\n");
  return 0;
}
