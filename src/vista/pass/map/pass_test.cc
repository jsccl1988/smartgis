// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/map/pass.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "vista/pass/map/detail/atlas.h"
#include "vista/component/map/place/place.h"
#include "vista/pass/map/detail/upload.h"
#include "vista/component/map/shade/multiply.h"
#include "render/programs/programs.h"
#include "render/rhi/rhi.h"

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// Opaque 4x4 white glyph. advance_px = text_size_px * 0.5. No window.
class StubGlyphRasterizer : public vista::GlyphRasterizer {
 public:
  bool rasterize(uint32_t, float text_size_px, Bitmap* out) override {
    if (!out) {
      return false;
    }
    out->width = 4;
    out->height = 4;
    out->rgba.assign(4u * 4u * 4u, 255);
    out->advance_px = text_size_px * 0.5f;
    return true;
  }
};

class FailingGlyphRasterizer : public vista::GlyphRasterizer {
 public:
  bool rasterize(uint32_t, float, Bitmap* out) override {
    if (out) {
      out->width = 0;
      out->height = 0;
      out->rgba.clear();
      out->advance_px = 0.f;
    }
    return false;
  }
};

vista::View view_64() {
  vista::View view;
  view.width_px = 64;
  view.height_px = 64;
  view.min_x = 0;
  view.min_y = 0;
  view.max_x = 10;
  view.max_y = 10;
  return view;
}

vista::DrawItem fill_triangle() {
  vista::DrawItem item;
  item.kind = vista::DrawKind::kFill;
  item.vertices = {
      {0.f, 0.f, 0.f, 0.f, 0.f},
      {1.f, 0.f, 0.f, 0.f, 0.f},
      {0.f, 1.f, 0.f, 0.f, 0.f},
  };
  item.indices = {0, 1, 2};
  item.rgba = 0xFFE24B4Bu;
  item.opacity = 1.f;
  item.pixel_space = false;
  return item;
}

vista::DrawItem text_quad() {
  vista::DrawItem item;
  item.kind = vista::DrawKind::kText;
  item.vertices = {
      {8.f, 8.f, 0.f, 0.f, 0.f},
      {24.f, 8.f, 0.f, 1.f, 0.f},
      {24.f, 24.f, 0.f, 1.f, 1.f},
      {8.f, 24.f, 0.f, 0.f, 1.f},
  };
  item.indices = {0, 1, 2, 0, 2, 3};
  item.rgba = 0xFF111111u;
  item.opacity = 1.f;
  item.codepoint = 65;
  item.text_size_px = 16.f;
  item.halo_width_px = 2.f;
  item.halo_rgba = 0xFFFFFFFFu;
  item.pixel_space = true;
  item.angle_rad = 0.3f;
  item.anchor_x = 16.f;
  item.anchor_y = 16.f;
  return item;
}

vista::DrawItem raster_quad() {
  vista::DrawItem item;
  item.kind = vista::DrawKind::kRaster;
  item.vertices = {
      {0.f, 0.f, 0.f, 0.f, 0.f},
      {1.f, 0.f, 0.f, 1.f, 0.f},
      {1.f, 1.f, 0.f, 1.f, 1.f},
      {0.f, 1.f, 0.f, 0.f, 1.f},
  };
  item.indices = {0, 1, 2, 0, 2, 3};
  item.codepoint = 7;
  item.opacity = 1.f;
  item.pixel_space = false;
  return item;
}

vista::DrawItem icon_quad() {
  vista::DrawItem item;
  item.kind = vista::DrawKind::kIcon;
  item.vertices = {
      {4.f, 4.f, 0.f, 0.f, 0.f},
      {20.f, 4.f, 0.f, 1.f, 0.f},
      {20.f, 20.f, 0.f, 1.f, 1.f},
      {4.f, 20.f, 0.f, 0.f, 1.f},
  };
  item.indices = {0, 1, 2, 0, 2, 3};
  item.symbol_id = "pin";
  item.opacity = 1.f;
  item.pixel_space = true;
  return item;
}

bool reject_raster(uint32_t, std::vector<uint8_t>*, int*, int*) { return false; }

