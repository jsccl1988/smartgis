// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/software/scene3d_software_painter.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/frame/map2d_carto.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "gis/vista/domain/atmosphere/systems/atmosphere_params.h"
#include "gis/vista/domain/atmosphere/field/field_channel.h"
#include "render/rhi/rhi.h"
#include "base/trace/event/process_trace.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace content {

Scene3dSoftwarePainter::~Scene3dSoftwarePainter() {
  release_engine_logo_overlay();
}

void Scene3dSoftwarePainter::bind(Scene3dGpuPresent* gpu,
                                  AtmosphereSession* atmosphere,
                                  const OrbitFrame* orbit,
                                  const MapScene* scene,
                                  const ViewFrame* label_frame) {
  gpu_ = gpu;
  atmosphere_ = atmosphere;
  orbit_ = orbit;
  scene_ = scene;
  label_frame_ = label_frame;
}

namespace {

constexpr wchar_t kEngineLogoClass[] = L"SmartGisEngineLogoBadge";
constexpr int kEngineLogoPadX = 10;
constexpr int kEngineLogoPadY = 6;
constexpr int kEngineLogoMargin = 14;

bool measure_engine_logo(HDC hdc, const char* engine_name, SIZE* out_box) {
  if (!hdc || !out_box) {
    return false;
  }
  const char* name =
      (engine_name && engine_name[0]) ? engine_name : "unknown";
  wchar_t wname[96];
  swprintf_s(wname, L"%hs", name);
  SIZE sz = {};
  if (!GetTextExtentPoint32W(hdc, wname, lstrlenW(wname), &sz)) {
    return false;
  }
  out_box->cx = sz.cx + kEngineLogoPadX * 2;
  out_box->cy = sz.cy + kEngineLogoPadY * 2;
  return out_box->cx > 0 && out_box->cy > 0;
}

void paint_engine_logo_at(HDC hdc, int x, int y, const char* engine_name) {
  if (!hdc) {
    return;
  }
  const char* name =
      (engine_name && engine_name[0]) ? engine_name : "unknown";
  wchar_t wname[96];
  swprintf_s(wname, L"%hs", name);
  SIZE box = {};
  if (!measure_engine_logo(hdc, name, &box)) {
    return;
  }
  RECT rc = {x, y, x + box.cx, y + box.cy};
  HBRUSH brush = CreateSolidBrush(RGB(18, 26, 34));
  FillRect(hdc, &rc, brush);
  DeleteObject(brush);
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(90, 110, 130));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  Rectangle(hdc, rc.left, rc.top, rc.right, rc.bottom);
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(pen);
  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(245, 248, 252));
  TextOutW(hdc, x + kEngineLogoPadX, y + kEngineLogoPadY, wname,
           lstrlenW(wname));
}

LRESULT CALLBACK engine_logo_wnd_proc(HWND hwnd, UINT msg, WPARAM wparam,
                                      LPARAM lparam) {
  if (msg == WM_PAINT) {
    PAINTSTRUCT ps = {};
    HDC hdc = BeginPaint(hwnd, &ps);
    const char* name =
        reinterpret_cast<const char*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    paint_engine_logo_at(hdc, 0, 0, name);
    EndPaint(hwnd, &ps);
    return 0;
  }
  if (msg == WM_ERASEBKGND) {
    return 1;
  }
  if (msg == WM_NCHITTEST) {
    // Let map / orbit gestures hit the parent under the badge.
    return HTTRANSPARENT;
  }
  return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void register_engine_logo_class() {
  static bool done = false;
  if (done) {
    return;
  }
  WNDCLASSEXW wc = {};
  wc.cbSize = sizeof(wc);
  wc.lpfnWndProc = engine_logo_wnd_proc;
  wc.hInstance = GetModuleHandleW(nullptr);
  wc.hCursor = ::LoadCursor(nullptr, IDC_ARROW);
  wc.hbrBackground = nullptr;
  wc.lpszClassName = kEngineLogoClass;
  RegisterClassExW(&wc);
  done = true;
}

}  // namespace

