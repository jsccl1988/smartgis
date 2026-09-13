// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "sdb/scene/scene.h"
#include "sdb/scene/tessellate.h"
#include "sdb/model/model.h"
#include "sdb/model/tileset.h"

#include "algorithm/geo/geometry.h"
#include "sdb/layer/layer.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

class TestRasterLayer : public sdb::SmtRasterLayer {
 public:
  bool Create() override { return true; }
  bool Open(const char*) override { return true; }
  bool Close() override { return true; }
  bool Fetch(sdb::eSmtFetchType) override { return true; }
  long CreaterRaster(const char* buf, long size, const base::fRect& rect,
                     long code) override {
    buf_ = buf;
    size_ = size;
    rect_ = rect;
    code_ = code;
    SetLayerRect(rect);
    return SMT_ERR_NONE;
  }
  long SetRasterRect(const base::fRect& rect) override {
    rect_ = rect;
    return SMT_ERR_NONE;
  }
  long GetRaster(char*& buf, long& size, base::fRect& rect,
                 long& code) const override {
    return GetRasterNoClone(buf, size, rect, code);
  }
  long GetRasterNoClone(char*& buf, long& size, base::fRect& rect,
                        long& code) const override {
    buf = const_cast<char*>(buf_);
    size = size_;
    rect = rect_;
    code = code_;
    return buf_ && size_ > 0 ? SMT_ERR_NONE : SMT_ERR_UNSUPPORTED;
  }
  long GetRasterRect(base::fRect& rect) const override {
    rect = rect_;
    return SMT_ERR_NONE;
  }

 private:
  const char* buf_ = nullptr;
  long size_ = 0;
  base::fRect rect_{};
  long code_ = 0;
};

class TestTileLayer : public sdb::SmtTileLayer {
 public:
  bool Create() override { return true; }
  bool Open(const char*) override { return true; }
  bool Close() override { return true; }
  bool Fetch(sdb::eSmtFetchType) override { return true; }
  void CalEnvelope() override {}
  int GetTileCount() const override { return static_cast<int>(tiles_.size()); }
  void MoveFirst() const override { it_ = 0; }
  void MoveNext() const override { ++it_; }
  void MoveLast() const override {
    it_ = tiles_.empty() ? 0 : tiles_.size() - 1;
  }
  void Delete() override {}
  bool IsEnd() const override { return it_ >= tiles_.size(); }
  void DeleteAll() override { tiles_.clear(); }
  long AppendTile(const base::SmtTile* tile, bool) override {
    if (tile) {
      tiles_.push_back(*tile);
    }
    return SMT_ERR_NONE;
  }
  long UpdateTile(const base::SmtTile*) override { return SMT_ERR_NONE; }
  long DeleteTile(const base::SmtTile*) override { return SMT_ERR_NONE; }
  base::SmtTile* GetTile() const override {
    return it_ < tiles_.size() ? const_cast<base::SmtTile*>(&tiles_[it_])
                               : nullptr;
  }
  base::SmtTile* GetTile(int index) const override {
    if (index < 0 || index >= static_cast<int>(tiles_.size())) {
      return nullptr;
    }
    return const_cast<base::SmtTile*>(&tiles_[static_cast<size_t>(index)]);
  }
  base::SmtTile* GetTileByID(uint id) const override {
    for (auto& tile : tiles_) {
      if (tile.lID == static_cast<long>(id)) {
        return const_cast<base::SmtTile*>(&tile);
      }
    }
    return nullptr;
  }