bool reject_icon(const std::string&, std::vector<uint8_t>*, int*, int*) {
  return false;
}

bool near(float a, float b) { return std::fabs(a - b) < 1.0e-4f; }

constexpr uint32_t kShadeRasterKey = 42;
// Two RGBA8 texels. Second alpha is below the vista hard cut (160).
const uint8_t kShadePixels[8] = {100, 150, 200, 255, 10, 20, 30, 100};

bool load_shade_raster(uint32_t key, std::vector<uint8_t>* rgba, int* w, int* h) {
  if (key != kShadeRasterKey || !rgba || !w || !h) {
    return false;
  }
  *w = 2;
  *h = 1;
  rgba->assign(kShadePixels, kShadePixels + 8);
  return true;
}

vista::DrawItem shade_quad(float opacity, vista::DrawBlend blend) {
  vista::DrawItem item = raster_quad();
  item.codepoint = kShadeRasterKey;
  item.rgba = 0xffffffffu;
  item.opacity = opacity;
  item.blend = blend;
  return item;
}

bool read_tint_a(const render::rhi::StubCommandList* stub, float* out) {
  if (!stub || !out) {
    return false;
  }
  const render::rhi::StubCommandList::ConstantRecord* color =
      stub->constant_at(render::programs::kColorSlot);
  if (!color || !color->has_bytes ||
      color->byte_size < sizeof(render::programs::Color)) {
    return false;
  }
  render::programs::Color tint;
  std::memcpy(&tint, color->bytes, sizeof(tint));
  *out = tint.a;
  return true;
}

void expect_raster_over_src_alpha(vista::MapPass* pass,
                                  render::rhi::Device* device) {
  if (!pass || !device) {
    expect(false, "raster over needs pass");
    return;
  }
  pass->invalidate_uploaded();
  render::rhi::CommandList* list = device->create_command_list();
  vista::MapIR frame;
  frame.background_rgba = 0xFF1B3A4Cu;
  frame.background_opacity = 1.f;
  frame.items.push_back(shade_quad(0.5f, vista::DrawBlend::kOver));
  expect(pass->record(device, list, frame, view_64(), nullptr, load_shade_raster,
                      reject_icon),
         "raster over records");
  auto* stub = static_cast<render::rhi::StubCommandList*>(list);
  expect(stub->draw_indexed_calls == 1, "raster over one quad");
  expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
         "raster over stays src alpha");
  expect(stub->last_pipeline == pass->textured_pipeline(),
         "raster over uses textured pipeline");
  float tint_a = 0.f;
  expect(read_tint_a(stub, &tint_a) && near(tint_a, 0.5f),
         "raster over keeps opacity");
  auto* tex = static_cast<render::rhi::StubTexture*>(stub->last_texture);
  expect(tex != nullptr && tex->bytes.size() >= 8, "raster over texture");
  if (tex != nullptr && tex->bytes.size() >= 8) {
    expect(std::memcmp(tex->bytes.data(), kShadePixels, 8) == 0,
           "raster over texels are not premultiplied");
  }
}

void expect_hillshade_multiply_blend(vista::MapPass* pass,
                                     render::rhi::Device* device) {
  if (!pass || !device) {
    expect(false, "hillshade multiply needs pass");
    return;
  }
  pass->invalidate_uploaded();
  render::rhi::CommandList* list = device->create_command_list();
  vista::MapIR frame;
  frame.background_rgba = 0xFF1B3A4Cu;
  frame.background_opacity = 1.f;
  constexpr float kOpacity = 0.5f;
  frame.items.push_back(shade_quad(kOpacity, vista::DrawBlend::kMultiply));
  expect(pass->record(device, list, frame, view_64(), nullptr, load_shade_raster,
                      reject_icon),
         "hillshade multiply records");
  auto* stub = static_cast<render::rhi::StubCommandList*>(list);
  expect(stub->draw_indexed_calls == 1, "hillshade multiply one quad");
  expect(stub->last_blend == render::rhi::BlendMode::kMultiply,
         "hillshade multiply records kMultiply");
  expect(stub->last_pipeline == pass->multiply_pipeline(),
         "hillshade multiply pipeline");
  float tint_a = 0.f;
  expect(read_tint_a(stub, &tint_a) && near(tint_a, 1.f),
         "hillshade multiply forces opacity 1");
  auto* tex = static_cast<render::rhi::StubTexture*>(stub->last_texture);
  expect(tex != nullptr && tex->bytes.size() >= 8, "hillshade texture");
  if (tex != nullptr && tex->bytes.size() >= 8) {
    // Upload must call vista::apply_multiply_coverage on these texels.
    std::vector<uint8_t> expected(kShadePixels, kShadePixels + 8);
    vista::apply_multiply_coverage(expected, kOpacity);
    expect(std::memcmp(tex->bytes.data(), expected.data(), 8) == 0,
           "hillshade texels match apply_multiply_coverage");
  }
}