void Scene3dSoftwarePainter::paint_engine_logo(HDC hdc, int width_px, int height_px,
                                          const char* engine_name) {
  if (!hdc || width_px < 48 || height_px < 28) {
    return;
  }
  SIZE box = {};
  if (!measure_engine_logo(hdc, engine_name, &box)) {
    return;
  }
  const int x = width_px - kEngineLogoMargin - box.cx;
  const int y = height_px - kEngineLogoMargin - box.cy;
  if (x < 4 || y < 4) {
    return;
  }
  paint_engine_logo_at(hdc, x, y, engine_name);
}

void Scene3dSoftwarePainter::hide_engine_logo_overlay() const {
  if (logo_hwnd_ && IsWindow(logo_hwnd_)) {
    ShowWindow(logo_hwnd_, SW_HIDE);
  }
}

void Scene3dSoftwarePainter::release_engine_logo_overlay() const {
  HWND hwnd = logo_hwnd_;
  logo_hwnd_ = nullptr;
  if (!hwnd || !IsWindow(hwnd)) {
    return;
  }
  SetWindowLongPtrW(hwnd, GWLP_USERDATA, 0);
  // Parent teardown already destroys children; avoid DestroyWindow on a
  // half-torn-down hierarchy (exit AV on Views/WinUI smoke).
  HWND parent = GetParent(hwnd);
  if (parent && IsWindow(parent)) {
    DestroyWindow(hwnd);
  }
}

void Scene3dSoftwarePainter::sync_engine_logo_overlay(HWND parent, int width_px,
                                                 int height_px) const {
  if (!parent || !IsWindow(parent) || width_px < 48 || height_px < 28) {
    hide_engine_logo_overlay();
    return;
  }
  register_engine_logo_class();
  HDC probe = GetDC(parent);
  if (!probe) {
    return;
  }
  SIZE box = {};
  const char* label = gpu_->engine_fps_label_[0] ? gpu_->engine_fps_label_
                                                : gpu_->render_engine_name;
  const bool measured = measure_engine_logo(probe, label, &box);
  ReleaseDC(parent, probe);
  if (!measured) {
    return;
  }
  const int x = width_px - kEngineLogoMargin - box.cx;
  const int y = height_px - kEngineLogoMargin - box.cy;
  if (x < 4 || y < 4) {
    hide_engine_logo_overlay();
    return;
  }
  if (!logo_hwnd_ || !IsWindow(logo_hwnd_) ||
      GetParent(logo_hwnd_) != parent) {
    release_engine_logo_overlay();
    logo_hwnd_ = CreateWindowExW(
        WS_EX_NOACTIVATE, kEngineLogoClass, L"",
        WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, x, y, box.cx, box.cy, parent,
        nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!logo_hwnd_) {
      return;
    }
  } else {
    SetWindowPos(logo_hwnd_, HWND_TOP, x, y, box.cx, box.cy,
                 SWP_NOACTIVATE | SWP_SHOWWINDOW);
  }
  SetWindowLongPtrW(logo_hwnd_, GWLP_USERDATA,
                    reinterpret_cast<LONG_PTR>(label));
  // Invalidate only â?UpdateWindow here re-enters while the parent is still
  // inside BeginPaint/EndPaint (FlyCube / stereo present paths).
  InvalidateRect(logo_hwnd_, nullptr, FALSE);
}


void Scene3dSoftwarePainter::project(float x, float y, float z, int width_px,
                               int height_px, int* sx, int* sy) const {
  if (!orbit_) {
    if (sx) {
      *sx = 0;
    }
    if (sy) {
      *sy = 0;
    }
    return;
  }
  orbit_->project(x, y, z, width_px, height_px, sx, sy);
}


void Scene3dSoftwarePainter::project_lon_lat(double lon, double lat, int width_px,
                                       int height_px, int* sx,
                                       int* sy) const {
  if (!orbit_) {
    if (sx) {
      *sx = 0;
    }
    if (sy) {
      *sy = 0;
    }
    return;
  }
  orbit_->project_lon_lat(lon, lat, width_px, height_px, sx, sy);
}


