// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

#include "scenic/render/rhi2d/impl/common/cc/raster_tile.h"
#include "scenic/render/rhi2d/impl/common/cc/tile_graph_runner.h"
#include "scenic/render/rhi2d/impl/common/paint/carto/encode/command_encoder.h"
#include "scenic/render/rhi2d/impl/common/surface/dib/owned.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_enumerate_full_and_damage() {
  _putenv_s("SMT_RHI2D_TILE_SIZE", "256");
  _putenv_s("SMT_RHI2D_TILE_OUTSET", "16");

  auto all = scenic::detail::enumerate_viewport_tiles(
      500, 400, RECT{0, 0, 0, 0}, /*full_damage=*/true, /*gen=*/1);
  expect(all.size() == 4, "500x400 @256 => 2x2 tiles");

  RECT damage{10, 10, 40, 40};
  auto partial = scenic::detail::enumerate_viewport_tiles(
      500, 400, damage, /*full_damage=*/false, /*gen=*/2);
  expect(partial.size() == 1, "small damage hits one tile");
  expect(partial[0].tx == 0 && partial[0].ty == 0, "damage in tile 0,0");
}

void test_graph_runner_parallel_sum() {
  scenic::detail::Rhi2dTileGraphRunner runner;
  runner.ensure_workers(4);
  constexpr size_t kN = 32;
  std::vector<std::atomic<int>> hits(kN);
  for (size_t i = 0; i < kN; ++i) {
    hits[i].store(0);
  }
  runner.run_tiles(kN, [&](size_t i) { hits[i].fetch_add(1); });
  int sum = 0;
  for (size_t i = 0; i < kN; ++i) {
    sum += hits[i].load();
    expect(hits[i].load() == 1, "each tile ran once");
  }
  expect(sum == static_cast<int>(kN), "all tiles processed");
  runner.shutdown();
}

void test_execute_tile_stitch() {
  constexpr int kW = 64;
  constexpr int kH = 64;
  scenic::detail::Rhi2dOwnedSurface full;
  expect(full.set_size(kW, kH) == SMT_ERR_NONE, "full surface");

  scenic::detail::Rhi2dCommandEncoder enc;
  enc.begin_pass(&full.surface());
  enc.clear(RGB(0, 0, 0));
  enc.fill_rect(0, 0, 32, 32, RGB(255, 0, 0));
  enc.fill_rect(32, 0, 32, 32, RGB(0, 255, 0));
  enc.fill_rect(0, 32, 32, 32, RGB(0, 0, 255));
  enc.fill_rect(32, 32, 32, 32, RGB(255, 255, 0));
  enc.end_pass();
  scenic::detail::Rhi2dCommandBuffer buf = enc.take_buffer();

  std::vector<scenic::detail::Rhi2dRasterTile> tiles;
  for (int ty = 0; ty < 2; ++ty) {
    for (int tx = 0; tx < 2; ++tx) {
      scenic::detail::Rhi2dRasterTile t;
      t.tx = tx;
      t.ty = ty;
      t.center = RECT{tx * 32, ty * 32, (tx + 1) * 32, (ty + 1) * 32};
      t.paint = t.center;
      t.gen = 1;
      tiles.push_back(t);
    }
  }

  scenic::detail::Rhi2dOwnedSurface composed;
  expect(composed.set_size(kW, kH) == SMT_ERR_NONE, "composed surface");
  composed.clear(0, 0, kW, kH, RGB(0, 0, 0));

  scenic::detail::Rhi2dTileGraphRunner runner;
  runner.ensure_workers(2);
  std::vector<std::unique_ptr<scenic::detail::Rhi2dOwnedSurface>> surfs(tiles.size());
  runner.run_tiles(tiles.size(), [&](size_t i) {
    auto s = std::make_unique<scenic::detail::Rhi2dOwnedSurface>();
    const auto& tile = tiles[i];
    const int tw = tile.paint.right - tile.paint.left;
    const int th = tile.paint.bottom - tile.paint.top;
    expect(s->set_size(tw, th) == SMT_ERR_NONE, "tile set_size");
    expect(scenic::detail::execute_tile(buf, s->surface(), tile.paint.left,
                                        tile.paint.top),
           "execute_tile");
    surfs[i] = std::move(s);
  });

  auto* bits = static_cast<uint8_t*>(composed.bits());
  const uint32_t stride = composed.stride_bytes();
  for (size_t i = 0; i < tiles.size(); ++i) {
    expect(surfs[i] != nullptr, "tile surf present");
    if (!surfs[i]) {
      continue;
    }
    const auto& tile = tiles[i];
    auto* src = static_cast<uint8_t*>(surfs[i]->bits());
    const uint32_t src_stride = surfs[i]->stride_bytes();
    for (int y = 0; y < 32; ++y) {
      std::memcpy(bits + (tile.center.top + y) * stride + tile.center.left * 4,
                  src + y * src_stride, 32 * 4);
    }
  }

  auto sample = [&](int x, int y) -> COLORREF {
    const uint8_t* p = bits + y * stride + x * 4;
    return RGB(p[2], p[1], p[0]);  // BGRA
  };
  expect(sample(8, 8) == RGB(255, 0, 0), "tl red");
  expect(sample(40, 8) == RGB(0, 255, 0), "tr green");
  expect(sample(8, 40) == RGB(0, 0, 255), "bl blue");
  expect(sample(40, 40) == RGB(255, 255, 0), "br yellow");

  runner.shutdown();
}