 private:
  std::vector<base::SmtTile> tiles_;
  mutable size_t it_ = 0;
};

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

  OGRLineString line;
  line.addPoint(0, 0);
  line.addPoint(10, 0);
  sdb::scene::TessMesh line_mesh;
  expect(sdb::scene::tessellate_geometry(&line, line_mesh), "tess line");
  expect(line_mesh.indices.size() == 6, "line segment is one quad");

  OGRPoint pt(1, 2);
  sdb::scene::TessMesh pt_mesh;
  expect(sdb::scene::tessellate_geometry(&pt, pt_mesh), "tess point");
  expect(pt_mesh.indices.size() == 3, "point is one triangle");

  OGRLinearRing ring;
  ring.addPoint(0, 0);
  ring.addPoint(2, 0);
  ring.addPoint(1, 2);
  ring.closeRings();
  OGRPolygon poly;
  poly.addRing(&ring);
  sdb::scene::TessMesh poly_mesh;
  expect(sdb::scene::tessellate_geometry(&poly, poly_mesh), "tess poly");
  expect(poly_mesh.indices.size() == 3, "triangle poly");

  // Arc leftover is an OGRLineString.
  OGRLineString arc;
  arc.addPoint(0, 0);
  arc.addPoint(1, 1);
  arc.addPoint(2, 0);
  sdb::scene::TessMesh arc_mesh;
  expect(sdb::scene::tessellate_arc(&arc, arc_mesh), "tess arc");
  expect(arc_mesh.indices.size() == 12, "arc 2 segments = 2 quads");

  // Fan leftover is an OGRPolygon.
  OGRLinearRing fan_ring;
  fan_ring.addPoint(1, 1);
  fan_ring.addPoint(3, 0);
  fan_ring.addPoint(0, 0);
  fan_ring.closeRings();
  OGRPolygon fan;
  fan.addRing(&fan_ring);
  sdb::scene::TessMesh fan_mesh;
  expect(sdb::scene::tessellate_fan(&fan, fan_mesh), "tess fan");
  expect(fan_mesh.indices.size() == 3, "fan triangle");

  geo::Tin tin;
  OGRPoint ta(0, 0);
  OGRPoint tb(2, 0);
  OGRPoint tc(1, 2);
  tin.add_point(&ta);
  tin.add_point(&tb);
  tin.add_point(&tc);
  base::SmtTriangle tri;
  tri.a = 0;
  tri.b = 1;
  tri.c = 2;
  tin.add_triangle(&tri);
  sdb::scene::TessMesh tin_mesh;
  expect(sdb::scene::tessellate_tin(&tin, tin_mesh), "tess tin");
  expect(tin_mesh.indices.size() == 3, "tin one triangle");

  geo::Grid grid(2, 2);
  grid.set_node(0, 0, geo::RawPoint(0, 0));
  grid.set_node(0, 1, geo::RawPoint(1, 0));
  grid.set_node(1, 0, geo::RawPoint(0, 1));
  grid.set_node(1, 1, geo::RawPoint(1, 1));
  sdb::scene::TessMesh grid_mesh;
  expect(sdb::scene::tessellate_grid(&grid, grid_mesh), "tess grid");
  expect(grid_mesh.indices.size() == 6, "grid 2x2 is one quad");

  TestRasterLayer raster;
  raster.SetLayerName("ras");
  base::fRect ras_rect;
  ras_rect.lb.x = 0;
  ras_rect.lb.y = 0;
  ras_rect.rt.x = 10;
  ras_rect.rt.y = 8;
  raster.SetLayerRect(ras_rect);
  sdb::scene::TessMesh ras_solid;
  expect(sdb::scene::tessellate_raster_layer(&raster, ras_solid),
         "tess raster solid");
  expect(ras_solid.indices.size() == 6, "raster envelope quad");
  expect(!ras_solid.has_image, "solid fallback without buffer");

  char pixels[4] = {1, 2, 3, 4};
  expect(raster.CreaterRaster(pixels, 4, ras_rect, 1) == SMT_ERR_NONE,
         "raster buffer");
  sdb::scene::TessMesh ras_tex;
  expect(sdb::scene::tessellate_raster_layer(&raster, ras_tex),
         "tess raster textured");
  expect(ras_tex.indices.size() == 6, "textured raster still one quad");
  expect(ras_tex.has_image, "image buffer marks textured quad");

  TestTileLayer tiles;
  tiles.SetLayerName("tiles");
  base::fRect tile_env;
  tile_env.lb.x = 0;
  tile_env.lb.y = 0;
  tile_env.rt.x = 4;
  tile_env.rt.y = 2;
  tiles.SetLayerRect(tile_env);
  sdb::scene::TessMesh tile_fallback;
  expect(sdb::scene::tessellate_tile_layer(&tiles, tile_fallback),
         "tess tile envelope fallback");
  expect(tile_fallback.indices.size() == 6, "empty tile layer uses envelope");

  base::SmtTile t0;
  t0.lID = 1;
  t0.rtTileRect.lb.x = 0;
  t0.rtTileRect.lb.y = 0;
  t0.rtTileRect.rt.x = 2;
  t0.rtTileRect.rt.y = 2;
  base::SmtTile t1;
  t1.lID = 2;
  t1.rtTileRect.lb.x = 2;
  t1.rtTileRect.lb.y = 0;
  t1.rtTileRect.rt.x = 4;
  t1.rtTileRect.rt.y = 2;
  t1.pTileBuf = pixels;
  t1.lTileBufSize = 4;
  tiles.AppendTile(&t0, false);
  tiles.AppendTile(&t1, false);
  sdb::scene::TessMesh tile_mesh;
  expect(sdb::scene::tessellate_tile_layer(&tiles, tile_mesh), "tess tiles");
  expect(tile_mesh.indices.size() == 12, "two tile quads");
  expect(tile_mesh.has_image, "tile with buffer is textured");

  sdb::scene::World gis;
  expect(gis.attach_tin(&tin, "tin") != nullptr, "attach tin");
  expect(gis.attach_grid(&grid, "grid") != nullptr, "attach grid");
  expect(gis.attach_raster_layer(&raster) != nullptr, "attach raster");
  expect(gis.attach_tile_layer(&tiles) != nullptr, "attach tiles");
  expect(gis.node_count() == 4, "tin+grid+raster+tile");
  expect(gis.node_at(0)->kind == sdb::scene::NodeKind::kVectorLayer &&
             gis.node_at(0)->tin == &tin,
         "tin node");
  expect(gis.node_at(1)->kind == sdb::scene::NodeKind::kVectorLayer &&
             gis.node_at(1)->grid == &grid,
         "grid node");
  expect(gis.node_at(2)->kind == sdb::scene::NodeKind::kRasterLayer, "raster kind");
  expect(gis.node_at(3)->kind == sdb::scene::NodeKind::kRasterLayer, "tile kind");

  sdb::model::ModelAsset cube;
  sdb::model::load_unit_cube(cube);
  sdb::scene::World handles;
  expect(handles.attach_model(nullptr, "nope") == nullptr, "null model");
  sdb::scene::Node* model_node = handles.attach_model(&cube, "cube");
  expect(model_node && model_node->kind == sdb::scene::NodeKind::kModel,
         "attach model");
  expect(model_node->model == &cube && model_node->min_x == -0.5, "model handle");

  const char* ts_json =
      "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,2,0,0,0,2,0,0,0,2]},"
      "\"geometricError\":10,\"content\":{\"uri\":\"a.glb\"},"
      "\"children\":[{\"boundingVolume\":{\"box\":[4,0,0,1,0,0,0,1,0,0,0,1]},"
      "\"geometricError\":0,\"content\":{\"uri\":\"b.glb\"}}]}}";
  sdb::model::Tileset tileset;
  expect(sdb::model::parse_tileset_json(ts_json, std::strlen(ts_json), tileset),
         "handle tileset parse");
  sdb::scene::Node* ts_node = handles.attach_tileset(&tileset, "ts");
  expect(ts_node && ts_node->kind == sdb::scene::NodeKind::kTileset,
         "attach tileset");
  const uint64_t gen = handles.generation();
  sdb::model::ViewState view;
  view.eye_x = 0;
  view.eye_y = 0;
  view.eye_z = 8;
  view.sse_denominator = 1;
  std::vector<const sdb::model::Tile*> vis;
  sdb::model::select_tiles(tileset, view, 0, vis);
  expect(handles.apply_tileset_selection(ts_node->id, vis), "visible bump");
  expect(handles.generation() == gen + 1, "tileset generation");
  expect(!handles.apply_tileset_selection(ts_node->id, vis), "no bump if same");
  expect(handles.generation() == gen + 1, "stable generation");

  sdb::scene::Node* terrain =
      handles.attach_terrain("dem", 0, 0, 0, 10, 10, 4);
  expect(terrain && terrain->kind == sdb::scene::NodeKind::kTerrain,
         "terrain handle");
  const uint64_t terrain_id = terrain->id;
  sdb::scene::Node* cloud =
      handles.attach_pointcloud("cloud", 1, 1, 1, 2, 2, 2);
  expect(cloud && cloud->kind == sdb::scene::NodeKind::kPointCloud,
         "pointcloud handle");
  expect(handles.find(terrain_id) &&
             handles.find(terrain_id)->kind == sdb::scene::NodeKind::kTerrain,
         "terrain still findable");

  if (g_fails) {
    std::fprintf(stderr, "scene_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "scene_test: ok\n");
  return 0;
}
