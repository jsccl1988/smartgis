// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstring>

#include "legacy/render/rhi2d/impl/gdi/paint/encode/command_encoder.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

// Local top-down 32bpp DIB (BGRA); does not use GdiSurfacePool.
bool make_test_dib(int width, int height, render::GdiSurface* out) {
  if (!out || width < 1 || height < 1) {
    return false;
  }
  BITMAPINFO bmi;
  std::memset(&bmi, 0, sizeof(bmi));
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = width;
  bmi.bmiHeader.biHeight = -height;  // top-down
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  void* bits = nullptr;
  HBITMAP bmp =
      ::CreateDIBSection(nullptr, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
  if (!bmp || !bits) {
    if (bmp) {
      ::DeleteObject(bmp);
    }
    return false;
  }
  out->bitmap = bmp;
  out->bits = bits;
  out->width = width;
  out->height = height;
  out->stride_bytes = static_cast<uint32_t>(width) * 4u;
  return true;
}

void destroy_test_dib(render::GdiSurface* surface) {
  if (!surface) {
    return;
  }
  if (surface->bitmap) {
    ::DeleteObject(surface->bitmap);
  }
  *surface = render::GdiSurface{};
}

uint8_t* pixel_at(render::GdiSurface& s, int x, int y) {
  auto* bits = static_cast<uint8_t*>(s.bits);
  return bits + static_cast<size_t>(y) * s.stride_bytes +
         static_cast<size_t>(x) * 4u;
}

bool is_bgra(const uint8_t* p, uint8_t b, uint8_t g, uint8_t r) {
  return p[0] == b && p[1] == g && p[2] == r;
}

void fill_dib_solid(render::GdiSurface& s, COLORREF color) {
  const uint8_t b = GetBValue(color);
  const uint8_t g = GetGValue(color);
  const uint8_t r = GetRValue(color);
  for (int y = 0; y < s.height; ++y) {
    for (int x = 0; x < s.width; ++x) {
      uint8_t* p = pixel_at(s, x, y);
      p[0] = b;
      p[1] = g;
      p[2] = r;
      p[3] = 255;
    }
  }
}

}  // namespace