// 90 degrees in pixel space (y down): +x rotates toward +y.
// Anchor (15, 15). Corner (10, 10) lands on pixel (20, 10), then ortho
// (20, 90) on a 100px view whose world span is 0..100 with y flipped.
// Cold upload merge: many solid meshes → one VB+IB (two create_buffer calls).
void expect_solid_mega_buffer_upload(render::rhi::Device* device) {
  if (!device) {
    expect(false, "mega upload needs device");
    return;
  }
  std::vector<vista::detail::PlacedMesh> meshes;
  for (int i = 0; i < 8; ++i) {
    vista::detail::PlacedMesh mesh;
    mesh.source = vista::detail::MeshSource::kSolid;
    const float x = static_cast<float>(i);
    mesh.vertices = {
        {x, 0.f, 0.f, 0.f, 0.f},
        {x + 1.f, 0.f, 0.f, 0.f, 0.f},
        {x, 1.f, 0.f, 0.f, 0.f},
    };
    mesh.indices = {0, 1, 2};
    // Distinct tints so draws do not coalesce; still one mega buffer.
    mesh.r = 0.1f * static_cast<float>(i);
    mesh.g = 0.2f;
    mesh.b = 0.3f;
    mesh.a = 1.f;
    meshes.push_back(std::move(mesh));
  }
  vista::detail::AtlasLayout atlas;
  std::vector<render::rhi::Buffer*> buffers;
  std::vector<render::rhi::Texture*> textures;
  const std::vector<vista::detail::UploadedDraw> draws =
      vista::detail::upload_draws(device, meshes, atlas, reject_raster,
                                        reject_icon, &buffers, &textures);
  expect(draws.size() == 8, "mega solid keeps one draw per tint");
  expect(buffers.size() == 2, "mega solid creates one VB and one IB");
  expect(textures.empty(), "mega solid has no textures");
  if (draws.size() == 8 && !draws.empty()) {
    render::rhi::Buffer* vb = draws[0].vertices;
    render::rhi::Buffer* ib = draws[0].indices;
    uint32_t first_sum = 0;
    for (const vista::detail::UploadedDraw& draw : draws) {
      expect(draw.vertices == vb && draw.indices == ib,
             "all solids share mega VB/IB");
      expect(draw.first_index == first_sum, "solid first_index packed");
      expect(draw.index_count == 3, "solid triangle index_count");
      first_sum += draw.index_count;
    }
  }
  for (render::rhi::Buffer* owned : buffers) {
    device->destroy_buffer(owned);
  }
}

