// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/render/rhi2d/impl/common/surface/composer/composer.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

#include "scenic/render/rhi2d/impl/common/paint/carto/frame/preview_xform.h"
#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void fill_solid(scenic::detail::Rhi2dOwnedSurface& buf, uint8_t b, uint8_t g, uint8_t r,
                uint8_t a) {
  auto* bits = static_cast<uint8_t*>(buf.bits());
  expect(bits != nullptr, "fill_solid bits");
  if (!bits) {
    return;
  }
  const int w = buf.width();
  const int h = buf.height();
  const uint32_t stride = buf.stride_bytes();
  for (int y = 0; y < h; ++y) {
    uint8_t* row = bits + static_cast<size_t>(y) * stride;
    for (int x = 0; x < w; ++x) {
      uint8_t* p = row + static_cast<size_t>(x) * 4u;
      p[0] = b;
      p[1] = g;
      p[2] = r;
      p[3] = a;
    }
  }
}

void test_pool_acquire_release_reuse() {
  scenic::detail::Rhi2dSurfacePool& pool = scenic::detail::rhi2d_surface_pool();
  scenic::detail::Rhi2dSurface a = pool.acquire(nullptr, 8, 8);
  expect(a.bitmap != nullptr && a.bits != nullptr, "pool acquire a");
  const HBITMAP first = a.bitmap;
  const uint64_t gen_after_create = a.generation;
  a.bump_generation();
  a.mark_dirty(1, 1, 2, 2);
  expect(a.generation == gen_after_create + 1, "manual bump generation");
  expect(a.has_dirty && a.dirty_x == 1 && a.dirty_y == 1 && a.dirty_w == 2 &&
             a.dirty_h == 2,
         "manual mark_dirty");
  pool.release(a);

  scenic::detail::Rhi2dSurface b = pool.acquire(nullptr, 8, 8);
  expect(b.bitmap == first, "pool reuse same bitmap");
  expect(b.bits != nullptr, "pool reuse bits");
  expect(b.generation == gen_after_create + 1, "pool keeps generation");
  expect(b.has_dirty, "pool keeps dirty flag");
  pool.release(b);

  // Different size should not reuse the 8x8 entry.
  scenic::detail::Rhi2dSurface c = pool.acquire(nullptr, 4, 4);
  expect(c.bitmap != nullptr && c.bitmap != first, "pool different size");
  pool.release(c);
}

void test_dirty_after_clear() {
  scenic::detail::Rhi2dOwnedSurface surf;
  expect(surf.set_size(4, 4) == 0, "dirty set_size");
  expect(surf.surface().generation >= 1, "clear via set_size bumps generation");
  expect(surf.surface().has_dirty, "clear via set_size marks dirty");
  expect(surf.surface().dirty_x == 0 && surf.surface().dirty_y == 0 &&
             surf.surface().dirty_w == 4 && surf.surface().dirty_h == 4,
         "full-buffer dirty rect after set_size clear");

  const uint64_t gen = surf.surface().generation;
  surf.surface().clear_dirty();
  expect(!surf.surface().has_dirty, "clear_dirty");
  expect(surf.clear(1, 1, 2, 2) == 0, "partial clear");
  expect(surf.surface().generation == gen + 1,
         "partial clear bumps generation");
  expect(surf.surface().has_dirty && surf.surface().dirty_x == 1 &&
             surf.surface().dirty_y == 1 && surf.surface().dirty_w == 2 &&
             surf.surface().dirty_h == 2,
         "partial clear dirty rect");
}