int main() {
  // --- clear + fill_rect ---
  render::GdiSurface surface;
  expect(make_test_dib(8, 8, &surface), "make_test_dib");

  render::detail::GdiCommandEncoder encoder;
  expect(encoder.begin_pass(&surface), "begin_pass");
  expect(encoder.clear(RGB(0, 0, 255)), "clear blue");
  expect(encoder.fill_rect(2, 2, 3, 3, RGB(255, 0, 0)), "fill_rect red");
  expect(encoder.end_pass(), "end_pass");

  render::GdiCommandBuffer buffer = encoder.take_buffer();
  expect(!buffer.empty(), "buffer not empty");
  expect(buffer.size() >= 3u, "clear + fill + end_pass");

  const uint64_t gen_before = surface.generation;
  expect(render::detail::replay(buffer, surface), "replay clear+fill");
  expect(surface.generation > gen_before, "generation bumped after write");
  expect(surface.has_dirty, "dirty marked after write");

  if (surface.bits) {
    const uint8_t* corner = pixel_at(surface, 0, 0);
    expect(is_bgra(corner, 255, 0, 0), "pixel(0,0) is blue after clear");

    const uint8_t* mid = pixel_at(surface, 2, 2);
    expect(is_bgra(mid, 0, 0, 255), "pixel(2,2) is red after fill_rect");

    const uint8_t* edge = pixel_at(surface, 7, 7);
    expect(is_bgra(edge, 255, 0, 0), "pixel(7,7) still blue");
  } else {
    expect(false, "surface bits after replay");
  }

  destroy_test_dib(&surface);

  // --- clear + stroke + clip stability ---
  expect(make_test_dib(16, 16, &surface), "make_test_dib stroke/clip");
  surface.clear_dirty();
  surface.generation = 0;

  expect(encoder.begin_pass(&surface), "begin_pass stroke");
  expect(encoder.clear(RGB(0, 0, 255)), "clear blue stroke");
  expect(encoder.stroke_rect(2, 2, 8, 8, RGB(0, 255, 0), 1), "stroke green");
  expect(encoder.clip_rect(4, 4, 4, 4), "clip_rect");
  expect(encoder.fill_rect(0, 0, 16, 16, RGB(255, 0, 0)), "fill under clip");
  expect(encoder.reset_clip(), "reset_clip");
  expect(encoder.fill_rect(14, 14, 2, 2, RGB(255, 255, 0)), "fill after reset");
  expect(encoder.end_pass(), "end_pass stroke");

  buffer = encoder.take_buffer();
  expect(render::detail::replay(buffer, surface), "replay stroke+clip");

  if (surface.bits) {
    // Border of stroked rect (2,2)-(10,10): sample top edge pixel (3,2).
    const uint8_t* stroke_px = pixel_at(surface, 3, 2);
    expect(is_bgra(stroke_px, 0, 255, 0), "stroke border pixel green");

    // Inside clip: fill red should land at (5,5).
    const uint8_t* clipped = pixel_at(surface, 5, 5);
    expect(is_bgra(clipped, 0, 0, 255), "inside clip is red");

    // Outside clip but inside former stroke area interior (3,3) stayed blue
    // (clip prevented the full-surface red fill from covering it).
    const uint8_t* unclipped = pixel_at(surface, 3, 3);
    expect(is_bgra(unclipped, 255, 0, 0),
           "outside clip stays blue (clip blocked fill)");

    // After reset_clip, corner fill reached (14,14).
    const uint8_t* after_reset = pixel_at(surface, 14, 14);
    expect(is_bgra(after_reset, 0, 255, 255),
           "after reset_clip yellow fill lands");
  } else {
    expect(false, "surface bits after stroke/clip replay");
  }

  destroy_test_dib(&surface);

  // --- blit of a small solid DIB ---
  expect(make_test_dib(8, 8, &surface), "make_test_dib blit target");
  render::GdiSurface src;
  expect(make_test_dib(4, 4, &src), "make_test_dib blit src");
  fill_dib_solid(src, RGB(0, 255, 0));  // green

  expect(encoder.begin_pass(&surface), "begin_pass blit");
  expect(encoder.clear(RGB(0, 0, 0)), "clear black");
  expect(encoder.blit(src.bitmap, 2, 2, 4, 4, 0, 0, 4, 4), "blit green");
  expect(encoder.end_pass(), "end_pass blit");

  buffer = encoder.take_buffer();
  expect(render::detail::replay(buffer, surface), "replay blit");

  if (surface.bits) {
    const uint8_t* blitted = pixel_at(surface, 3, 3);
    expect(is_bgra(blitted, 0, 255, 0), "blit pixel is green");
    const uint8_t* outside = pixel_at(surface, 0, 0);
    expect(is_bgra(outside, 0, 0, 0), "outside blit stays black");
  } else {
    expect(false, "surface bits after blit replay");
  }

  destroy_test_dib(&src);
  destroy_test_dib(&surface);

  // --- polyline + set_pen ---
  expect(make_test_dib(32, 32, &surface), "make_test_dib polyline");
  expect(encoder.begin_pass(&surface), "begin_pass polyline");
  expect(encoder.clear(RGB(0, 0, 0)), "clear black polyline");
  expect(encoder.set_pen(RGB(255, 0, 0), 1), "set_pen red");
  POINT line_pts[2] = {{4, 4}, {20, 4}};
  expect(encoder.polyline(line_pts, 2), "polyline");
  expect(encoder.end_pass(), "end_pass polyline");
  buffer = encoder.take_buffer();
  expect(render::detail::replay(buffer, surface), "replay polyline");
  if (surface.bits) {
    const uint8_t* on_line = pixel_at(surface, 12, 4);
    expect(is_bgra(on_line, 0, 0, 255), "polyline pixel is red");
  } else {
    expect(false, "surface bits after polyline replay");
  }
  destroy_test_dib(&surface);

  // --- poly_polyline + set_pen ---
  expect(make_test_dib(32, 32, &surface), "make_test_dib poly_polyline");
  expect(encoder.begin_pass(&surface), "begin_pass poly_polyline");
  expect(encoder.clear(RGB(0, 0, 0)), "clear black poly_polyline");
  expect(encoder.set_pen(RGB(0, 255, 0), 1), "set_pen green");
  POINT poly_pts[4] = {{4, 8}, {20, 8}, {4, 16}, {20, 16}};
  int poly_counts[2] = {2, 2};
  expect(encoder.poly_polyline(poly_pts, poly_counts, 2), "poly_polyline");
  expect(encoder.end_pass(), "end_pass poly_polyline");
  buffer = encoder.take_buffer();
  expect(render::detail::replay(buffer, surface), "replay poly_polyline");
  if (surface.bits) {
    const uint8_t* on_line = pixel_at(surface, 12, 8);
    expect(is_bgra(on_line, 0, 255, 0), "poly_polyline pixel is green");
  } else {
    expect(false, "surface bits after poly_polyline replay");
  }
  destroy_test_dib(&surface);

  if (g_fails) {
    std::fprintf(stderr, "gdi_encode_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("gdi_encode_test: ok\n");
  return 0;
}