// Same tint + adjacent IB ranges collapse to one draw; still one mega VB/IB.
void expect_solid_tint_coalesce(render::rhi::Device* device) {
  if (!device) {
    expect(false, "coalesce upload needs device");
    return;
  }
  std::vector<vista::detail::PlacedMesh> meshes;
  for (int i = 0; i < 8; ++i) {
    vista::detail::PlacedMesh mesh;
    mesh.source = vista::detail::MeshSource::kSolid;
    const float x = static_cast<float>(i);
    mesh.vertices = {
        {x, 0.f, 0.f, 0.f, 0.f},
        {x + 1.f, 0.f, 0.f, 0.f, 0.f},
        {x, 1.f, 0.f, 0.f, 0.f},
    };
    mesh.indices = {0, 1, 2};
    mesh.r = 0.4f;
    mesh.g = 0.5f;
    mesh.b = 0.6f;
    mesh.a = 1.f;
    meshes.push_back(std::move(mesh));
  }
  vista::detail::AtlasLayout atlas;
  std::vector<render::rhi::Buffer*> buffers;
  std::vector<render::rhi::Texture*> textures;
  const std::vector<vista::detail::UploadedDraw> draws =
      vista::detail::upload_draws(device, meshes, atlas, reject_raster,
                                        reject_icon, &buffers, &textures);
  expect(draws.size() == 1, "same-tint solids coalesce to one draw");
  expect(buffers.size() == 2, "coalesced solids still one VB+IB");
  if (draws.size() == 1) {
    expect(draws[0].index_count == 24, "coalesced index_count is 8 triangles");
    expect(draws[0].first_index == 0, "coalesced first_index");
  }
  for (render::rhi::Buffer* owned : buffers) {
    device->destroy_buffer(owned);
  }
}

// Cold china path: place borrows stay live; upload packs without seal.
void expect_borrow_upload_without_seal(render::rhi::Device* device) {
  if (!device) {
    expect(false, "borrow upload needs device");
    return;
  }
  const vista::View view = view_64();
  vista::MapIR frame;
  frame.items.push_back(fill_triangle());
  std::vector<vista::detail::PlacedMesh> meshes =
      vista::detail::place_frame(frame, view);
  expect(meshes.size() == 2 && meshes[1].vertices_src != nullptr,
         "core still borrows before upload");
  vista::detail::AtlasLayout atlas;
  std::vector<render::rhi::Buffer*> buffers;
  std::vector<render::rhi::Texture*> textures;
  const std::vector<vista::detail::UploadedDraw> draws =
      vista::detail::upload_draws(device, std::move(meshes), atlas,
                                  reject_raster, reject_icon, &buffers,
                                  &textures);
  expect(!draws.empty(), "borrow pack uploads while MapIR alive");
  expect(buffers.size() == 2, "borrow pack one solid mega VB+IB");
  // MapIR must still own the source verts after upload (no seal).
  expect(frame.items[0].vertices.size() == 3, "MapIR verts intact after pack");
  for (render::rhi::Buffer* owned : buffers) {
    device->destroy_buffer(owned);
  }
}

// kReplace with cache_key items must one-shot place+upload (not per-slice
// DrawItem assign). Two keyed fills share one solid mega buffer.
void expect_replace_keyed_oneshot(render::rhi::Device* device) {
  if (!device) {
    expect(false, "replace keyed needs device");
    return;
  }
  vista::MapPass pass;
  pass.set_upload_policy(vista::MapPass::UploadPolicy::kReplace);
  const vista::View view = view_64();
  render::rhi::CommandList* list = device->create_command_list();
  vista::MapIR frame;
  frame.background_rgba = 0xFF1B3A4Cu;
  frame.background_opacity = 1.f;
  vista::DrawItem a = fill_triangle();
  a.cache_key = 11;
  vista::DrawItem b = fill_triangle();
  for (vista::Vertex& v : b.vertices) {
    v.x += 2.f;
  }
  b.cache_key = 22;
  frame.items.push_back(std::move(a));
  frame.items.push_back(std::move(b));
  expect(pass.record(device, list, frame, view, nullptr, reject_raster,
                     reject_icon),
         "kReplace keyed record");
  auto* stub = static_cast<render::rhi::StubCommandList*>(list);
  // Two fills × (feather+core) = 4 draws when tints match? Same rgba → may
  // coalesce within mega. At least feather+core work ran.
  expect(stub->draw_indexed_calls >= 2, "keyed replace draws meshes");
}