void test_preview_zoom_stretch_anchor() {
  base::Viewport dest;
  dest.m_fVOX = 0.f;
  dest.m_fVOY = 0.f;
  dest.m_fVWidth = 100.f;
  dest.m_fVHeight = 100.f;
  expect(scenic::detail::apply_preview_zoom_stretch(&dest, 50.f, 50.f, 0.5f),
         "zoom-in stretch ok");
  expect(std::fabs(dest.m_fVWidth - 200.f) < 1e-3f &&
             std::fabs(dest.m_fVHeight - 200.f) < 1e-3f,
         "zoom-in doubles dest size");
  expect(std::fabs(dest.m_fVOX - (-50.f)) < 1e-3f &&
             std::fabs(dest.m_fVOY - (-50.f)) < 1e-3f,
         "zoom-in keeps anchor at (50,50)");
  // Source pixel 50 maps to dest -50 + 50 * (200/100) = 50.
  const float mapped =
      dest.m_fVOX + 50.f * (dest.m_fVWidth / 100.f);
  expect(std::fabs(mapped - 50.f) < 1e-3f, "anchor pixel stays fixed");

  expect(!scenic::detail::apply_preview_zoom_stretch(&dest, 0.f, 0.f, 0.f),
         "reject non-positive fscale");
  expect(!scenic::detail::apply_preview_zoom_stretch(nullptr, 0.f, 0.f, 1.f),
         "reject null dest");

  base::Viewport src;
  src.m_fVOX = 0.f;
  src.m_fVOY = 0.f;
  src.m_fVWidth = 100.f;
  src.m_fVHeight = 100.f;
  base::Viewport from_fblc;
  expect(scenic::detail::set_preview_stretch_from_fblc(&from_fblc, src, 1.f,
                                                       2.f, 50.f, 50.f),
         "from_fblc zoom-in");
  expect(std::fabs(from_fblc.m_fVWidth - 200.f) < 1e-3f, "from_fblc size");
  // Contract for RenderMapToDC: blit_to(dest=from_fblc, src=src) enlarges;
  // dest/src swap would shrink and invert preview vs settled windowport.
  expect(from_fblc.m_fVWidth > src.m_fVWidth, "zoom-in dest larger than src");

  base::Viewport zoom_out;
  expect(scenic::detail::set_preview_stretch_from_fblc(&zoom_out, src, 2.f, 1.f,
                                                       50.f, 50.f),
         "from_fblc zoom-out");
  expect(std::fabs(zoom_out.m_fVWidth - 50.f) < 1e-3f,
         "zoom-out halves dest size");
  expect(zoom_out.m_fVWidth < src.m_fVWidth, "zoom-out dest smaller than src");

  base::Viewport capped = src;
  capped.m_fVWidth = 10000.f;
  capped.m_fVHeight = 10000.f;
  scenic::detail::clamp_preview_dest(&capped, src);
  expect(std::fabs(capped.m_fVWidth - 100.f) < 1e-3f &&
             std::fabs(capped.m_fVHeight - 100.f) < 1e-3f,
         "clamp resets pathological dest");
}

}  // namespace

int main() {
  test_pool_acquire_release_reuse();
  test_dirty_after_clear();
  test_preview_zoom_stretch_anchor();

  scenic::detail::Rhi2dOwnedSurface src;
  scenic::detail::Rhi2dOwnedSurface dst;
  expect(src.set_size(2, 2) == 0, "src set_size");
  expect(dst.set_size(4, 4) == 0, "dst set_size");
  fill_solid(src, 10, 20, 30, 255);
  fill_solid(dst, 0, 0, 0, 255);

  const uint64_t dst_gen_before = dst.surface().generation;
  dst.surface().clear_dirty();

  expect(scenic::detail::blit_surfaces(src.surface(), dst.surface(), 0, 0, 4, 4,
                                       0, 0, 2, 2, /*color_key=*/false,
                                       RGB(0, 0, 0)),
         "compose stretch 2x2 -> 4x4");

  expect(dst.surface().generation == dst_gen_before + 1,
         "compose bumps dst generation");
  expect(dst.surface().has_dirty && dst.surface().dirty_w == 4 &&
             dst.surface().dirty_h == 4,
         "compose marks dst dirty");

  auto* bits = static_cast<uint8_t*>(dst.bits());
  expect(bits != nullptr, "dst bits after compose");
  if (bits) {
    expect(bits[0] == 10 && bits[1] == 20 && bits[2] == 30,
           "dst(0,0) sampled from src");
    const uint32_t stride = dst.stride_bytes();
    uint8_t* corner = bits + 3 * stride + 3 * 4;
    expect(corner[0] == 10 && corner[1] == 20 && corner[2] == 30,
           "dst(3,3) sampled from src");
  }

  gpu::detail::CompositorFrame frame;
  expect(scenic::detail::make_compositor_frame(dst.surface(), &frame),
         "make_frame");
  expect(frame.width_px == 4 && frame.height_px == 4, "frame size");
  expect(!frame.render_pass_list.empty(), "frame has pass");
  expect(!frame.render_pass_list.back().image_data.empty(), "owned image_data");

  std::vector<uint8_t> blended;
  expect(scenic::detail::blend_frame_to_bgra(frame, &blended), "frame_to_bgra");
  expect(blended.size() == 4u * 4u * 4u, "blended size");

  bool sink_hit = false;
  scenic::detail::set_compositor_submit(
      [&](const gpu::detail::CompositorFrame& f) {
        sink_hit = f.width_px == 4 && f.height_px == 4;
        return sink_hit;
      });
  expect(scenic::detail::has_compositor_submit(), "has submit");
  expect(scenic::detail::submit_surface(dst.surface()), "submit");
  expect(sink_hit, "submit sink called");
  scenic::detail::clear_compositor_submit();
  expect(!scenic::detail::has_compositor_submit(), "cleared submit");

  if (g_fails) {
    std::fprintf(stderr, "gdi_compose_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("gdi_compose_test: ok\n");
  return 0;
}