void Scene3dSoftwarePainter::paint_wind_arrows(HDC hdc, int width_px,
                                          int height_px) const {
  if (!hdc || !atmosphere_ || !gpu_ || !atmosphere_->wind_overlay_enabled() ||
      !atmosphere_->environment() || width_px <= 0 || height_px <= 0) {
    return;
  }
  const gis::atmosphere::FieldStore& store = atmosphere_->environment()->field_store();
  if (store.layer_count() == 0) {
    return;
  }
  const content::Extent2 e = gpu_->world_extent();
  const double lon_span = e.xmax - e.xmin;
  const double lat_span = e.ymax - e.ymin;
  if (!(lon_span > 0.0) || !(lat_span > 0.0)) {
    return;
  }

  constexpr int kGrid = 10;
  const double t = atmosphere_->time_sec();
  HPEN pen = CreatePen(PS_SOLID, 1, RGB(120, 200, 255));
  HGDIOBJ old_pen = SelectObject(hdc, pen);
  for (int j = 0; j < kGrid; ++j) {
    for (int i = 0; i < kGrid; ++i) {
      const double lon =
          e.xmin + (static_cast<double>(i) + 0.5) / kGrid * lon_span;
      const double lat =
          e.ymin + (static_cast<double>(j) + 0.5) / kGrid * lat_span;
      const float u =
          store.sample(gis::atmosphere::FieldChannel::kWindU, lon, lat, t);
      const float v =
          store.sample(gis::atmosphere::FieldChannel::kWindV, lon, lat, t);
      const float speed = std::sqrt(u * u + v * v);
      if (!(speed > 1.0e-3f)) {
        continue;
      }
      int sx = 0;
      int sy = 0;
      project_lon_lat(lon, lat, width_px, height_px, &sx, &sy);
      if (sx < -20 || sy < -20 || sx > width_px + 20 || sy > height_px + 20) {
        continue;
      }
      // Arrow length scales with speed; cap so the grid stays readable.
      const float len = (std::min)(28.f, 6.f + speed * 1.2f);
      const float inv = 1.f / speed;
      const float dx = u * inv * len;
      const float dy = -v * inv * len;  // screen Y down; V is northward
      const int ex = sx + static_cast<int>(std::lround(dx));
      const int ey = sy + static_cast<int>(std::lround(dy));
      MoveToEx(hdc, sx, sy, nullptr);
      LineTo(hdc, ex, ey);
      // Simple arrowhead.
      const float hx = -dx * 0.25f;
      const float hy = -dy * 0.25f;
      const float px = -hy * 0.6f;
      const float py = hx * 0.6f;
      MoveToEx(hdc, ex, ey, nullptr);
      LineTo(hdc, ex + static_cast<int>(std::lround(hx + px)),
             ey + static_cast<int>(std::lround(hy + py)));
      MoveToEx(hdc, ex, ey, nullptr);
      LineTo(hdc, ex + static_cast<int>(std::lround(hx - px)),
             ey + static_cast<int>(std::lround(hy - py)));
    }
  }
  SelectObject(hdc, old_pen);
  DeleteObject(pen);
}