// Linear pack of many solids (P0d: per-mesh reserve(size+n) was quadratic).
void expect_many_solid_pack_linear(render::rhi::Device* device) {
  if (!device) {
    expect(false, "linear pack needs device");
    return;
  }
  constexpr int kCount = 4096;
  std::vector<vista::detail::PlacedMesh> meshes;
  meshes.reserve(static_cast<size_t>(kCount));
  for (int i = 0; i < kCount; ++i) {
    vista::detail::PlacedMesh mesh;
    mesh.source = vista::detail::MeshSource::kSolid;
    const float x = static_cast<float>(i);
    mesh.vertices = {
        {x, 0.f, 0.f, 0.f, 0.f},
        {x + 1.f, 0.f, 0.f, 0.f, 0.f},
        {x, 1.f, 0.f, 0.f, 0.f},
    };
    mesh.indices = {0, 1, 2};
    mesh.r = 0.2f;
    mesh.g = 0.3f;
    mesh.b = 0.4f;
    mesh.a = 1.f;
    meshes.push_back(std::move(mesh));
  }
  vista::detail::AtlasLayout atlas;
  std::vector<render::rhi::Buffer*> buffers;
  std::vector<render::rhi::Texture*> textures;
  const auto t0 = std::chrono::steady_clock::now();
  const std::vector<vista::detail::UploadedDraw> draws =
      vista::detail::upload_draws(device, meshes, atlas, reject_raster,
                                        reject_icon, &buffers, &textures);
  const int64_t ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - t0)
                         .count();
  std::fprintf(stderr, "upload_draws %d solids (null rhi): %lld ms\n", kCount,
               static_cast<long long>(ms));
  expect(draws.size() == 1, "4096 same-tint solids one draw");
  expect(buffers.size() == 2, "4096 solids one VB+IB");
  expect(ms < 1500, "4096-solid pack stays linear (Debug)");
  for (render::rhi::Buffer* owned : buffers) {
    device->destroy_buffer(owned);
  }
}

void expect_pixel_quad_rotates_into_ortho() {
  constexpr float kRightAngle = 1.5707963267948966f;
  vista::View view;
  view.width_px = 100;
  view.height_px = 100;
  view.min_x = 0;
  view.min_y = 0;
  view.max_x = 100;
  view.max_y = 100;

  vista::DrawItem item;
  item.kind = vista::DrawKind::kIcon;
  item.pixel_space = true;
  item.angle_rad = kRightAngle;
  item.anchor_x = 15.f;
  item.anchor_y = 15.f;
  item.vertices = {
      {10.f, 10.f, 0.f, 0.f, 0.f},
      {20.f, 10.f, 0.f, 1.f, 0.f},
      {20.f, 20.f, 0.f, 1.f, 1.f},
      {10.f, 20.f, 0.f, 0.f, 1.f},
  };
  item.indices = {0, 1, 2, 0, 2, 3};

  vista::MapIR frame;
  frame.items.push_back(item);
  const std::vector<vista::detail::PlacedMesh> meshes =
      vista::detail::place_frame(frame, view);
  expect(meshes.size() == 1 && meshes[0].vertex_list().size() == 4,
         "rotated quad mesh");
  if (meshes.size() != 1 || meshes[0].vertex_list().size() != 4) {
    return;
  }
  const std::vector<vista::Vertex>& v = meshes[0].vertex_list();
  expect(near(v[0].x, 20.f) && near(v[0].y, 90.f), "corner0 about anchor");
  expect(near(v[1].x, 20.f) && near(v[1].y, 80.f), "corner1 about anchor");
  expect(near(v[2].x, 10.f) && near(v[2].y, 80.f), "corner2 about anchor");
  expect(near(v[3].x, 10.f) && near(v[3].y, 90.f), "corner3 about anchor");
  expect(near(v[1].u, 1.f) && near(v[1].v, 0.f), "uv survives rotation");
}

}  // namespace