void test_graph_runner_cancel() {
  scenic::detail::Rhi2dTileGraphRunner runner;
  runner.ensure_workers(4);
  std::atomic<int> started{0};
  std::atomic<int> finished{0};
  runner.request_cancel();
  runner.run_tiles(64, [&](size_t /*i*/) {
    started.fetch_add(1);
    if (runner.is_cancel_requested()) {
      return;
    }
    finished.fetch_add(1);
  });
  expect(finished.load() == 0, "cancel skips tile bodies");
  runner.clear_cancel();
  runner.shutdown();
}

void test_tile_raster_env_fallback() {
  _putenv_s("SMT_RHI2D_PARALLEL", "");
  _putenv_s("SMT_RHI2D_TILE_RASTER", "0");
  expect(scenic::detail::rhi2d_parallel_mode() ==
             scenic::detail::Rhi2dParallelMode::kSerial,
         "legacy TILE_RASTER=0 => serial");
  expect(!scenic::detail::rhi2d_tile_raster_enabled(), "env 0 disables tile");
  expect(scenic::detail::rhi2d_tile_raster_worker_count(16) == 1,
         "disabled => 1 worker");
  _putenv_s("SMT_RHI2D_TILE_RASTER", "1");
#if defined(_DEBUG)
  // Debug defaults PARALLEL unset → serial (browse stability); TILE_RASTER=1
  // alone does not override that floor — set SMT_RHI2D_PARALLEL=tile to opt in.
  expect(scenic::detail::rhi2d_parallel_mode() ==
             scenic::detail::Rhi2dParallelMode::kSerial,
         "Debug: TILE_RASTER=1 without PARALLEL => serial");
  _putenv_s("SMT_RHI2D_PARALLEL", "tile");
#endif
  expect(scenic::detail::rhi2d_parallel_mode() ==
             scenic::detail::Rhi2dParallelMode::kTile,
         "PARALLEL=tile (or Release TILE_RASTER=1) => tile");
  expect(scenic::detail::rhi2d_tile_raster_enabled(), "env enables tile");
  expect(scenic::detail::rhi2d_parallel_worker_count(16) >= 2,
         "enabled multi-job => >=2 workers");
  _putenv_s("SMT_RHI2D_PARALLEL", "layer");
  expect(scenic::detail::rhi2d_parallel_mode() ==
             scenic::detail::Rhi2dParallelMode::kLayer,
         "PARALLEL=layer");
  expect(scenic::detail::rhi2d_layer_raster_enabled(), "layer enabled");
  expect(!scenic::detail::rhi2d_tile_raster_enabled(), "layer not tile");
  _putenv_s("SMT_RHI2D_PARALLEL", "serial");
  expect(scenic::detail::rhi2d_parallel_mode() ==
             scenic::detail::Rhi2dParallelMode::kSerial,
         "PARALLEL=serial");
  _putenv_s("SMT_RHI2D_PARALLEL", "");
  _putenv_s("SMT_RHI2D_TILE_RASTER", "");
}