void Scene3dSoftwarePainter::paint_hud(HDC hdc, int width_px, int height_px) const {
  if (!hdc || !gpu_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  gpu_->remember_view_size(width_px, height_px);
  paint_wind_arrows(hdc, width_px, height_px);

  // Compass rose: needle points to geographic north on screen (default orbit
  // frames north toward the top of the viewport).
  {
    const int cx = width_px - 56;
    const int cy = 56;
    const int r = 28;
    HPEN ring = CreatePen(PS_SOLID, 2, RGB(210, 225, 240));
    HGDIOBJ old_pen = SelectObject(hdc, ring);
    HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
    Ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    const float angle = gpu_->yaw() - kScene3dDefaultYaw;
    const float nx = std::sin(angle);
    const float ny = -std::cos(angle);
    const int tip_x = cx + static_cast<int>(std::lround(nx * (r - 6)));
    const int tip_y = cy + static_cast<int>(std::lround(ny * (r - 6)));
    HPEN needle = CreatePen(PS_SOLID, 2, RGB(220, 60, 50));
    SelectObject(hdc, needle);
    MoveToEx(hdc, cx, cy, nullptr);
    LineTo(hdc, tip_x, tip_y);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(needle);
    DeleteObject(ring);
    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, RGB(235, 245, 255));
    TextOutW(hdc, cx - 5, cy - r - 18, L"N", 1);
  }

  SetBkMode(hdc, TRANSPARENT);
  SetTextColor(hdc, RGB(220, 235, 250));
  wchar_t line[220];
  swprintf_s(line,
             L"Shared scene  yaw=%.2f pitch=%.2f dist=%.2f  "
             L"(wheel@cursor, pan, pinch)",
             gpu_->yaw(), gpu_->pitch(), gpu_->distance());
  TextOutW(hdc, 12, 12, line, lstrlenW(line));
  const wchar_t* so_t =
      hosts_shared_scene_
          ? L"Orbit DEM SoT â?leftover SmartGis.exe is reference"
          : L"Local DEM SoT â?leftover SmartGis.exe is reference";
  TextOutW(hdc, 12, 32, so_t, lstrlenW(so_t));

  wchar_t eng[140];
  const char* name = gpu_->render_engine_name ? gpu_->render_engine_name : "unknown";
  swprintf_s(eng, L"Engine  %hs  Fps%.3f%s", name, gpu_->last_fps,
             gpu_->wireframe_enabled() ? L"  wireframe=on" : L"");
  TextOutW(hdc, 12, 52, eng, lstrlenW(eng));

  const wchar_t* wasd = L"WASD  orbit / wheel zoom";
  TextOutW(hdc, 12, height_px > 40 ? height_px - 28 : 72, wasd, lstrlenW(wasd));
  if (atmosphere_ && atmosphere_->environment()) {
    wchar_t atmo[160];
    swprintf_s(atmo,
               L"Atmosphere  t=%.1fs  ocean=%d cloud=%d wind=%d",
               atmosphere_->time_sec(),
               atmosphere_->environment()->ocean_enabled() ? 1 : 0,
               atmosphere_->environment()->cloud_enabled() ? 1 : 0,
               atmosphere_->wind_overlay_enabled() ? 1 : 0);
    TextOutW(hdc, 12, 72, atmo, lstrlenW(atmo));
  }
  if (gpu_->wireframe_enabled()) {
    paint_wireframe_edges(hdc, width_px, height_px);
  }
  // Bottom-right engine badge last so it stays readable over wireframe.
  // DXGI/GL flip surfaces ignore GDI on the present HWND â?use a WS_CHILD
  // badge. Mem-DC BitBlt paths and soft GDI / ContentMapView bake into HDC.
  const HWND hwnd = WindowFromDC(hdc);
  const bool window_dc =
      hwnd != nullptr && GetObjectType(hdc) != OBJ_MEMDC;
  const bool swapchain_surface =
      std::strncmp(name, "FlyCube/", 8) == 0 ||
      std::strcmp(name, "Stereo/GL") == 0;
  if (window_dc && swapchain_surface) {
    sync_engine_logo_overlay(hwnd, width_px, height_px);
    if (!logo_hwnd_ || !IsWindow(logo_hwnd_)) {
      paint_engine_logo(hdc, width_px, height_px,
                        gpu_->engine_fps_label_[0] ? gpu_->engine_fps_label_
                                                  : gpu_->render_engine_name);
    }
  } else {
    hide_engine_logo_overlay();
    paint_engine_logo(hdc, width_px, height_px,
                      gpu_->engine_fps_label_[0] ? gpu_->engine_fps_label_
                                                : gpu_->render_engine_name);
  }
}

