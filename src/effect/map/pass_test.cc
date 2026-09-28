// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "effect/map/pass.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

#include "effect/map/detail/place.h"
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
class StubGlyphRasterizer : public effect::map::GlyphRasterizer {
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

class FailingGlyphRasterizer : public effect::map::GlyphRasterizer {
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

gis::vista::View view_64() {
  gis::vista::View view;
  view.width_px = 64;
  view.height_px = 64;
  view.min_x = 0;
  view.min_y = 0;
  view.max_x = 10;
  view.max_y = 10;
  return view;
}

gis::vista::DrawItem fill_triangle() {
  gis::vista::DrawItem item;
  item.kind = gis::vista::DrawKind::kFill;
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

gis::vista::DrawItem text_quad() {
  gis::vista::DrawItem item;
  item.kind = gis::vista::DrawKind::kText;
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

gis::vista::DrawItem raster_quad() {
  gis::vista::DrawItem item;
  item.kind = gis::vista::DrawKind::kRaster;
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

gis::vista::DrawItem icon_quad() {
  gis::vista::DrawItem item;
  item.kind = gis::vista::DrawKind::kIcon;
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

// 90 degrees in pixel space (y down): +x rotates toward +y.
// Anchor (15, 15). Corner (10, 10) lands on pixel (20, 10), then ortho
// (20, 90) on a 100px view whose world span is 0..100 with y flipped.
void expect_pixel_quad_rotates_into_ortho() {
  constexpr float kRightAngle = 1.5707963267948966f;
  gis::vista::View view;
  view.width_px = 100;
  view.height_px = 100;
  view.min_x = 0;
  view.min_y = 0;
  view.max_x = 100;
  view.max_y = 100;

  gis::vista::DrawItem item;
  item.kind = gis::vista::DrawKind::kIcon;
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

  gis::vista::MapFrame frame;
  frame.items.push_back(item);
  const std::vector<effect::map::detail::PlacedMesh> meshes =
      effect::map::detail::place_frame(frame, view);
  expect(meshes.size() == 1 && meshes[0].vertices.size() == 4,
         "rotated quad mesh");
  if (meshes.size() != 1 || meshes[0].vertices.size() != 4) {
    return;
  }
  const std::vector<gis::vista::Vertex>& v = meshes[0].vertices;
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

  effect::map::Pass pass;
  const gis::vista::View view = view_64();
  expect(!pass.record(nullptr, nullptr, {}, view, nullptr, reject_raster,
                      reject_icon),
         "null device and list");

  {
    render::rhi::CommandList* list = device->create_command_list();
    expect(list != nullptr, "empty list");
    gis::vista::MapFrame frame;
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
    gis::vista::MapFrame frame;
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
    // MapFrame content changed — camera-only reuse must not keep prior uploads.
    pass.invalidate_uploaded();
    render::rhi::CommandList* list = device->create_command_list();
    gis::vista::MapFrame frame;
    frame.background_rgba = 0xFF000000u;
    frame.background_opacity = 1.f;
    gis::vista::DrawItem translucent = fill_triangle();
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
    const gis::vista::View view = view_64();
    gis::vista::MapFrame frame;
    frame.items.push_back(fill_triangle());
    const std::vector<effect::map::detail::PlacedMesh> meshes =
        effect::map::detail::place_frame(frame, view);
    expect(meshes.size() == 2, "fill placement emits feather and core");
  }

  {
    StubGlyphRasterizer glyphs;
    pass.invalidate_uploaded();
    render::rhi::CommandList* list = device->create_command_list();
    gis::vista::MapFrame frame;
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
    gis::vista::MapFrame frame;
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
