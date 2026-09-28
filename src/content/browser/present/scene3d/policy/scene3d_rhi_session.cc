// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/policy/scene3d_rhi_session.h"

#include <atomic>

namespace content {
namespace {

std::atomic<uint32_t> g_scene3d_engine{
    static_cast<uint32_t>(Scene3dEngine::kFlyCube)};

}  // namespace

void set_scene3d_engine(Scene3dEngine engine) {
  g_scene3d_engine.store(static_cast<uint32_t>(engine),
                         std::memory_order_release);
}

Scene3dEngine scene3d_engine() {
  return static_cast<Scene3dEngine>(
      g_scene3d_engine.load(std::memory_order_acquire));
}

bool prefer_scene3d_flycube() {
  return scene3d_engine() == Scene3dEngine::kFlyCube;
}

bool prefer_scene3d_stereo_gl() {
  return scene3d_engine() == Scene3dEngine::kStereoGl;
}

bool prefer_scene3d_gdi() {
  return scene3d_engine() == Scene3dEngine::kGdi;
}

bool force_content_mapview_3d() {
  return !prefer_scene3d_flycube();
}

}  // namespace content