namespace {

// Leftover SmartGis.exe hypsometric character: low greenâyellow, high pink/white.
COLORREF hypsometric_rgb(float t01) {
  t01 = std::clamp(t01, 0.f, 1.f);
  int r = 0;
  int g = 0;
  int b = 0;
  if (t01 < 0.35f) {
    const float u = t01 / 0.35f;
    r = static_cast<int>(70 + 140 * u);
    g = static_cast<int>(140 + 70 * u);
    b = static_cast<int>(55 + 20 * (1.f - u));
  } else if (t01 < 0.65f) {
    const float u = (t01 - 0.35f) / 0.30f;
    r = static_cast<int>(210 + 25 * u);
    g = static_cast<int>(210 - 40 * u);
    b = static_cast<int>(75 + 40 * u);
  } else {
    const float u = (t01 - 0.65f) / 0.35f;
    r = static_cast<int>(235 + 20 * u);
    g = static_cast<int>(170 + 70 * u);
    b = static_cast<int>(115 + 120 * u);
  }
  return RGB(r, g, b);
}

}  // namespace

void Scene3dSoftwarePainter::paint_wireframe_edges(HDC hdc, int width_px,
                                              int height_px) const {
  if (!hdc || width_px <= 0 || height_px <= 0 || gpu_->local_idx().size() < 3 ||
      gpu_->local_xyz().size() < 9) {
    return;
  }
  HPEN edge = CreatePen(PS_SOLID, 1, RGB(40, 50, 60));
  HGDIOBJ old_pen = SelectObject(hdc, edge);
  const size_t total_tris = gpu_->local_idx().size() / 3;
  constexpr size_t kMaxEdges = 16000;
  const size_t step =
      total_tris > kMaxEdges ? (total_tris + kMaxEdges - 1) / kMaxEdges : 1;
  size_t drawn = 0;
  for (size_t t = 0; t < total_tris && drawn < kMaxEdges; t += step, ++drawn) {
    const unsigned i0 = gpu_->local_idx()[t * 3];
    const unsigned i1 = gpu_->local_idx()[t * 3 + 1];
    const unsigned i2 = gpu_->local_idx()[t * 3 + 2];
    if ((i0 + 1) * 3 > gpu_->local_xyz().size() || (i1 + 1) * 3 > gpu_->local_xyz().size() ||
        (i2 + 1) * 3 > gpu_->local_xyz().size()) {
      continue;
    }
    int p0[2] = {};
    int p1[2] = {};
    int p2[2] = {};
    project(gpu_->local_xyz()[i0 * 3], gpu_->local_xyz()[i0 * 3 + 1], gpu_->local_xyz()[i0 * 3 + 2],
            width_px, height_px, &p0[0], &p0[1]);
    project(gpu_->local_xyz()[i1 * 3], gpu_->local_xyz()[i1 * 3 + 1], gpu_->local_xyz()[i1 * 3 + 2],
            width_px, height_px, &p1[0], &p1[1]);
    project(gpu_->local_xyz()[i2 * 3], gpu_->local_xyz()[i2 * 3 + 1], gpu_->local_xyz()[i2 * 3 + 2],
            width_px, height_px, &p2[0], &p2[1]);
    MoveToEx(hdc, p0[0], p0[1], nullptr);
    LineTo(hdc, p1[0], p1[1]);
    LineTo(hdc, p2[0], p2[1]);
    LineTo(hdc, p0[0], p0[1]);
  }
  SelectObject(hdc, old_pen);
  DeleteObject(edge);
}

