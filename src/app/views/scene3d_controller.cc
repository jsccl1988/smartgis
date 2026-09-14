// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/scene3d_controller.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace app {
namespace {

constexpr float kFovY = 0.785398f;  // ~45 deg

}  // namespace

Scene3dController::Scene3dController() = default;

Scene3dController::~Scene3dController() {
  release_mesh();
}

void Scene3dController::abandon_mesh() {
  cube_vb_ = nullptr;
  cube_ib_ = nullptr;
  mesh_device_ = nullptr;
  cube_index_count_ = 0;
}

void Scene3dController::release_mesh() {
  if (mesh_device_) {
    if (cube_vb_) {
      mesh_device_->destroy_buffer(cube_vb_);
    }
    if (cube_ib_) {
      mesh_device_->destroy_buffer(cube_ib_);
    }
  }
  cube_vb_ = nullptr;
  cube_ib_ = nullptr;
  mesh_device_ = nullptr;
  cube_index_count_ = 0;
}

void Scene3dController::reset() {
  yaw_ = 0.55f;
  pitch_ = 0.4f;
  distance_ = 3.2f;
  has_last_ = false;
}

void Scene3dController::apply_draft(const tool::Draft& draft) {
  if (draft.kind == tool::DraftKind::kWheel) {
    if (draft.wheel > 0) {
      distance_ *= 0.9f;
    } else if (draft.wheel < 0) {
      distance_ *= 1.1f;
    }
    distance_ = std::clamp(distance_, 1.2f, 12.f);
    return;
  }

  if (draft.points.empty()) {
    return;
  }

  if (draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    const int x0 = draft.points[0].x_px;
    const int y0 = draft.points[0].y_px;
    const int x1 = draft.points[1].x_px;
    const int y1 = draft.points[1].y_px;
    yaw_ += static_cast<float>(x1 - x0) * 0.01f;
    pitch_ += static_cast<float>(y1 - y0) * 0.01f;
    pitch_ = std::clamp(pitch_, -1.2f, 1.2f);
    last_x_ = x1;
    last_y_ = y1;
    has_last_ = true;
    return;
  }

  if (draft.kind == tool::DraftKind::kPoint) {
    const int x = draft.points.front().x_px;
    const int y = draft.points.front().y_px;
    if (has_last_) {
      yaw_ += static_cast<float>(x - last_x_) * 0.01f;
      pitch_ += static_cast<float>(y - last_y_) * 0.01f;
      pitch_ = std::clamp(pitch_, -1.2f, 1.2f);
    }
    last_x_ = x;
    last_y_ = y;
    has_last_ = true;
  }
}

render::rhi::CameraMatrices Scene3dController::camera_matrices(
    float aspect) const {
  return render::rhi::make_orbit_camera(yaw_, pitch_, distance_, kFovY, aspect,
                                        0.1f, 100.f);
}

bool Scene3dController::ensure_cube_mesh(render::rhi::Device* device) {
  if (!device) {
    return false;
  }
  if (mesh_device_ == device && cube_vb_ && cube_ib_ && cube_index_count_ > 0) {
    return true;
  }
  release_mesh();
  // Unit cube corners as float3 positions (solid FlyCube VS stride).
  const float corners[8][3] = {{-1.f, -1.f, -1.f}, {1.f, -1.f, -1.f},
                               {1.f, 1.f, -1.f},  {-1.f, 1.f, -1.f},
                               {-1.f, -1.f, 1.f},  {1.f, -1.f, 1.f},
                               {1.f, 1.f, 1.f},   {-1.f, 1.f, 1.f}};
  // Two triangles per face.
  const uint32_t indices[] = {
      0, 1, 2, 0, 2, 3, 4, 6, 5, 4, 7, 6, 0, 4, 5, 0, 5, 1,
      1, 5, 6, 1, 6, 2, 2, 6, 7, 2, 7, 3, 3, 7, 4, 3, 4, 0};
  cube_vb_ = device->create_buffer(sizeof(corners),
                                   render::rhi::BufferUsage::kVertex);
  cube_ib_ = device->create_buffer(sizeof(indices),
                                   render::rhi::BufferUsage::kIndex);
  if (!cube_vb_ || !cube_ib_ ||
      !device->upload(cube_vb_, corners, sizeof(corners)) ||
      !device->upload(cube_ib_, indices, sizeof(indices))) {
    if (cube_vb_) {
      device->destroy_buffer(cube_vb_);
    }
    if (cube_ib_) {
      device->destroy_buffer(cube_ib_);
    }
    cube_vb_ = nullptr;
    cube_ib_ = nullptr;
    return false;
  }
  mesh_device_ = device;
  cube_index_count_ = static_cast<uint32_t>(sizeof(indices) / sizeof(indices[0]));
  return true;
}

