// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/engine.h"

#include <cstdio>
#include <memory>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void run_kind(scenic::Kind kind) {
  std::unique_ptr<scenic::Engine> engine(scenic::create_null_engine(kind));
  expect(engine != nullptr, "create_null_engine");
  expect(engine->is_null(), "is_null");
  expect(engine->kind() == kind, "kind");
  scenic::SessionDesc desc;
  desc.width_px = 1280;
  desc.height_px = 720;
  expect(engine->initialize(desc), "initialize");
  expect(engine->present(), "present");
  expect(!engine->paint_hdc(nullptr, 0, 0), "null paint_hdc");
  engine->shutdown();
}

void run_map2d_engine() {
  std::unique_ptr<scenic::Engine> engine(scenic::create_map2d_engine());
  expect(engine != nullptr, "create_map2d_engine");
  expect(!engine->is_null(), "map2d not null");
  expect(engine->kind() == scenic::Kind::kMap2d, "map2d kind");
  scenic::SessionDesc desc;
  desc.width_px = 64;
  desc.height_px = 48;
  expect(engine->initialize(desc), "map2d initialize");
  expect(engine->present(), "map2d present");
}

void run_scene3d_engine() {
  std::unique_ptr<scenic::Engine> engine(scenic::create_scene3d_engine());
  expect(engine != nullptr, "create_scene3d_engine");
  expect(!engine->is_null(), "scene3d not null");
  expect(engine->kind() == scenic::Kind::kScene3d, "scene3d kind");
  scenic::SessionDesc desc;
  desc.width_px = 64;
  desc.height_px = 48;
  expect(engine->initialize(desc), "scene3d initialize");
  expect(engine->present(), "scene3d present");
}

}  // namespace

int main() {
  run_kind(scenic::Kind::kMap2d);
  run_kind(scenic::Kind::kScene3d);
  run_map2d_engine();
  run_scene3d_engine();
  if (g_fails != 0) {
    std::fprintf(stderr, "scenic_engine_test: %d fail(s)\n", g_fails);
    return 1;
  }
  return 0;
}