int main() {
  using render::rhi::Backend;
  using render::rhi::DeviceDesc;
  using render::rhi::StubCommandList;
  using render::rhi::create_device;

  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);

  expect_pixel_quad_rotates_into_ortho();

  std::unique_ptr<render::rhi::Device> device(create_device(Backend::kNull));
  expect(device != nullptr, "create null device");
  expect(device->initialize(DeviceDesc()), "null device init");

  expect_solid_mega_buffer_upload(device.get());
  expect_solid_tint_coalesce(device.get());
  expect_borrow_upload_without_seal(device.get());
  expect_replace_keyed_oneshot(device.get());
  expect_many_solid_pack_linear(device.get());

  vista::MapPass pass;
  const vista::View view = view_64();
  expect(!pass.record(nullptr, nullptr, {}, view, nullptr, reject_raster,
                      reject_icon),
         "null device and list");

  {
    render::rhi::CommandList* list = device->create_command_list();
    expect(list != nullptr, "empty list");
    vista::MapIR frame;
    frame.background_rgba = 0xFF1B3A4Cu;
    frame.background_opacity = 1.f;
    expect(pass.record(device.get(), list, frame, view, nullptr, reject_raster,
                       reject_icon),
           "empty frame");
    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->clear_load_calls == 1, "empty frame clears");
    expect(stub->draw_indexed_calls == 0, "empty frame has no draw");
    expect(stub->closed, "empty frame closes");
    expect(std::fabs(stub->last_pass.clear_r - (0x1B / 255.f)) < 1.0e-5f,
           "clear r");
    expect(std::fabs(stub->last_pass.clear_g - (0x3A / 255.f)) < 1.0e-5f,
           "clear g");
    expect(std::fabs(stub->last_pass.clear_b - (0x4C / 255.f)) < 1.0e-5f,
           "clear b");
    expect(device->execute(list), "empty frame execute");
  }

  {
    render::rhi::CommandList* list = device->create_command_list();
    vista::MapIR frame;
    frame.background_rgba = 0xFF1B3A4Cu;
    frame.background_opacity = 1.f;
    frame.items.push_back(fill_triangle());
    expect(pass.record(device.get(), list, frame, view, nullptr, reject_raster,
                       reject_icon),
           "fill triangle");
    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->clear_load_calls == 1, "fill clears");
    expect(stub->draw_indexed_calls >= 2, "fill feather then core");
    expect(stub->index_counts.size() >= 2 && stub->index_counts[0] == 3 &&
               stub->index_counts[1] == 3,
           "fill feather and core index counts");
    expect(stub->last_pipeline == pass.solid_pipeline(),
           "fill solid pipeline");
  }

  {
    // MapIR content changed — camera-only reuse must not keep prior uploads.
    pass.invalidate_uploaded();
    render::rhi::CommandList* list = device->create_command_list();
    vista::MapIR frame;
    frame.background_rgba = 0xFF000000u;
    frame.background_opacity = 1.f;
    vista::DrawItem translucent = fill_triangle();
    translucent.rgba = 0x80FF0000u;
    translucent.opacity = 0.5f;
    frame.items.push_back(translucent);
    expect(pass.record(device.get(), list, frame, view, nullptr, reject_raster,
                       reject_icon),
           "translucent fill");
    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->draw_indexed_calls >= 2, "translucent fill draws");
    expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
           "translucent fill uses src alpha blend");
    expect(stub->set_constants_calls >= 2,
           "feather and core solid color constants");
  }

  {
    const vista::View view = view_64();
    vista::MapIR frame;
    frame.items.push_back(fill_triangle());
    const std::vector<vista::detail::PlacedMesh> meshes =
        vista::detail::place_frame(frame, view);
    expect(meshes.size() == 2, "fill placement emits feather and core");
    expect(meshes.size() == 2 && meshes[1].vertices_src != nullptr,
           "world fill core borrows DrawItem vertices");
    expect(meshes.size() == 2 && meshes[1].vertex_list().size() == 3,
           "world fill core has three verts");
  }

  {
    const vista::View view = view_64();
    vista::MapIR frame;
    frame.items.push_back(fill_triangle());
    std::vector<vista::detail::PlacedMesh> meshes =
        vista::detail::place_frame(frame, view);
    expect(meshes.size() == 2 && meshes[1].vertices_src != nullptr,
           "borrow before seal");
    vista::detail::seal_borrowed_meshes(&meshes);
    expect(meshes.size() == 2 && meshes[1].vertices_src == nullptr &&
               meshes[1].indices_src == nullptr,
           "seal owns CPU buffers");
    expect(meshes.size() == 2 && meshes[1].vertices.size() == 3,
           "sealed core keeps three verts");
    frame.items.clear();
    frame.items.shrink_to_fit();
    vista::detail::AtlasLayout atlas;
    std::vector<render::rhi::Buffer*> buffers;
    std::vector<render::rhi::Texture*> textures;
    const std::vector<vista::detail::UploadedDraw> draws =
        vista::detail::upload_draws(device.get(), meshes, atlas,
                                          reject_raster, reject_icon, &buffers,
                                          &textures);
    expect(!draws.empty(), "FlyCube-safe upload after MapIR died");
    for (render::rhi::Buffer* owned : buffers) {
      device->destroy_buffer(owned);
    }
  }

  {
    StubGlyphRasterizer glyphs;
    pass.invalidate_uploaded();
    render::rhi::CommandList* list = device->create_command_list();
    vista::MapIR frame;
    frame.background_rgba = 0xFF1B3A4Cu;
    frame.background_opacity = 1.f;
    frame.items.push_back(raster_quad());
    frame.items.push_back(icon_quad());
    frame.items.push_back(fill_triangle());
    frame.items.push_back(text_quad());
    expect(pass.record(device.get(), list, frame, view, &glyphs, reject_raster,
                       reject_icon),
           "fill plus text");
    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->clear_load_calls >= 1, "clear plus draws");
    expect(stub->draw_indexed_calls >= 1, "at least one draw");
    // Failed raster/icon are skipped. Fill feather+core (3+3) + halo (6) + glyph (6).
    expect(stub->draw_indexed_calls == 4, "fill feather, core, halo, glyph");
    expect(stub->index_counts.size() == 4 && stub->index_counts[0] == 3 &&
               stub->index_counts[1] == 3 && stub->index_counts[2] == 6 &&
               stub->index_counts[3] == 6,
           "index counts");
    expect(stub->bind_texture_calls >= 1, "glyph atlas bound");
    expect(stub->last_pipeline == pass.textured_pipeline(),
           "textured glyph");
    expect(stub->last_blend == render::rhi::BlendMode::kSrcAlpha,
           "glyph src alpha");
    expect(stub->last_depth == render::rhi::DepthMode::kDisabled,
           "2d depth off");
    expect(stub->bind_camera_calls >= 1, "ortho camera");
    expect(stub->last_camera.kind == render::rhi::CameraKind::kOrtho,
           "ortho kind");
    expect(std::fabs(stub->last_camera.proj[0] - 0.2f) < 1.0e-4f, "ortho sx");
    expect(std::fabs(stub->last_camera.proj[5] - 0.2f) < 1.0e-4f, "ortho sy");
    expect(stub->closed, "list closed");
    expect(device->execute(list), "execute null");
  }

  {
    FailingGlyphRasterizer glyphs;
    pass.invalidate_uploaded();
    render::rhi::CommandList* list = device->create_command_list();
    vista::MapIR frame;
    frame.background_rgba = 0xFFFFFFFFu;
    frame.background_opacity = 1.f;
    frame.items.push_back(fill_triangle());
    frame.items.push_back(text_quad());
    expect(pass.record(device.get(), list, frame, view, &glyphs, reject_raster,
                       reject_icon),
           "failed glyph still records");
    auto* stub = static_cast<StubCommandList*>(list);
    expect(stub->clear_load_calls == 1, "failed glyph still clears");
    expect(stub->draw_indexed_calls == 2, "failed glyph skips text; fill feather+core");
  }

  expect_raster_over_src_alpha(&pass, device.get());
  expect_hillshade_multiply_blend(&pass, device.get());

  device->shutdown();

  if (g_fails) {
    std::fprintf(stderr, "map2d_pass_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "map2d_pass_test: ok\n");
  std::fflush(stdout);
  // FlyCube-linked CRT can hang in static destructors after the null device
  // intentionally leaks stubs. Exit without running atexit/dtors.
  std::_Exit(0);
}