void test_layer_execute_colorkey_compose() {
  constexpr int kW = 32;
  constexpr int kH = 32;
  constexpr COLORREF kOcean = RGB(170, 211, 223);
  scenic::detail::Rhi2dOwnedSurface back;
  expect(back.set_size(kW, kH) == SMT_ERR_NONE, "back surface");
  back.clear(0, 0, kW, kH, kOcean);

  scenic::detail::Rhi2dCommandEncoder enc0;
  enc0.begin_pass(&back.surface());
  enc0.fill_rect(0, 0, 16, 32, RGB(255, 0, 0));
  enc0.end_pass();
  scenic::detail::Rhi2dCommandBuffer buf0 = enc0.take_buffer();

  scenic::detail::Rhi2dCommandEncoder enc1;
  enc1.begin_pass(&back.surface());
  enc1.fill_rect(16, 0, 16, 32, RGB(0, 0, 255));
  enc1.end_pass();
  scenic::detail::Rhi2dCommandBuffer buf1 = enc1.take_buffer();

  std::vector<scenic::detail::Rhi2dCommandBuffer> layers;
  layers.push_back(std::move(buf0));
  layers.push_back(std::move(buf1));

  std::vector<std::unique_ptr<scenic::detail::Rhi2dOwnedSurface>> surfs(layers.size());
  scenic::detail::Rhi2dTileGraphRunner runner;
  runner.ensure_workers(2);
  runner.run_tiles(layers.size(), [&](size_t i) {
    auto s = std::make_unique<scenic::detail::Rhi2dOwnedSurface>();
    expect(s->set_size(kW, kH) == SMT_ERR_NONE, "layer set_size");
    // Ocean pad (set_size clear) + paint; compose keys on ocean.
    expect(scenic::detail::execute(layers[i], s->surface()), "layer execute");
    surfs[i] = std::move(s);
  });

  // Soft stand-in for TransparentBlt keyed on ocean (same product compose).
  auto* bits = static_cast<uint8_t*>(back.bits());
  const uint32_t stride = back.stride_bytes();
  for (size_t i = 0; i < surfs.size(); ++i) {
    expect(surfs[i] != nullptr, "layer surf");
    if (!surfs[i]) {
      continue;
    }
    auto* src = static_cast<uint8_t*>(surfs[i]->bits());
    const uint32_t src_stride = surfs[i]->stride_bytes();
    for (int y = 0; y < kH; ++y) {
      for (int x = 0; x < kW; ++x) {
        uint8_t* sp = src + y * src_stride + x * 4;
        const COLORREF c = RGB(sp[2], sp[1], sp[0]);
        if (c == kOcean) {
          continue;
        }
        uint8_t* dp = bits + y * stride + x * 4;
        dp[0] = sp[0];
        dp[1] = sp[1];
        dp[2] = sp[2];
        dp[3] = sp[3];
      }
    }
  }

  auto sample = [&](int x, int y) -> COLORREF {
    const uint8_t* p = bits + y * stride + x * 4;
    return RGB(p[2], p[1], p[0]);
  };
  expect(sample(8, 8) == RGB(255, 0, 0), "left keeps red from layer0");
  expect(sample(24, 8) == RGB(0, 0, 255), "right stays blue from layer1");
  runner.shutdown();
}

}  // namespace

int main() {
  test_enumerate_full_and_damage();
  test_graph_runner_parallel_sum();
  test_execute_tile_stitch();
  test_graph_runner_cancel();
  test_tile_raster_env_fallback();
  test_layer_execute_colorkey_compose();
  if (g_fails != 0) {
    std::fprintf(stderr, "tile_raster_test: %d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("tile_raster_test: ok\n");
  return 0;
}