bool Scene3dController::present_gpu(render::rhi::Device* device,
                                    uint32_t width_px, uint32_t height_px) {
  if (!device || width_px == 0 || height_px == 0) {
    return false;
  }
  if (!ensure_cube_mesh(device)) {
    return false;
  }
  render::rhi::CommandList* list = device->create_command_list();
  if (!list) {
    return false;
  }
  const float aspect = static_cast<float>(width_px) /
                       static_cast<float>(height_px > 0 ? height_px : 1);
  render::rhi::RenderPassDesc pass;
  pass.clear_r = 0.05f;
  pass.clear_g = 0.12f;
  pass.clear_b = 0.18f;
  pass.clear_a = 1.f;
  pass.width = width_px;
  pass.height = height_px;
  list->begin_render_pass(pass);
  list->set_viewport(0, 0, static_cast<float>(width_px),
                     static_cast<float>(height_px), 0, 1);
  list->bind_camera(camera_matrices(aspect));
  list->set_solid_color(1.f, 0.78f, 0.35f, 1.f);
  list->bind_vertex_buffer(cube_vb_, 0, 3 * sizeof(float));
  list->bind_index_buffer(cube_ib_, 0);
  list->draw_indexed(cube_index_count_, 1, 0, 0, 0);
  list->end_render_pass();
  list->close();
  const bool ok = device->execute(list);
  device->destroy_command_list(list);
  if (ok) {
    device->present();
  }
  return ok;
}

void Scene3dController::project(float x, float y, float z, int width_px,
                                int height_px, int* sx, int* sy) const {
  const float cy = std::cos(yaw_);
  const float syaw = std::sin(yaw_);
  const float cp = std::cos(pitch_);
  const float sp = std::sin(pitch_);
  const float x1 = x * cy - z * syaw;
  const float z1 = x * syaw + z * cy;
  const float y2 = y * cp - z1 * sp;
  const float z2 = y * sp + z1 * cp;
  const float depth = z2 + distance_;
  const float inv = depth > 0.15f ? (1.f / depth) : (1.f / 0.15f);
  const float f = 280.f * inv;
  if (sx) {
    *sx = width_px / 2 + static_cast<int>(std::lround(x1 * f));
  }
  if (sy) {
    *sy = height_px / 2 - static_cast<int>(std::lround(y2 * f));
  }
}

void Scene3dController::paint_hud(HDC hdc, int width_px, int height_px) const {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(220, 235, 250));
  wchar_t line[160];
  swprintf_s(line,
             L"3D FlyCube  yaw=%.2f pitch=%.2f dist=%.2f  (drag/wheel)", yaw_,
             pitch_, distance_);
  TextOutW(hdc, 12, 12, line, lstrlenW(line));
  TextOutW(hdc, 12, 32, L"view3d.trackball — orbit camera → GPU present", 46);
}

void Scene3dController::paint(HDC hdc, int width_px, int height_px) const {
  if (!hdc || width_px <= 0 || height_px <= 0) {
    return;
  }

  HBRUSH bg = CreateSolidBrush(RGB(18, 32, 48));
  RECT full = {0, 0, width_px, height_px};
  FillRect(hdc, &full, bg);
  DeleteObject(bg);

  HPEN grid_pen = CreatePen(PS_SOLID, 1, RGB(70, 110, 150));
  HGDIOBJ old_pen = SelectObject(hdc, grid_pen);
  for (int i = -3; i <= 3; ++i) {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    project(static_cast<float>(i), 0.f, -3.f, width_px, height_px, &x0, &y0);
    project(static_cast<float>(i), 0.f, 3.f, width_px, height_px, &x1, &y1);
    MoveToEx(hdc, x0, y0, nullptr);
    LineTo(hdc, x1, y1);
    project(-3.f, 0.f, static_cast<float>(i), width_px, height_px, &x0, &y0);
    project(3.f, 0.f, static_cast<float>(i), width_px, height_px, &x1, &y1);
    MoveToEx(hdc, x0, y0, nullptr);
    LineTo(hdc, x1, y1);
  }
  SelectObject(hdc, old_pen);
  DeleteObject(grid_pen);

  const float s = 1.f;
  const float corners[8][3] = {{-s, -s, -s}, {s, -s, -s}, {s, s, -s}, {-s, s, -s},
                               {-s, -s, s},  {s, -s, s},  {s, s, s},  {-s, s, s}};
  int pts[8][2] = {};
  for (int i = 0; i < 8; ++i) {
    project(corners[i][0], corners[i][1], corners[i][2], width_px, height_px,
            &pts[i][0], &pts[i][1]);
  }
  const int edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                            {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
  HPEN cube_pen = CreatePen(PS_SOLID, 2, RGB(255, 200, 120));
  old_pen = SelectObject(hdc, cube_pen);
  for (const auto& e : edges) {
    MoveToEx(hdc, pts[e[0]][0], pts[e[0]][1], nullptr);
    LineTo(hdc, pts[e[1]][0], pts[e[1]][1]);
  }
  SelectObject(hdc, old_pen);
  DeleteObject(cube_pen);

  paint_hud(hdc, width_px, height_px);
  TextOutW(hdc, 12, 52, L"(GDI fallback — no FlyCube device)", 34);
}

}  // namespace app