void Scene3dSoftwarePainter::paint(HDC hdc, int width_px, int height_px,
                              bool fill_background) const {
  BASE_TRACE_EVENT("gdi", "scene3d.gdi");
  if (!hdc || !gpu_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  std::lock_guard<std::mutex> lock(gpu_->mutex());
  gpu_->remember_view_size(width_px, height_px);
  gpu_->render_engine_name = "GDI";
  gpu_->note_present_frame();

  if (fill_background) {
    // Black void behind the ocean plane (leftover stereo SoT).
    HBRUSH bg = CreateSolidBrush(RGB(0, 0, 0));
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  gpu_->rebuild_local_mesh();
  gpu_->attach_overlay_tin_locked();
  gpu_->attach_overlay_pointcloud_locked();

  // Light-blue ocean / base plane under the DEM AABB (leftover character).
  if (!gpu_->local_xyz().empty()) {
    float minx = gpu_->local_xyz()[0];
    float maxx = minx;
    float miny = gpu_->local_xyz()[1];
    float maxy = miny;
    float minz = gpu_->local_xyz()[2];
    float maxz = minz;
    for (size_t i = 0; i + 2 < gpu_->local_xyz().size(); i += 3) {
      minx = (std::min)(minx, gpu_->local_xyz()[i]);
      maxx = (std::max)(maxx, gpu_->local_xyz()[i]);
      miny = (std::min)(miny, gpu_->local_xyz()[i + 1]);
      maxy = (std::max)(maxy, gpu_->local_xyz()[i + 1]);
      minz = (std::min)(minz, gpu_->local_xyz()[i + 2]);
      maxz = (std::max)(maxz, gpu_->local_xyz()[i + 2]);
    }
    const float y_plane = miny - 0.02f * (std::max)(maxy - miny, 0.05f);
    int c[4][2] = {};
    project(minx, y_plane, minz, width_px, height_px, &c[0][0], &c[0][1]);
    project(maxx, y_plane, minz, width_px, height_px, &c[1][0], &c[1][1]);
    project(maxx, y_plane, maxz, width_px, height_px, &c[2][0], &c[2][1]);
    project(minx, y_plane, maxz, width_px, height_px, &c[3][0], &c[3][1]);
    const POINT ocean[4] = {{c[0][0], c[0][1]},
                            {c[1][0], c[1][1]},
                            {c[2][0], c[2][1]},
                            {c[3][0], c[3][1]}};
    HBRUSH ocean_br = CreateSolidBrush(RGB(120, 190, 230));
    HPEN ocean_pen = CreatePen(PS_SOLID, 1, RGB(90, 160, 210));
    HGDIOBJ old_pen = SelectObject(hdc, ocean_pen);
    HGDIOBJ old_brush = SelectObject(hdc, ocean_br);
    Polygon(hdc, ocean, 4);
    SelectObject(hdc, old_brush);
    SelectObject(hdc, old_pen);
    DeleteObject(ocean_br);
    DeleteObject(ocean_pen);
  }

  // Continuous DEM: draw all tris up to a high cap. Sparse stride left
  // fragmented olive ribbons (not leftover hypsometric land).
  HPEN mesh_pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
  HGDIOBJ old_pen = SelectObject(hdc, mesh_pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  const size_t total_tris = gpu_->local_idx().size() / 3;
  constexpr size_t kMaxDraw = 24000;
  const size_t step =
      total_tris > kMaxDraw ? (total_tris + kMaxDraw - 1) / kMaxDraw : 1;

  float elev_min = 0.f;
  float elev_max = 0.f;
  bool elev_init = false;
  const size_t dem_idx_end = gpu_->dem_local_idx_count();
  // Hypsometric range from DEM verts only so lifted water does not skew land.
  size_t dem_vert_floats = gpu_->local_xyz().size();
  if (dem_idx_end > 0 && dem_idx_end <= gpu_->local_idx().size()) {
    size_t max_vi = 0;
    for (size_t i = 0; i < dem_idx_end; ++i) {
      max_vi =
          (std::max)(max_vi, static_cast<size_t>(gpu_->local_idx()[i]));
    }
    dem_vert_floats = (std::min)(gpu_->local_xyz().size(), (max_vi + 1) * 3);
  }
  for (size_t i = 1; i + 2 < dem_vert_floats; i += 3) {
    const float y = gpu_->local_xyz()[i];
    if (!elev_init) {
      elev_min = elev_max = y;
      elev_init = true;
    } else {
      elev_min = (std::min)(elev_min, y);
      elev_max = (std::max)(elev_max, y);
    }
  }
  const float elev_span = (std::max)(elev_max - elev_min, 1.0e-3f);

  const bool water_tint = gpu_->overlay_tin_has_albedo();
  const uint8_t* water_rgb =
      water_tint ? gpu_->overlay_tin_albedo() : nullptr;

  size_t drawn = 0;
  for (size_t t = 0; t < total_tris && drawn < kMaxDraw; t += step, ++drawn) {
    const unsigned i0 = gpu_->local_idx()[t * 3];
    const unsigned i1 = gpu_->local_idx()[t * 3 + 1];
    const unsigned i2 = gpu_->local_idx()[t * 3 + 2];
    if ((i0 + 1) * 3 > gpu_->local_xyz().size() || (i1 + 1) * 3 > gpu_->local_xyz().size() ||
        (i2 + 1) * 3 > gpu_->local_xyz().size()) {
      continue;
    }
    const bool is_overlay =
        dem_idx_end > 0 && (t * 3) >= dem_idx_end;
    HBRUSH fill = nullptr;
    if (is_overlay && water_rgb) {
      fill = CreateSolidBrush(
          RGB(water_rgb[0], water_rgb[1], water_rgb[2]));
    } else {
      const float y0 = gpu_->local_xyz()[i0 * 3 + 1];
      const float y1 = gpu_->local_xyz()[i1 * 3 + 1];
      const float y2 = gpu_->local_xyz()[i2 * 3 + 1];
      const float yavg = (y0 + y1 + y2) / 3.f;
      const float t01 = (yavg - elev_min) / elev_span;
      fill = CreateSolidBrush(hypsometric_rgb(t01));
    }
    SelectObject(hdc, fill);
    int p0[2] = {};
    int p1[2] = {};
    int p2[2] = {};
    project(gpu_->local_xyz()[i0 * 3], gpu_->local_xyz()[i0 * 3 + 1], gpu_->local_xyz()[i0 * 3 + 2],
            width_px, height_px, &p0[0], &p0[1]);
    project(gpu_->local_xyz()[i1 * 3], gpu_->local_xyz()[i1 * 3 + 1], gpu_->local_xyz()[i1 * 3 + 2],
            width_px, height_px, &p1[0], &p1[1]);
    project(gpu_->local_xyz()[i2 * 3], gpu_->local_xyz()[i2 * 3 + 1], gpu_->local_xyz()[i2 * 3 + 2],
            width_px, height_px, &p2[0], &p2[1]);
    const POINT pts[3] = {{p0[0], p0[1]}, {p1[0], p1[1]}, {p2[0], p2[1]}};
    Polygon(hdc, pts, 3);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
    DeleteObject(fill);
  }
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(mesh_pen);

  // Leftover-style place-names: white text + thick black outline.
  if (gpu_->look_preset() == Scene3dLookPreset::kLegacyStereo) {
    HFONT font = CreateFontW(-16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
                             DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
                             CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
    HGDIOBJ old_font = SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
    SetBkMode(hdc, TRANSPARENT);
    for (const Scene3dLegacyLabel& lab : gpu_->legacy_labels()) {
      int sx = 0;
      int sy = 0;
      project_lon_lat(lab.lon, lab.lat, width_px, height_px, &sx, &sy);
      if (sx < -40 || sy < -20 || sx > width_px + 40 || sy > height_px + 20) {
        continue;
      }
      wchar_t wbuf[64] = {};
      MultiByteToWideChar(CP_UTF8, 0, lab.text.c_str(), -1, wbuf, 63);
      const int len = static_cast<int>(wcslen(wbuf));
      SetTextColor(hdc, RGB(0, 0, 0));
      for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
          if (dx == 0 && dy == 0) {
            continue;
          }
          TextOutW(hdc, sx + dx, sy + dy, wbuf, len);
        }
      }
      SetTextColor(hdc, RGB(255, 255, 255));
      TextOutW(hdc, sx, sy, wbuf, len);
    }
    SelectObject(hdc, old_font);
    if (font) {
      DeleteObject(font);
    }
  }

  if (scene_) {
    ViewFrame fitted;
    const ViewFrame* labels_frame = label_frame_;
    if (!labels_frame) {
      fitted.fit_extent(*scene_, width_px, height_px);
      labels_frame = &fitted;
    }
    Map2dPresenter labels;
    labels.bind(scene_, labels_frame);
    labels.paint_labels_projected(
        hdc, width_px, height_px,
        [this, width_px, height_px](double lon, double lat, int* sx, int* sy) {
          project_lon_lat(lon, lat, width_px, height_px, sx, sy);
        });
  }

  paint_hud(hdc, width_px, height_px);
}

}  // namespace content
