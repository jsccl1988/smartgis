// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "render/scene/scene.h"
#include "render/rhi/rhi.h"
#include "sdb/model/model.h"
#include "sdb/model/tileset.h"
#include "sdb/scene/scene.h"

#include "ogrsf_frmts.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// Horizontal ribbon half-width appears as |y| on xyz vertices (stride floats).
bool mesh_y_extent(const render::scene::GpuScene::GpuMesh* mesh, float* min_y,
                   float* max_y) {
  if (!mesh || !mesh->vertex || !min_y || !max_y) {
    return false;
  }
  auto* stub = static_cast<render::rhi::StubBuffer*>(mesh->vertex);
  const size_t stride_floats = mesh->stride / sizeof(float);
  if (stride_floats < 3 || stub->bytes.size() < stride_floats * sizeof(float)) {
    return false;
  }
  const size_t floats = stub->bytes.size() / sizeof(float);
  const float* xyz = reinterpret_cast<const float*>(stub->bytes.data());
  float lo = xyz[1];
  float hi = xyz[1];
  for (size_t i = 0; i + 2 < floats; i += stride_floats) {
    lo = std::min(lo, xyz[i + 1]);
    hi = std::max(hi, xyz[i + 1]);
  }
  *min_y = lo;
  *max_y = hi;
  return true;
}

}  // namespace

