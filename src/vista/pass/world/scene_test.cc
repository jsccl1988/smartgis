// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/component/world/paint.h"
#include "vista/pass/world/cull/frustum_camera.h"
#include "vista/component/world/cull/prep_cull.h"
#include "vista/component/world/cull/frustum_aabb.h"
#include "vista/component/world/cull/mesh_cull.h"
#include "vista/pass/world/opaque_effect.h"
#include "vista/pass/world/pass.h"
#include "render/graph/frame_graph.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"
#include "vista/assets/model/model.h"
#include "vista/assets/tileset/tileset.h"
#include "vista/component/world/world.h"

#include "ogrsf_frmts.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>
#include "base/process/switches.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool read_constant(const render::rhi::StubCommandList* stub, uint32_t slot,
                   void* out, uint32_t byte_size) {
  const render::rhi::StubCommandList::ConstantRecord* record =
      stub ? stub->constant_at(slot) : nullptr;
  if (!record || !record->has_bytes || record->byte_size != byte_size || !out) {
    return false;
  }
  std::memcpy(out, record->bytes, byte_size);
  return true;
}

// Horizontal ribbon half-width appears as |y| on xyz vertices (stride floats).
bool mesh_y_extent(const vista::WorldPass::GpuMesh* mesh, float* min_y,
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
  // Null / WorldPass sync only. Real FlyCube init/present hangs headless;
  // exercise that path under rhi_test with RUN_FLYCUBE_GPU=1.
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
  // Product default is unlit DEM/paint terrain; P0 lit assertions opt in.
  base::set_switch("scene3d-lit-terrain", "1");

  vista::World world;
  world.add_node(vista::NodeKind::kVectorLayer, "roads", 0, 0, 0, 1, 1, 0);
  world.add_node(vista::NodeKind::kModel, "cube", 0, 0, 0, 1, 1, 1);

  std::unique_ptr<render::rhi::Device> device(
      render::rhi::create_device(render::rhi::Backend::kNull));
  expect(device != nullptr, "create null device");
  expect(device->initialize(render::rhi::DeviceDesc()), "null device");

  vista::WorldPass gpu;
  gpu.sync_from(world);
  expect(gpu.instance_count() == 2, "two instances");
  expect(gpu.instance_at(0)->kind == vista::NodeKind::kVectorLayer, "2d");
  expect(gpu.instance_at(1)->kind == vista::NodeKind::kModel, "3d");

  // SP4: kTerrain AABB mirrors through WorldPass::sync_from; CPU mesh / AABB
  // fallback upload + draw (DEM adapter seeds World in dem_stereo_test).
  {
    vista::World terrain_world;
    vista::Node* terrain = terrain_world.attach_terrain("china_dem", 73.0, 17.5,
                                                      0.1, 135.0, 54.0, 8.0);
    expect(terrain != nullptr, "attach terrain");
    vista::WorldPass terrain_gpu;
    terrain_gpu.sync_from(terrain_world);
    expect(terrain_gpu.instance_count() == 1, "terrain instance");
    expect(terrain_gpu.instance_at(0)->kind == vista::NodeKind::kTerrain,
           "terrain kind");
    expect(terrain_gpu.instance_at(0)->node_id == terrain->id, "terrain id");
    expect(terrain_gpu.instance_at(0)->min_x == 73.0 &&
               terrain_gpu.instance_at(0)->max_x == 135.0,
           "terrain lon");

    // AABB-only terrain still uploads a box mesh (36 indices).
    render::rhi::CommandList* aabb_list = device->create_command_list();
    expect(terrain_gpu.record(device.get(), aabb_list, 64, 64),
           "record terrain aabb");
    auto* aabb_stub =
        static_cast<render::rhi::StubCommandList*>(aabb_list);
    expect(aabb_stub->draw_indexed_calls >= 1, "terrain aabb draw");
    expect(aabb_stub->index_counts.size() >= 1 &&
               aabb_stub->index_counts[0] == 36,
           "terrain aabb 36 indices");

    // Explicit CPU mesh (tiny quad) replaces AABB on next sync.
    const float positions[] = {
        0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f, 1.f,
    };
    const uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    expect(terrain_world.set_terrain_mesh(terrain->id, positions, 12, indices,
                                          6),
           "set terrain mesh");
    expect(terrain->has_terrain_mesh(), "node has terrain mesh");
    terrain_gpu.sync_from(terrain_world);
    expect(terrain_gpu.instance_at(0)->terrain.indices.size() == 6,
           "synced terrain indices");
    render::rhi::CommandList* mesh_list = device->create_command_list();
    expect(terrain_gpu.record(device.get(), mesh_list, 64, 64),
           "record terrain mesh");
    auto* mesh_stub =
        static_cast<render::rhi::StubCommandList*>(mesh_list);
    expect(mesh_stub->draw_indexed_calls >= 1, "terrain mesh draw");
    expect(mesh_stub->index_counts.size() >= 1 &&
               mesh_stub->index_counts[0] == 6,
           "terrain mesh 6 indices");
    // P0 Task 3: 3D lit solid + POS+NORMAL stride.
    expect(mesh_stub->set_pipeline_calls >= 1, "terrain lit set_pipeline");
    expect(mesh_stub->last_pipeline == terrain_gpu.lit_pipeline(),
           "terrain pipeline is lit");
    render::programs::Light terrain_light{};
    const render::programs::Light light_defaults{};
    expect(mesh_stub->set_constants_calls >= 1, "terrain set_constants");
    expect(read_constant(mesh_stub, render::programs::kLightSlot, &terrain_light,
                         sizeof(terrain_light)) &&
               terrain_light.dir_x == light_defaults.dir_x &&
               terrain_light.dir_y == light_defaults.dir_y &&
               terrain_light.dir_z == light_defaults.dir_z &&
               terrain_light.ambient == light_defaults.ambient &&
               terrain_light.color_r == light_defaults.color_r &&
               terrain_light.color_g == light_defaults.color_g &&
               terrain_light.color_b == light_defaults.color_b &&
               terrain_light.intensity == light_defaults.intensity,
           "terrain light constant defaults");
    expect(terrain_gpu.mesh_count() >= 1, "terrain mesh uploaded");
    expect(terrain_gpu.mesh_at(0)->stride == 6 * sizeof(float),
           "terrain lit stride 6 floats");

    // SP4 knife 3: external orbit camera on record.
    const render::rhi::CameraMatrices orbit =
        render::rhi::make_orbit_camera(0.5f, 0.3f, 4.f, 0.785398f, 1.f, 0.1f,
                                       100.f);
    terrain_gpu.set_view_camera(orbit);
    expect(terrain_gpu.has_view_camera(), "view camera set");
    render::rhi::CommandList* orbit_list = device->create_command_list();
    expect(terrain_gpu.record(device.get(), orbit_list, 64, 64),
           "record with orbit camera");
    auto* orbit_stub =
        static_cast<render::rhi::StubCommandList*>(orbit_list);
    expect(orbit_stub->bind_camera_calls >= 1, "orbit bind_camera");
    terrain_gpu.clear_view_camera();
    expect(!terrain_gpu.has_view_camera(), "view camera cleared");
  }

  vista::ModelAsset cube;
  vista::load_unit_cube(cube);
  vista::World models;
  expect(models.attach_model(&cube, "cube") != nullptr, "attach model asset");
  vista::WorldPass model_gpu;
  model_gpu.sync_from(models);
  expect(model_gpu.instance_count() == 1, "model instance");
  expect(model_gpu.instance_at(0)->model == &cube, "model pointer synced");
  model_gpu.set_solid_color(1.f, 0.f, 0.f, 1.f);
  model_gpu.set_view_ortho(0, 0, 10, 10);
  expect(model_gpu.has_view_ortho(), "view ortho set");
  // Fresh lists per record; NullDevice destroy_* intentionally leaks stubs
  // (FlyCube-linked CRT can hang on operator delete).
  render::rhi::CommandList* model_list = device->create_command_list();
  expect(model_gpu.record(device.get(), model_list, 64, 64), "record cube");
  auto* model_stub =
      static_cast<render::rhi::StubCommandList*>(model_list);
  expect(model_stub->draw_indexed_calls >= 1, "cube draw");
  render::programs::Color cube_color{};
  expect(model_stub->set_constants_calls >= 1, "solid color on cube");
  expect(read_constant(model_stub, render::programs::kColorSlot, &cube_color,
                       sizeof(cube_color)) &&
             cube_color.r == 1.f && cube_color.g == 0.f && cube_color.b == 0.f &&
             cube_color.a == 1.f,
         "cube color constant");
  expect(model_stub->last_pipeline == model_gpu.lit_pipeline(),
         "model pipeline is lit");
  render::programs::Light model_light{};
  expect(read_constant(model_stub, render::programs::kLightSlot, &model_light,
                       sizeof(model_light)) &&
             model_light.ambient == 0.42f && model_light.intensity == 1.15f,
         "model light constant");
  expect(model_gpu.mesh_count() >= 1 &&
             model_gpu.mesh_at(0)->stride == 6 * sizeof(float),
         "model lit stride 6 floats");
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

  // Tileset content â mesh: missing URI keeps AABB (36 indices). With tinygltf
  // and a temp GLB, decoded triangle (3 indices) replaces the AABB bridge.
  {
    const char* ts_json =
        "{\"root\":{\"boundingVolume\":{\"box\":[0,0,0,1,0,0,0,1,0,0,0,1]},"
        "\"geometricError\":1,\"content\":{\"uri\":\"missing.glb\"}}}";
    vista::Tileset tileset;
    expect(vista::parse_tileset_json(ts_json, std::strlen(ts_json),
                                          tileset),
           "tileset parse for gpu");
    vista::World ts_world;
    vista::Node* ts_node = ts_world.attach_tileset(&tileset, "ts");
    expect(ts_node != nullptr, "attach tileset gpu");
    std::vector<const vista::Tile*> vis;
    vis.push_back(&tileset.root);
    expect(ts_world.apply_tileset_selection(ts_node->id, vis), "select uri");
    vista::WorldPass ts_gpu;
    ts_gpu.sync_from(ts_world);
    expect(ts_gpu.instance_count() == 1, "tileset instance");
    expect(ts_gpu.instance_at(0)->visible_uris.size() == 1, "visible uri");
    render::rhi::CommandList* ts_list = device->create_command_list();
    expect(ts_gpu.record(device.get(), ts_list, 64, 64), "record tileset aabb");
    auto* ts_stub = static_cast<render::rhi::StubCommandList*>(ts_list);
    expect(ts_stub->index_counts.size() >= 1 && ts_stub->index_counts[0] == 36,
           "missing content uses AABB 36 indices");

    if (vista::has_tinygltf()) {
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
      vista::Tileset tileset2;
      expect(vista::parse_tileset_json(ts_json2, std::strlen(ts_json2),
                                            tileset2),
             "tileset parse glb");
      vista::World ts_world2;
      vista::Node* n2 = ts_world2.attach_tileset(&tileset2, "ts2");
      std::vector<const vista::Tile*> vis2;
      vis2.push_back(&tileset2.root);
      expect(ts_world2.apply_tileset_selection(n2->id, vis2), "select glb");
      vista::WorldPass ts_gpu2;
      ts_gpu2.sync_from(ts_world2);
      render::rhi::CommandList* list3 = device->create_command_list();
      expect(ts_gpu2.record(device.get(), list3, 64, 64), "record decoded tile");
      auto* stub3 = static_cast<render::rhi::StubCommandList*>(list3);
      expect(stub3->index_counts.size() >= 1 && stub3->index_counts[0] == 3,
             "decoded tile triangle 3 indices");
      std::remove(glb_path);
    }
  }

  // Style ResolvedPaint â per-mesh solid colors (not one global set_solid_color).
  {
    vista::ModelAsset cube_a;
    vista::ModelAsset cube_b;
    vista::load_unit_cube(cube_a);
    vista::load_unit_cube(cube_b);
    vista::World paint_world;
    expect(paint_world.attach_model(&cube_a, "fill-cube") != nullptr,
           "attach fill cube");
    expect(paint_world.attach_model(&cube_b, "line-cube") != nullptr,
           "attach line cube");

    vista::WorldPass paint_gpu;
    paint_gpu.sync_from(paint_world);
    expect(paint_gpu.instance_count() == 2, "paint two instances");

    gis::style::ResolvedPaint fill_paint;
    fill_paint.type = gis::style::LayerType::kFill;
    fill_paint.fill_color = 0xFFFF0000;  // opaque red
    fill_paint.fill_opacity = 1.f;
    gis::style::ResolvedPaint line_paint;
    line_paint.type = gis::style::LayerType::kLine;
    line_paint.line_color = 0xFF00FF00;  // opaque green
    line_paint.line_opacity = 0.5f;
    line_paint.line_width = 3.f;
    expect(paint_gpu.set_instance_paint(0, fill_paint), "set fill paint");
    expect(paint_gpu.set_instance_paint(1, line_paint), "set line paint");
    expect(paint_gpu.instance_at(0)->has_paint, "inst0 has paint");
    expect(paint_gpu.instance_at(0)->paint.type == gis::style::LayerType::kFill,
           "inst0 fill type");
    expect(paint_gpu.instance_at(1)->paint.line_width == 3.f,
           "inst1 line width stored");

    float er = 0;
    float eg = 0;
    float eb = 0;
    float ea = 0;
    vista::rgba_from_resolved_paint(fill_paint, &er, &eg, &eb, &ea);
    expect(er > 0.99f && eg < 0.01f && eb < 0.01f && ea > 0.99f,
           "fill paint maps to red");
    vista::rgba_from_resolved_paint(line_paint, &er, &eg, &eb, &ea);
    expect(er < 0.01f && eg > 0.99f && eb < 0.01f && ea > 0.49f && ea < 0.51f,
           "line paint maps to green half-alpha");

    render::rhi::CommandList* paint_list = device->create_command_list();
    expect(paint_gpu.record(device.get(), paint_list, 64, 64),
           "record paint cubes");
    auto* paint_stub =
        static_cast<render::rhi::StubCommandList*>(paint_list);
    expect(paint_stub->draw_indexed_calls >= 2, "two painted draws");
    render::programs::Color paint_color{};
    expect(paint_stub->set_constants_calls >= 2, "per-mesh color constants");
    expect(read_constant(paint_stub, render::programs::kColorSlot, &paint_color,
                         sizeof(paint_color)),
           "color slot recorded");
    // Last draw is the line layer (green, 0.5 alpha).
    expect(paint_color.r < 0.01f && paint_color.g > 0.99f &&
               paint_color.b < 0.01f && paint_color.a > 0.49f &&
               paint_color.a < 0.51f,
           "last solid is line green");

    // Re-record with circle paint on instance 0 â different solid than fill.
    gis::style::ResolvedPaint circle_paint;
    circle_paint.type = gis::style::LayerType::kCircle;
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
    // Two meshes: circle (blue) then line (green). Last color stays green.
    render::programs::Color circle_color{};
    expect(circle_stub->set_constants_calls >= 2, "circle record colors");
    expect(read_constant(circle_stub, render::programs::kColorSlot,
                         &circle_color, sizeof(circle_color)) &&
               circle_color.g > 0.99f,
           "order still ends on line green");

    // Single-instance fill vs circle: different StubCommandList solid.
    vista::World one;
    expect(one.attach_model(&cube_a, "one") != nullptr, "one cube");
    vista::WorldPass one_gpu;
    one_gpu.sync_from(one);
    one_gpu.set_instance_paint(0, fill_paint);
    render::rhi::CommandList* one_fill = device->create_command_list();
    expect(one_gpu.record(device.get(), one_fill, 32, 32), "record one fill");
    auto* fill_stub = static_cast<render::rhi::StubCommandList*>(one_fill);
    render::programs::Color fill_color{};
    expect(read_constant(fill_stub, render::programs::kColorSlot, &fill_color,
                         sizeof(fill_color)),
           "fill color slot");
    one_gpu.set_instance_paint(0, circle_paint);
    render::rhi::CommandList* one_circle = device->create_command_list();
    expect(one_gpu.record(device.get(), one_circle, 32, 32),
           "record one circle");
    auto* c_stub = static_cast<render::rhi::StubCommandList*>(one_circle);
    render::programs::Color one_circle_color{};
    expect(read_constant(c_stub, render::programs::kColorSlot, &one_circle_color,
                         sizeof(one_circle_color)),
           "circle color slot");
    expect(fill_color.r > 0.99f && fill_color.g < 0.01f && fill_color.b < 0.01f,
           "fill was red");
    expect(one_circle_color.r < 0.01f && one_circle_color.g < 0.01f &&
               one_circle_color.b > 0.99f,
           "circle solid is blue");
    expect(fill_color.r != one_circle_color.r ||
               fill_color.b != one_circle_color.b,
           "fill and circle solids differ");

    // Background paint drives clear color on empty mesh list path.
    gis::style::ResolvedPaint bg;
    bg.type = gis::style::LayerType::kBackground;
    bg.fill_color = 0xFF112233;
    bg.fill_opacity = 1.f;
    vista::WorldPass bg_gpu;
    bg_gpu.set_background_paint(bg);
    expect(bg_gpu.has_background_paint(), "background set");
    render::rhi::CommandList* bg_list = device->create_command_list();
    expect(bg_gpu.record(device.get(), bg_list, 16, 16), "record background");
    expect(static_cast<render::rhi::StubCommandList*>(bg_list)->closed,
           "bg list closed");

    // Symbol with in-memory RGBA bytes â textured draw (bind_texture).
    gis::style::ResolvedPaint symbol_paint;
    symbol_paint.type = gis::style::LayerType::kSymbol;
    symbol_paint.has_symbol = true;
    symbol_paint.symbol.id = "dot";
    // 2x2 RGBA8 (raw; not PNG).
    const uint8_t rgba[] = {255, 0, 0, 255, 0, 255, 0, 255,
                            0,   0, 255, 255, 255, 255, 0, 255};
    symbol_paint.symbol.bytes.assign(rgba, rgba + sizeof(rgba));
    vista::World sym_world;
    expect(sym_world.attach_model(&cube_a, "icon") != nullptr, "symbol cube");
    vista::WorldPass sym_gpu;
    sym_gpu.sync_from(sym_world);
    expect(sym_gpu.set_instance_paint(0, symbol_paint), "set symbol paint");
    render::rhi::CommandList* sym_list = device->create_command_list();
    expect(sym_gpu.record(device.get(), sym_list, 32, 32), "record symbol");
    auto* sym_stub = static_cast<render::rhi::StubCommandList*>(sym_list);
    expect(sym_stub->bind_texture_calls >= 1, "symbol texture bound");
    expect(sym_stub->last_texture != nullptr, "symbol texture non-null");
    expect(sym_stub->draw_indexed_calls >= 1, "symbol draw");
    expect(sym_stub->last_pipeline == sym_gpu.textured_pipeline(),
           "symbol uses textured pipeline");
    // Path-only symbol with missing file must skip (no crash).
    gis::style::ResolvedPaint path_paint;
    path_paint.type = gis::style::LayerType::kSymbol;
    path_paint.has_symbol = true;
    path_paint.symbol.path = "definitely_missing_icon_rgba.bin";
    path_paint.fill_color = 0xFFFFFF00;
    path_paint.fill_opacity = 1.f;
    sym_gpu.set_instance_paint(0, path_paint);
    render::rhi::CommandList* path_list = device->create_command_list();
    expect(sym_gpu.record(device.get(), path_list, 32, 32),
           "record missing symbol path");
    auto* path_stub = static_cast<render::rhi::StubCommandList*>(path_list);
    // Falls back to a color constant when texture upload is skipped.
    expect(path_stub->set_constants_calls >= 1,
           "missing symbol path uses color constant");
    expect(path_stub->last_pipeline == sym_gpu.lit_pipeline(),
           "missing symbol path stays lit");
  }

  // Paint line_width â LineTessOptions::pixel_width changes ribbon half-width
  // in world units (same topology for butt; observable via vertex Y extent).
  // Round caps also bump index count vs butt for the same width.
  {
    OGRLineString road;
    road.addPoint(0, 0);
    road.addPoint(100, 0);
    const OGRGeometry* geoms[] = {&road};
    vista::World line_world;
    expect(line_world.attach_vector_geoms("road", geoms, 1) != nullptr,
           "attach road line");

    vista::WorldPass line_gpu;
    line_gpu.sync_from(line_world);
    // Envelope width 100 world / 100 px â world_units_per_pixel = 1.
    line_gpu.set_view_ortho(0, -50, 100, 50);

    gis::style::ResolvedPaint thin;
    thin.type = gis::style::LayerType::kLine;
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

    gis::style::ResolvedPaint thick = thin;
    thick.line_width = 8.f;  // half_world = 4 â full height 8
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
    gis::style::ResolvedPaint round_paint = thick;
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
    vista::World circle_world;
    expect(circle_world.attach_vector_geoms("dot", pts, 1) != nullptr,
           "attach circle point");
    vista::WorldPass circle_gpu;
    circle_gpu.sync_from(circle_world);
    circle_gpu.set_view_ortho(0, -50, 100, 50);
    gis::style::ResolvedPaint small_c;
    small_c.type = gis::style::LayerType::kCircle;
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

    gis::style::ResolvedPaint big_c = small_c;
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

  // P0 Task 3: 3D lit + paint albedo on GpuMesh.solid_*; 2D stays non-lit.
  {
    vista::World lit_world;
    vista::Node* terrain = lit_world.attach_terrain("lit_dem", 0.0, 0.0, 0.0, 1.0,
                                                  1.0, 1.0);
    expect(terrain != nullptr, "attach lit terrain");
    const float positions[] = {
        0.f, 0.f, 0.f, 1.f, 0.f, 0.f, 1.f, 0.f, 1.f, 0.f, 0.f, 1.f,
    };
    const uint32_t indices[] = {0, 1, 2, 0, 2, 3};
    expect(lit_world.set_terrain_mesh(terrain->id, positions, 12, indices, 6),
           "set lit terrain mesh");

    vista::WorldPass lit_gpu;
    lit_gpu.sync_from(lit_world);
    gis::style::ResolvedPaint fill;
    fill.type = gis::style::LayerType::kFill;
    fill.fill_color = 0xFF3366CC;  // opaque blue-ish
    fill.fill_opacity = 1.f;
    expect(lit_gpu.set_instance_paint(0, fill), "set terrain paint");

    float er = 0;
    float eg = 0;
    float eb = 0;
    float ea = 0;
    vista::rgba_from_resolved_paint(fill, &er, &eg, &eb, &ea);

    render::rhi::CommandList* lit_list = device->create_command_list();
    expect(lit_gpu.record(device.get(), lit_list, 64, 64), "record lit paint");
    auto* lit_stub = static_cast<render::rhi::StubCommandList*>(lit_list);
    expect(lit_stub->last_pipeline == lit_gpu.lit_pipeline(),
           "lit paint pipeline");
    render::programs::Light lit_light{};
    expect(lit_stub->set_constants_calls >= 1, "lit paint constants");
    expect(read_constant(lit_stub, render::programs::kLightSlot, &lit_light,
                         sizeof(lit_light)) &&
               lit_light.ambient == 0.42f,
           "lit paint light");
    expect(lit_gpu.mesh_count() >= 1, "lit paint mesh");
    const vista::WorldPass::GpuMesh* lit_mesh = lit_gpu.mesh_at(0);
    expect(lit_mesh && lit_mesh->solid_r > er - 0.02f &&
               lit_mesh->solid_r < er + 0.02f &&
               lit_mesh->solid_g > eg - 0.02f &&
               lit_mesh->solid_g < eg + 0.02f &&
               lit_mesh->solid_b > eb - 0.02f &&
               lit_mesh->solid_b < eb + 0.02f &&
               lit_mesh->solid_a > ea - 0.02f &&
               lit_mesh->solid_a < ea + 0.02f,
           "set_instance_paint albedo on GpuMesh.solid_*");
    expect(lit_mesh->stride == 6 * sizeof(float), "lit paint stride");
    // Interleaved NORMAL after POSITION: first face is in XY (z varies) so
    // generated normal should be mostly +Y or -Y (not zero).
    auto* vb = static_cast<render::rhi::StubBuffer*>(lit_mesh->vertex);
    expect(vb && vb->bytes.size() >= 6 * sizeof(float), "lit vb bytes");
    if (vb && vb->bytes.size() >= 6 * sizeof(float)) {
      const float* v0 = reinterpret_cast<const float*>(vb->bytes.data());
      const float nx = v0[3];
      const float ny = v0[4];
      const float nz = v0[5];
      const float len2 = nx * nx + ny * ny + nz * nz;
      expect(len2 > 0.5f && len2 < 1.5f, "generated normal near unit length");
    }

    // 2D vector record must not switch to kLitSolid.
    OGRLineString road;
    road.addPoint(0.0, 0.0);
    road.addPoint(10.0, 0.0);
    const OGRGeometry* geoms[] = {&road};
    vista::World vec_world;
    expect(vec_world.attach_vector_geoms("poly2d", geoms, 1) != nullptr,
           "attach 2d");
    vista::WorldPass vec_gpu;
    vec_gpu.sync_from(vec_world);
    render::rhi::CommandList* vec_list = device->create_command_list();
    expect(vec_gpu.record(device.get(), vec_list, 64, 64), "record 2d");
    auto* vec_stub = static_cast<render::rhi::StubCommandList*>(vec_list);
    expect(vec_stub->last_pipeline == vec_gpu.solid_pipeline(),
           "2d uses solid pipeline");
    expect(vec_stub->last_pipeline != vec_gpu.lit_pipeline(),
           "2d does not use lit pipeline");
    if (vec_gpu.mesh_count() >= 1) {
      expect(vec_gpu.mesh_at(0)->stride == 3 * sizeof(float) ||
                 vec_gpu.mesh_at(0)->stride == 5 * sizeof(float),
             "2d stride remains pos or pos+uv");
    }
  }

  // P0 Task 4: CPU frustum vs AABB (pure + WorldPass â¥64 instances).
  {
    float identity[16];
    render::rhi::set_identity4(identity);
    const vista::FrustumPlanes id_planes =
        vista::extract_frustum_planes(identity, identity);
    expect(vista::aabb_intersects_frustum(-1.f, -1.f, -1.f, 1.f, 1.f,
                                                  1.f, id_planes),
           "unit aabb intersects identity frustum");
    expect(!vista::aabb_intersects_frustum(10.f, 10.f, 10.f, 11.f, 11.f,
                                                   11.f, id_planes),
           "far aabb culled by identity frustum");

    const render::rhi::CameraMatrices persp =
        render::rhi::make_perspective_camera(0.785398f, 1.f, 0.1f, 100.f);
    const vista::FrustumPlanes persp_planes =
        vista::extract_frustum_planes(persp);
    expect(vista::aabb_intersects_frustum(-0.5f, -0.5f, -0.5f, 0.5f,
                                                  0.5f, 0.5f, persp_planes),
           "origin aabb visible to default perspective");
    expect(!vista::aabb_intersects_frustum(500.f, -0.5f, -0.5f, 501.f,
                                                   0.5f, 0.5f, persp_planes),
           "x=500 aabb culled by default perspective");

    // Product default keeps frustum cull off (Scene3d DEM AABB mismatch).
    // This block opts in so WorldPass draw counts exercise the cull path.
    const char* prev_cull = base::switch_cstr("scene3d-frustum-cull");
    const std::string saved_cull = prev_cull ? prev_cull : "";
    const char* prev_no_cull = base::switch_cstr("scene3d-no-cull");
    const std::string saved_no_cull = prev_no_cull ? prev_no_cull : "";
    base::set_switch("scene3d-frustum-cull", "1");
    base::set_switch("scene3d-no-cull", "");

    vista::World cull_world;
    constexpr int kHalf = 32;
    constexpr int kTotal = 64;
    for (int i = 0; i < kTotal; ++i) {
      char name[32];
      std::snprintf(name, sizeof(name), "cull_%d", i);
      if (i < kHalf) {
        const double x = (i % 4) * 0.4 - 0.6;
        const double y = ((i / 4) % 4) * 0.4 - 0.6;
        const double z = (i / 16) * 0.3 - 0.3;
        cull_world.add_node(vista::NodeKind::kTerrain, name, x, y, z, x + 0.3,
                            y + 0.3, z + 0.3);
      } else {
        const double x = 400.0 + static_cast<double>(i - kHalf) * 2.0;
        cull_world.add_node(vista::NodeKind::kTerrain, name, x, -0.2, -0.2,
                            x + 0.5, 0.2, 0.2);
      }
    }
    vista::WorldPass cull_gpu;
    cull_gpu.sync_from(cull_world);
    expect(cull_gpu.instance_count() ==
               static_cast<size_t>(kTotal),
           "64 cull instances");

    render::rhi::CommandList* full_list = device->create_command_list();
    expect(cull_gpu.record(device.get(), full_list, 64, 64),
           "record 64 without camera cull");
    auto* full_stub =
        static_cast<render::rhi::StubCommandList*>(full_list);
    expect(full_stub->draw_indexed_calls ==
               static_cast<uint32_t>(kTotal),
           "no camera: draw all 64");

    const render::rhi::CameraMatrices orbit =
        render::rhi::make_orbit_camera(0.f, 0.25f, 4.f, 0.785398f, 1.f, 0.1f,
                                       100.f);
    cull_gpu.set_view_camera(orbit);
    render::rhi::CommandList* cull_list = device->create_command_list();
    expect(cull_gpu.record(device.get(), cull_list, 64, 64),
           "record 64 with frustum cull");
    auto* cull_stub =
        static_cast<render::rhi::StubCommandList*>(cull_list);
    expect(cull_stub->draw_indexed_calls <
               static_cast<uint32_t>(kTotal),
           "frustum cull drops some draws");
    expect(cull_stub->draw_indexed_calls >=
               static_cast<uint32_t>(kHalf) - 4u,
           "near-half still drawn");
    expect(cull_stub->draw_indexed_calls <=
               static_cast<uint32_t>(kHalf) + 4u,
           "far-half mostly culled");

    // Wide orbit still includes near boxes; far boxes stay out.
    const render::rhi::CameraMatrices wide =
        render::rhi::make_orbit_camera(0.f, 0.f, 3.f, 1.2f, 1.f, 0.1f, 200.f);
    cull_gpu.set_view_camera(wide);
    render::rhi::CommandList* wide_list = device->create_command_list();
    expect(cull_gpu.record(device.get(), wide_list, 64, 64),
           "record wide frustum");
    auto* wide_stub =
        static_cast<render::rhi::StubCommandList*>(wide_list);
    expect(wide_stub->draw_indexed_calls >=
               static_cast<uint32_t>(kHalf) - 2u,
           "wide frustum keeps near boxes");
    expect(wide_stub->draw_indexed_calls <
               static_cast<uint32_t>(kTotal),
           "wide frustum still culls far boxes");

    // Prep parallel + cull: same draw counts as serial cull (workers only
    // write visible[]; record stays serial).
    const char* prev_prep = base::switch_cstr("gpuscene-prep-parallel");
    const std::string saved_prep = prev_prep ? prev_prep : "";
    base::set_switch("gpuscene-prep-parallel", "1");
    expect(vista::detail::prep_parallel_effective_workers(true) ==
               vista::detail::prep_parallel_requested_workers(),
           "prep_par + cull â effective == requested");
    render::rhi::CommandList* prep_list = device->create_command_list();
    expect(cull_gpu.record(device.get(), prep_list, 64, 64),
           "record with prep_par + cull");
    auto* prep_stub =
        static_cast<render::rhi::StubCommandList*>(prep_list);
    expect(prep_stub->draw_indexed_calls == wide_stub->draw_indexed_calls,
           "prep_par cull matches serial cull draws");
    base::set_switch("gpuscene-prep-parallel", saved_prep.c_str());

    cull_gpu.clear_view_camera();

    base::set_switch("scene3d-frustum-cull", saved_cull.c_str());
    base::set_switch("scene3d-no-cull", saved_no_cull.c_str());
  }

  // Honesty: GPUSCENE_PREP_PARALLEL alone never enables workers.
  {
    const char* prev_cull = base::switch_cstr("scene3d-frustum-cull");
    const std::string saved_cull = prev_cull ? prev_cull : "";
    const char* prev_prep = base::switch_cstr("gpuscene-prep-parallel");
    const std::string saved_prep = prev_prep ? prev_prep : "";
    base::set_switch("scene3d-frustum-cull", "");
    base::set_switch("gpuscene-prep-parallel", "");
    expect(vista::detail::prep_parallel_requested_workers() == 1,
           "default prep_par off â N=1");
    expect(vista::detail::prep_parallel_effective_workers(false) == 1,
           "cull off â effective N=1");
    base::set_switch("gpuscene-prep-parallel", "1");
    expect(vista::detail::prep_parallel_requested_workers() >= 1,
           "prep_par=1 requests >= 1");
    expect(vista::detail::prep_parallel_effective_workers(false) == 1,
           "prep_par=1 without cull â still N=1");
    std::vector<vista::MeshCullItem> empty_meshes(4);
    std::vector<uint8_t> visible;
    vista::detail::prep_cull_meshes(empty_meshes, nullptr, &visible);
    expect(visible.size() == 4 && visible[0] == 1 && visible[3] == 1,
           "prep without cull leaves all visible");
    base::set_switch("scene3d-frustum-cull", saved_cull.c_str());
    base::set_switch("gpuscene-prep-parallel", saved_prep.c_str());
  }

  {
    vista::WorldPass empty_scene;
    vista::OpaqueEffect opaque_scene(&empty_scene);
    render::graph::ViewInput scene_only;
    scene_only.width_px = 64;
    scene_only.height_px = 64;
    scene_only.effects.push_back(&opaque_scene);
    expect(render::graph::present(device.get(), scene_only),
           "empty WorldPass opaque effect presents");
  }

  device->shutdown();

  if (g_fails) {
    std::fprintf(stderr, "scene_gpu_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "scene_gpu_test: ok\n");
  std::fflush(stdout);
  // FlyCube-linked CRT can hang in static destructors / operator delete after
  // NullDevice intentionally leaks stubs. Exit without running atexit/dtors.
  std::_Exit(0);
}
