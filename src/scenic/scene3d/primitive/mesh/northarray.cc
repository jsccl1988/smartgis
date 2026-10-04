// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "scenic/scene3d/primitive/mesh/northarray.h"

#include <cmath>

#include "base/math/math.h"
#include "scenic/render/err.h"
#include "scenic/render/rhi3d/public/state/states_manager.h"

namespace scenic {
namespace detail {

NorthArray::NorthArray(float init_angle, float win_h, PerspCamera* camera)
    : camera_(camera), north_pt_angle_(init_angle), win_h_(win_h) {}

NorthArray::~NorthArray() { Destroy(); }

long NorthArray::Init(::base::Vector3& pos, Material& material,
                      const char* tex_name) {
  return Object3d::Init(pos, material, tex_name);
}

long NorthArray::Create(LP3DRENDERDEVICE device) {
  if (!device) {
    return kErrInvalidParam;
  }

  device->CreateFont("Segoe UI", 13, 0, FW_SEMIBOLD, TRUE, FALSE, FALSE, 10,
                     font_clock_);

  const float r = win_h_ * 7.f / 16.f;
  vb_clock_pan_.reset(
      device->CreateVertexBuffer(361, VF_XYZ | VF_DIFFUSE, true));
  if (vb_clock_pan_ && kErrNone == vb_clock_pan_->Lock()) {
    for (int i = 0; i < 361; ++i) {
      const float x = r * std::cos(DEG2RAD(i)) + win_h_ * 17.f / 32.f;
      const float y = r * std::sin(DEG2RAD(i)) + win_h_ * 17.f / 32.f;
      vb_clock_pan_->Vertex(x, y, 0);
      vb_clock_pan_->Diffuse(0.42f, 0.58f, 0.72f, 0.85f);
    }
    vb_clock_pan_->Unlock();
  }

  const float l1 = win_h_ * 3.f / 10.f;
  const float l2 = win_h_ / 5.f;
  vb_clock_array_.reset(
      device->CreateVertexBuffer(4, VF_XYZ | VF_DIFFUSE, true));
  if (vb_clock_array_ && kErrNone == vb_clock_array_->Lock()) {
    float x = l2 * std::cos(DEG2RAD(120));
    float y = l2 * std::sin(DEG2RAD(120));
    vb_clock_array_->Vertex(x, y, 0);
    vb_clock_array_->Diffuse(0.55f, 0.12f, 0.12f, 1.f);

    x = l1 * std::cos(DEG2RAD(270));
    y = l1 * std::sin(DEG2RAD(270));
    vb_clock_array_->Vertex(x, y, 0);
    vb_clock_array_->Diffuse(0.92f, 0.28f, 0.24f, 1.f);

    vb_clock_array_->Vertex(0.f, 0.f, 0.f);
    vb_clock_array_->Diffuse(0.92f, 0.28f, 0.24f, 1.f);

    x = l2 * std::cos(DEG2RAD(60));
    y = l2 * std::sin(DEG2RAD(60));
    vb_clock_array_->Vertex(x, y, 0);
    vb_clock_array_->Diffuse(0.55f, 0.12f, 0.12f, 1.f);
    vb_clock_array_->Unlock();
  }
  return kErrNone;
}

long NorthArray::Destroy() {
  camera_ = nullptr;
  vb_clock_pan_.reset();
  vb_clock_array_.reset();
  return kErrNone;
}

long NorthArray::Update(LP3DRENDERDEVICE /*device*/, float /*elapsed*/) {
  if (!camera_) {
    return kErrNone;
  }
  Vector3 dir = camera_->target() - camera_->eye();
  dir.y = 0;
  dir.normalize();
  north_pt_angle_ = RAD2DEG(std::acos(dir.z));
  if (dir.x < 0) {
    north_pt_angle_ = 360.f - north_pt_angle_;
  }
  return kErrNone;
}

long NorthArray::Render(LP3DRENDERDEVICE device) {
  if (!device) {
    return kErrInvalidParam;
  }
  Viewport3D viewport = device->GetViewport();
  Viewport3D viewport_org = viewport;

  const ulong dial = static_cast<ulong>(win_h_);
  const ulong full_h = viewport_org.ulHeight;
  viewport.ulHeight = dial;
  viewport.ulWidth = dial;
  if (device->GetBaseApi() != RA_OPENGL && full_h > dial) {
    viewport.ulY = viewport_org.ulY + (full_h - dial);
  }

  device->SetViewport(viewport);
  device->MatrixLoadIdentity();
  device->MatrixModeSet(MM_PROJECTION);
  device->MatrixPush();
  device->MatrixLoadIdentity();
  device->SetOrtho(0., viewport.ulWidth, 0., viewport.ulHeight, -1., 1.);
  device->MatrixModeSet(MM_MODELVIEW);

  GpuStateManager* states = device->GetStateManager();
  if (states) {
    states->SetLight(false);
    states->Set2DTextures(false);
  }
  draw_clock(device);
  draw_array(device);
  if (states) {
    states->SetLight(true);
    states->Set2DTextures(true);
  }

  device->MatrixModeSet(MM_PROJECTION);
  device->MatrixPop();
  device->MatrixModeSet(MM_MODELVIEW);
  device->SetViewport(viewport_org);
  return kErrNone;
}

void NorthArray::draw_clock(LP3DRENDERDEVICE device) {
  const Color card(0.72f, 0.82f, 0.92f, 1.f);
  const float r = win_h_ * 3.f / 8.f;
  float x = r * std::cos(DEG2RAD(0)) + win_h_ / 2.f;
  float y = r * std::sin(DEG2RAD(0)) + win_h_ / 2.f;
  device->DrawText(font_clock_, x, y, card, "E");
  x = r * std::cos(DEG2RAD(270)) + win_h_ / 2.f;
  y = r * std::sin(DEG2RAD(270)) + win_h_ / 2.f;
  device->DrawText(font_clock_, x, y, card, "N");
  x = r * std::cos(DEG2RAD(180)) + win_h_ / 2.f;
  y = r * std::sin(DEG2RAD(180)) + win_h_ / 2.f;
  device->DrawText(font_clock_, x, y, card, "W");
  x = r * std::cos(DEG2RAD(90)) + win_h_ / 2.f;
  y = r * std::sin(DEG2RAD(90)) + win_h_ / 2.f;
  device->DrawText(font_clock_, x, y, card, "S");
  if (vb_clock_pan_) {
    device->DrawPrimitives(PT_LINESTRIP, vb_clock_pan_.get(), 0, 360);
  }
}

void NorthArray::draw_array(LP3DRENDERDEVICE device) {
  device->DrawText(font_clock_, 0, win_h_ * 3.f / 32.f,
                   Color(0.78f, 0.84f, 0.92f, 1.f), "%.0f",
                   360.f - north_pt_angle_);
  if (!vb_clock_array_) {
    return;
  }
  device->MatrixPush();
  device->MatrixTranslation(win_h_ * 17.f / 32.f, win_h_ * 17.f / 32.f, 0.f);
  device->MatrixRotation(north_pt_angle_ - 180.f, 0, 0, 1);
  device->DrawPrimitives(PT_TRIANGLESTRIP, vb_clock_array_.get(), 0, 2);
  device->MatrixPop();
}

}  // namespace detail
}  // namespace scenic