int main() {
  // Null / GpuScene sync only. Real FlyCube init/present hangs headless;
  // exercise that path under rhi_test with SMT_RUN_FLYCUBE_GPU=1.
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  sdb::scene::World world;
  world.add_node(sdb::scene::NodeKind::kVectorLayer, "roads", 0, 0, 0, 1, 1, 0);
  world.add_node(sdb::scene::NodeKind::kModel, "cube", 0, 0, 0, 1, 1, 1);

  std::unique_ptr<render::rhi::Device> device(
      render::rhi::create_device(render::rhi::Backend::kNull));

  render::scene::GpuScene gpu;
  gpu.sync_from(world);
  expect(gpu.instance_count() == 2, "two instances");
  expect(gpu.instance_at(0)->kind == sdb::scene::NodeKind::kVectorLayer, "2d");
  expect(gpu.instance_at(1)->kind == sdb::scene::NodeKind::kModel, "3d");

  sdb::model::ModelAsset cube;
  sdb::model::load_unit_cube(cube);
  sdb::scene::World models;
  expect(models.attach_model(&cube, "cube") != nullptr, "attach model asset");
  render::scene::GpuScene model_gpu;
  model_gpu.sync_from(models);
  expect(model_gpu.instance_count() == 1, "model instance");
  expect(model_gpu.instance_at(0)->model == &cube, "model pointer synced");
  model_gpu.set_solid_color(1.f, 0.f, 0.f, 1.f);
  model_gpu.set_view_ortho(0, 0, 10, 10);
  expect(model_gpu.has_view_ortho(), "view ortho set");
  expect(device->initialize(render::rhi::DeviceDesc()), "null device");
  // Fresh lists per record; NullDevice destroy_* intentionally leaks stubs
  // (FlyCube-linked CRT can hang on operator delete).
  render::rhi::CommandList* model_list = device->create_command_list();
  expect(model_gpu.record(device.get(), model_list, 64, 64), "record cube");
  auto* model_stub =
      static_cast<render::rhi::StubCommandList*>(model_list);
  expect(model_stub->draw_indexed_calls >= 1, "cube draw");
  expect(model_stub->set_solid_color_calls >= 1, "solid color on cube");
  expect(model_stub->index_counts.size() >= 1 &&
             model_stub->index_counts[0] == 36,
         "unit cube 36 indices");

  const uint64_t gen = gpu.synced_generation();
  gpu.sync_from(world);
  expect(gpu.synced_generation() == gen, "second sync no-op");
  expect(gpu.instance_count() == 2, "count unchanged");
  render::rhi::CommandList* list = device->create_command_list();
  expect(gpu.record(device.get(), list, 64, 64), "record");
  expect(device->execute(list), "execute");
  auto* stub = static_cast<render::rhi::StubCommandList*>(list);
  expect(stub->closed, "closed after record");

  expect(!gpu.record(device.get(), nullptr, 64, 64), "null list");
  render::rhi::CommandList* list2 = device->create_command_list();
  expect(!gpu.record(device.get(), list2, 0, 64), "zero width");

  // Tileset content → mesh: missing URI keeps AABB (36 indices). With tinygltf
  // and a temp GLB, decoded triangle (3 indices) replaces the AABB bridge.
  {
    const char* ts_json =
        "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
        "\"geometricError\":1,\"content\":{\"uri\":\"missing.glb\"}}}";
    sdb::model::Tileset tileset;
    expect(sdb::model::parse_tileset_json(ts_json, std::strlen(ts_json),
                                          tileset),
           "tileset parse for gpu");
    sdb::scene::World ts_world;
    sdb::scene::Node* ts_node = ts_world.attach_tileset(&tileset, "ts");
    expect(ts_node != nullptr, "attach tileset gpu");
    std::vector<const sdb::model::Tile*> vis;
    vis.push_back(&tileset.root);
    expect(ts_world.apply_tileset_selection(ts_node->id, vis), "select uri");
    render::scene::GpuScene ts_gpu;
    ts_gpu.sync_from(ts_world);
    expect(ts_gpu.instance_count() == 1, "tileset instance");
    expect(ts_gpu.instance_at(0)->visible_uris.size() == 1, "visible uri");
    render::rhi::CommandList* ts_list = device->create_command_list();
    expect(ts_gpu.record(device.get(), ts_list, 64, 64), "record tileset aabb");
    auto* ts_stub = static_cast<render::rhi::StubCommandList*>(ts_list);
    expect(ts_stub->index_counts.size() >= 1 && ts_stub->index_counts[0] == 36,
           "missing content uses AABB 36 indices");

    if (sdb::model::has_tinygltf()) {
      const char* glb_path = "scene_gpu_tile.glb";
      // Minimal TRIANGLES glTF 2.0 GLB (same fixture shape as model_test).
      const char* json =
          "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
          "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],"
          "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":0},"
          "\"indices\":1}]}],"
          "\"accessors\":["
          "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\","
          "\"max\":[1,1,0],\"min\":[0,0,0]},"
          "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}"
          "],"
          "\"bufferViews\":["
          "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
          "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}"
          "],"
          "\"buffers\":[{\"byteLength\":44}]}";
      std::string json_pad(json);
      while (json_pad.size() % 4 != 0) {
        json_pad.push_back(' ');
      }
      std::vector<unsigned char> bin(44, 0);
      const float pos[] = {0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 0.f, 1.f, 0.f};
      std::memcpy(bin.data(), pos, sizeof(pos));
      const uint16_t idx[] = {0, 1, 2};
      std::memcpy(bin.data() + 36, idx, sizeof(idx));
      std::ofstream out(glb_path, std::ios::binary);
      const uint32_t total =
          static_cast<uint32_t>(12 + 8 + json_pad.size() + 8 + bin.size());
      auto write_u32 = [&out](uint32_t v) {
        const unsigned char b[4] = {
            static_cast<unsigned char>(v & 0xffu),
            static_cast<unsigned char>((v >> 8) & 0xffu),
            static_cast<unsigned char>((v >> 16) & 0xffu),
            static_cast<unsigned char>((v >> 24) & 0xffu)};
        out.write(reinterpret_cast<const char*>(b), 4);
      };
      out.write("glTF", 4);
      write_u32(2);
      write_u32(total);
      write_u32(static_cast<uint32_t>(json_pad.size()));
      out.write("JSON", 4);
      out.write(json_pad.data(), static_cast<std::streamsize>(json_pad.size()));
      write_u32(static_cast<uint32_t>(bin.size()));
      out.write("BIN\0", 4);
      out.write(reinterpret_cast<const char*>(bin.data()),
                static_cast<std::streamsize>(bin.size()));
      out.close();

      const char* ts_json2 =
          "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
          "\"geometricError\":1,\"content\":{\"uri\":\"scene_gpu_tile.glb\"}}}";
      sdb::model::Tileset tileset2;
      expect(sdb::model::parse_tileset_json(ts_json2, std::strlen(ts_json2),
                                            tileset2),
             "tileset parse glb");
      sdb::scene::World ts_world2;
      sdb::scene::Node* n2 = ts_world2.attach_tileset(&tileset2, "ts2");
      std::vector<const sdb::model::Tile*> vis2;
      vis2.push_back(&tileset2.root);
      expect(ts_world2.apply_tileset_selection(n2->id, vis2), "select glb");
      render::scene::GpuScene ts_gpu2;
      ts_gpu2.sync_from(ts_world2);
      render::rhi::CommandList* list3 = device->create_command_list();
      expect(ts_gpu2.record(device.get(), list3, 64, 64), "record decoded tile");
      auto* stub3 = static_cast<render::rhi::StubCommandList*>(list3);
      expect(stub3->index_counts.size() >= 1 && stub3->index_counts[0] == 3,
             "decoded tile triangle 3 indices");
      std::remove(glb_path);
    }
  }

  // Style ResolvedPaint → per-mesh solid colors (not one global set_solid_color).
  {
    sdb::model::ModelAsset cube_a;
    sdb::model::ModelAsset cube_b;
    sdb::model::load_unit_cube(cube_a);
    sdb::model::load_unit_cube(cube_b);
    sdb::scene::World paint_world;
    expect(paint_world.attach_model(&cube_a, "fill-cube") != nullptr,
           "attach fill cube");
    expect(paint_world.attach_model(&cube_b, "line-cube") != nullptr,
           "attach line cube");

    render::scene::GpuScene paint_gpu;
    paint_gpu.sync_from(paint_world);
    expect(paint_gpu.instance_count() == 2, "paint two instances");

    sdb::style::ResolvedPaint fill_paint;
    fill_paint.type = sdb::style::LayerType::kFill;
    fill_paint.fill_color = 0xFFFF0000;  // opaque red
    fill_paint.fill_opacity = 1.f;
    sdb::style::ResolvedPaint line_paint;
    line_paint.type = sdb::style::LayerType::kLine;
    line_paint.line_color = 0xFF00FF00;  // opaque green
    line_paint.line_opacity = 0.5f;
    line_paint.line_width = 3.f;
    expect(paint_gpu.set_instance_paint(0, fill_paint), "set fill paint");
    expect(paint_gpu.set_instance_paint(1, line_paint), "set line paint");
    expect(paint_gpu.instance_at(0)->has_paint, "inst0 has paint");
    expect(paint_gpu.instance_at(0)->paint.type == sdb::style::LayerType::kFill,
           "inst0 fill type");
    expect(paint_gpu.instance_at(1)->paint.line_width == 3.f,
           "inst1 line width stored");

    float er = 0;
    float eg = 0;
    float eb = 0;
    float ea = 0;
    render::scene::rgba_from_resolved_paint(fill_paint, &er, &eg, &eb, &ea);
    expect(er > 0.99f && eg < 0.01f && eb < 0.01f && ea > 0.99f,
           "fill paint maps to red");
    render::scene::rgba_from_resolved_paint(line_paint, &er, &eg, &eb, &ea);
    expect(er < 0.01f && eg > 0.99f && eb < 0.01f && ea > 0.49f && ea < 0.51f,
           "line paint maps to green half-alpha");

    render::rhi::CommandList* paint_list = device->create_command_list();
    expect(paint_gpu.record(device.get(), paint_list, 64, 64),
           "record paint cubes");
    auto* paint_stub =
        static_cast<render::rhi::StubCommandList*>(paint_list);
    expect(paint_stub->draw_indexed_calls >= 2, "two painted draws");
    expect(paint_stub->set_solid_color_calls >= 2,
           "per-mesh solid color calls");
    // Last draw is the line layer (green, 0.5 alpha).
    expect(paint_stub->solid_r < 0.01f && paint_stub->solid_g > 0.99f &&
               paint_stub->solid_b < 0.01f && paint_stub->solid_a > 0.49f &&
               paint_stub->solid_a < 0.51f,
           "last solid is line green");

    // Re-record with circle paint on instance 0 → different solid than fill.
    sdb::style::ResolvedPaint circle_paint;
    circle_paint.type = sdb::style::LayerType::kCircle;
    circle_paint.circle_color = 0xFF0000FF;  // blue
    circle_paint.circle_opacity = 1.f;
    circle_paint.circle_radius = 8.f;
    expect(paint_gpu.set_instance_paint(0, circle_paint), "set circle paint");
    expect(paint_gpu.instance_at(0)->paint.circle_radius == 8.f,
           "circle radius stored");
    render::rhi::CommandList* circle_list = device->create_command_list();
    expect(paint_gpu.record(device.get(), circle_list, 64, 64),
           "record circle paint");
    auto* circle_stub =
        static_cast<render::rhi::StubCommandList*>(circle_list);
    // Two meshes: circle (blue) then line (green). Last solid stays green.
    expect(circle_stub->set_solid_color_calls >= 2, "circle record solids");
    expect(circle_stub->solid_g > 0.99f, "order still ends on line green");

    // Single-instance fill vs circle: different StubCommandList solid.
    sdb::scene::World one;
    expect(one.attach_model(&cube_a, "one") != nullptr, "one cube");
    render::scene::GpuScene one_gpu;
    one_gpu.sync_from(one);
    one_gpu.set_instance_paint(0, fill_paint);
    render::rhi::CommandList* one_fill = device->create_command_list();
    expect(one_gpu.record(device.get(), one_fill, 32, 32), "record one fill");
    auto* fill_stub = static_cast<render::rhi::StubCommandList*>(one_fill);
    const float fill_r = fill_stub->solid_r;
    const float fill_g = fill_stub->solid_g;
    const float fill_b = fill_stub->solid_b;
    one_gpu.set_instance_paint(0, circle_paint);
    render::rhi::CommandList* one_circle = device->create_command_list();
    expect(one_gpu.record(device.get(), one_circle, 32, 32),
           "record one circle");
    auto* c_stub = static_cast<render::rhi::StubCommandList*>(one_circle);
    expect(fill_r > 0.99f && fill_g < 0.01f && fill_b < 0.01f, "fill was red");
    expect(c_stub->solid_r < 0.01f && c_stub->solid_g < 0.01f &&
               c_stub->solid_b > 0.99f,
           "circle solid is blue");
    expect(fill_r != c_stub->solid_r || fill_b != c_stub->solid_b,
           "fill and circle solids differ");

    // Background paint drives clear color on empty mesh list path.
    sdb::style::ResolvedPaint bg;
    bg.type = sdb::style::LayerType::kBackground;
    bg.fill_color = 0xFF112233;
    bg.fill_opacity = 1.f;
    render::scene::GpuScene bg_gpu;
    bg_gpu.set_background_paint(bg);
    expect(bg_gpu.has_background_paint(), "background set");
    render::rhi::CommandList* bg_list = device->create_command_list();
    expect(bg_gpu.record(device.get(), bg_list, 16, 16), "record background");
    expect(static_cast<render::rhi::StubCommandList*>(bg_list)->closed,
           "bg list closed");

    // Symbol with in-memory RGBA bytes → textured draw (bind_texture).
    sdb::style::ResolvedPaint symbol_paint;
    symbol_paint.type = sdb::style::LayerType::kSymbol;
    symbol_paint.has_symbol = true;
    symbol_paint.symbol.id = "dot";
    // 2x2 RGBA8 (raw; not PNG).
    const uint8_t rgba[] = {255, 0, 0, 255, 0, 255, 0, 255,
                            0,   0, 255, 255, 255, 255, 0, 255};
    symbol_paint.symbol.bytes.assign(rgba, rgba + sizeof(rgba));
    sdb::scene::World sym_world;
    expect(sym_world.attach_model(&cube_a, "icon") != nullptr, "symbol cube");
    render::scene::GpuScene sym_gpu;
    sym_gpu.sync_from(sym_world);
    expect(sym_gpu.set_instance_paint(0, symbol_paint), "set symbol paint");
    render::rhi::CommandList* sym_list = device->create_command_list();
    expect(sym_gpu.record(device.get(), sym_list, 32, 32), "record symbol");
    auto* sym_stub = static_cast<render::rhi::StubCommandList*>(sym_list);
    expect(sym_stub->bind_texture_calls >= 1, "symbol texture bound");
    expect(sym_stub->last_texture != nullptr, "symbol texture non-null");
    expect(sym_stub->draw_indexed_calls >= 1, "symbol draw");
    // Path-only symbol with missing file must skip (no crash).
    sdb::style::ResolvedPaint path_paint;
    path_paint.type = sdb::style::LayerType::kSymbol;
    path_paint.has_symbol = true;
    path_paint.symbol.path = "definitely_missing_icon_rgba.bin";
    path_paint.fill_color = 0xFFFFFF00;
    path_paint.fill_opacity = 1.f;
    sym_gpu.set_instance_paint(0, path_paint);
    render::rhi::CommandList* path_list = device->create_command_list();
    expect(sym_gpu.record(device.get(), path_list, 32, 32),
           "record missing symbol path");
    auto* path_stub = static_cast<render::rhi::StubCommandList*>(path_list);
    // Falls back to solid tint when texture upload is skipped.
    expect(path_stub->set_solid_color_calls >= 1,
           "missing symbol path uses solid");
  }

  // Paint line_width → LineTessOptions::pixel_width changes ribbon half-width
  // in world units (same topology for butt; observable via vertex Y extent).
  // Round caps also bump index count vs butt for the same width.
  {
    OGRLineString road;
    road.addPoint(0, 0);
    road.addPoint(100, 0);
    const OGRGeometry* geoms[] = {&road};
    sdb::scene::World line_world;
    expect(line_world.attach_vector_geoms("road", geoms, 1) != nullptr,
           "attach road line");

    render::scene::GpuScene line_gpu;
    line_gpu.sync_from(line_world);
    // Envelope width 100 world / 100 px → world_units_per_pixel = 1.
    line_gpu.set_view_ortho(0, -50, 100, 50);

    sdb::style::ResolvedPaint thin;
    thin.type = sdb::style::LayerType::kLine;
    thin.line_color = 0xFFFFFFFF;
    thin.line_opacity = 1.f;
    thin.line_width = 2.f;  // half_world = 0.5 * 2 * 1 = 1
    thin.line_cap = "butt";
    thin.line_join = "miter";
    expect(line_gpu.set_instance_paint(0, thin), "set thin line paint");
    render::rhi::CommandList* thin_list = device->create_command_list();
    expect(line_gpu.record(device.get(), thin_list, 100, 100),
           "record thin line");
    expect(line_gpu.mesh_count() == 1, "thin mesh uploaded");
    const auto* thin_mesh = line_gpu.mesh_at(0);
    expect(thin_mesh != nullptr && thin_mesh->line_width == 2.f,
           "thin line_width on mesh");
    expect(thin_mesh->index_count == 6, "butt segment 6 indices");
    const uint32_t thin_indices = thin_mesh->index_count;
    float thin_miny = 0;
    float thin_maxy = 0;
    expect(mesh_y_extent(thin_mesh, &thin_miny, &thin_maxy), "thin y extent");
    expect(thin_maxy - thin_miny > 1.9f && thin_maxy - thin_miny < 2.1f,
           "thin ribbon height ~2 world");

    sdb::style::ResolvedPaint thick = thin;
    thick.line_width = 8.f;  // half_world = 4 → full height 8
    expect(line_gpu.set_instance_paint(0, thick), "set thick line paint");
    render::rhi::CommandList* thick_list = device->create_command_list();
    expect(line_gpu.record(device.get(), thick_list, 100, 100),
           "record thick line");
    const auto* thick_mesh = line_gpu.mesh_at(0);
    expect(thick_mesh != nullptr && thick_mesh->line_width == 8.f,
           "thick line_width on mesh");
    expect(thick_mesh->index_count == thin_indices,
           "butt topology independent of width");
    const uint32_t thick_indices = thick_mesh->index_count;
    float thick_miny = 0;
    float thick_maxy = 0;
    expect(mesh_y_extent(thick_mesh, &thick_miny, &thick_maxy),
           "thick y extent");
    expect(thick_maxy - thick_miny > 7.9f && thick_maxy - thick_miny < 8.1f,
           "thick ribbon height ~8 world");
    expect((thick_maxy - thick_miny) > (thin_maxy - thin_miny) * 3.5f,
           "thick ribbon taller than thin");

    // Round caps: more indices than butt at same width.
    sdb::style::ResolvedPaint round_paint = thick;
    round_paint.line_cap = "round";
    expect(line_gpu.set_instance_paint(0, round_paint), "set round cap paint");
    render::rhi::CommandList* round_list = device->create_command_list();
    expect(line_gpu.record(device.get(), round_list, 100, 100),
           "record round line");
    const auto* round_mesh = line_gpu.mesh_at(0);
    expect(round_mesh != nullptr && round_mesh->index_count > thick_indices,
           "round caps add indices vs butt");

    // Circle paint on a point: diamond radius scales with circle_radius.
    OGRPoint dot(50, 0);
    const OGRGeometry* pts[] = {&dot};
    sdb::scene::World circle_world;
    expect(circle_world.attach_vector_geoms("dot", pts, 1) != nullptr,
           "attach circle point");
    render::scene::GpuScene circle_gpu;
    circle_gpu.sync_from(circle_world);
    circle_gpu.set_view_ortho(0, -50, 100, 50);
    sdb::style::ResolvedPaint small_c;
    small_c.type = sdb::style::LayerType::kCircle;
    small_c.circle_color = 0xFFFF0000;
    small_c.circle_opacity = 1.f;
    small_c.circle_radius = 2.f;  // world r = 2
    expect(circle_gpu.set_instance_paint(0, small_c), "set small circle");
    render::rhi::CommandList* sc_list = device->create_command_list();
    expect(circle_gpu.record(device.get(), sc_list, 100, 100),
           "record small circle");
    const auto* sc_mesh = circle_gpu.mesh_at(0);
    expect(sc_mesh != nullptr && sc_mesh->index_count == 12,
           "diamond 4 tris / 12 indices");
    float sc_miny = 0;
    float sc_maxy = 0;
    expect(mesh_y_extent(sc_mesh, &sc_miny, &sc_maxy), "small circle extent");
    expect(sc_maxy - sc_miny > 3.9f && sc_maxy - sc_miny < 4.1f,
           "small circle height ~4");

    sdb::style::ResolvedPaint big_c = small_c;
    big_c.circle_radius = 10.f;
    expect(circle_gpu.set_instance_paint(0, big_c), "set big circle");
    render::rhi::CommandList* bc_list = device->create_command_list();
    expect(circle_gpu.record(device.get(), bc_list, 100, 100),
           "record big circle");
    float bc_miny = 0;
    float bc_maxy = 0;
    expect(mesh_y_extent(circle_gpu.mesh_at(0), &bc_miny, &bc_maxy),
           "big circle extent");
    expect(bc_maxy - bc_miny > 19.9f && bc_maxy - bc_miny < 20.1f,
           "big circle height ~20");
    expect((bc_maxy - bc_miny) > (sc_maxy - sc_miny) * 4.5f,
           "big circle larger than small");
  }

  device->shutdown();

  if (g_fails) {
    std::fprintf(stderr, "scene_gpu_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "scene_gpu_test: ok\n");
  return 0;
}
