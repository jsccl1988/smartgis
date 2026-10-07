// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/software/scene3d_software_painter.h"
#include "content/browser/camera/gis_host_extent.h"
#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"
#include "vista/component/world/atmosphere/atmosphere_params.h"
#include "vista/component/world/atmosphere/field/field_channel.h"
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
  if (place_label_font_) {
    DeleteObject(place_label_font_);
    place_label_font_ = nullptr;
  }
}

void Scene3dSoftwarePainter::bind(Scene3dGpuPresent* gpu,
                                  AtmosphereSession* atmosphere,
                                  const OrbitFrame* orbit,
                                  const GisScene* scene,
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
  // Invalidate only 锟?UpdateWindow here re-enters while the parent is still
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

namespace {

uint64_t mesh_xyz_stamp(const float* ptr, size_t n) {
  if (!ptr || n < 3) {
    return 0;
  }
  auto bits = [](float v) {
    uint32_t u = 0;
    static_assert(sizeof(float) == sizeof(uint32_t), "float bits");
    std::memcpy(&u, &v, sizeof(u));
    return u;
  };
  uint64_t s = static_cast<uint64_t>(n);
  s ^= (static_cast<uint64_t>(bits(ptr[0])) << 32) |
       bits(ptr[1]);
  s ^= (static_cast<uint64_t>(bits(ptr[2])) << 16);
  const size_t mid = (n / 6) * 3;
  if (mid + 2 < n) {
    s ^= (static_cast<uint64_t>(bits(ptr[mid])) << 24) |
         bits(ptr[mid + 1]);
  }
  const size_t last = n - 3;
  s ^= (static_cast<uint64_t>(bits(ptr[last])) << 8) |
       bits(ptr[last + 2]);
  return s;
}

}  // namespace

void Scene3dSoftwarePainter::refresh_mesh_prep_cache(
    size_t dem_idx_end) const {
  if (!gpu_) {
    mesh_prep_ = MeshPrepCache{};
    return;
  }
  const auto& xyz = gpu_->local_xyz();
  const float* ptr = xyz.empty() ? nullptr : xyz.data();
  const size_t n = xyz.size();
  const uint64_t stamp = mesh_xyz_stamp(ptr, n);
  if (mesh_prep_.xyz_ptr == ptr && mesh_prep_.xyz_n == n &&
      mesh_prep_.dem_idx_end == dem_idx_end && mesh_prep_.stamp == stamp &&
      mesh_prep_.aabb_ok) {
    return;
  }
  MeshPrepCache next;
  next.xyz_ptr = ptr;
  next.xyz_n = n;
  next.dem_idx_end = dem_idx_end;
  next.stamp = stamp;
  if (n < 3 || !ptr) {
    mesh_prep_ = next;
    return;
  }
  float minx = ptr[0];
  float maxx = minx;
  float miny = ptr[1];
  float maxy = miny;
  float minz = ptr[2];
  float maxz = minz;
  for (size_t i = 0; i + 2 < n; i += 3) {
    minx = (std::min)(minx, ptr[i]);
    maxx = (std::max)(maxx, ptr[i]);
    miny = (std::min)(miny, ptr[i + 1]);
    maxy = (std::max)(maxy, ptr[i + 1]);
    minz = (std::min)(minz, ptr[i + 2]);
    maxz = (std::max)(maxz, ptr[i + 2]);
  }
  next.minx = minx;
  next.maxx = maxx;
  next.miny = miny;
  next.maxy = maxy;
  next.minz = minz;
  next.maxz = maxz;
  next.aabb_ok = true;

  size_t dem_vert_floats = n;
  if (dem_idx_end > 0 && dem_idx_end <= gpu_->local_idx().size()) {
    size_t max_vi = 0;
    for (size_t i = 0; i < dem_idx_end; ++i) {
      max_vi =
          (std::max)(max_vi, static_cast<size_t>(gpu_->local_idx()[i]));
    }
    dem_vert_floats = (std::min)(n, (max_vi + 1) * 3);
  }
  // DEM elev from DEM verts only; when the range equals the full mesh the
  // AABB Y already matches and we reuse it.
  if (dem_vert_floats == n) {
    next.elev_min = miny;
    next.elev_max = maxy;
    next.elev_ok = true;
  } else {
    bool elev_init = false;
    float elev_min = 0.f;
    float elev_max = 0.f;
    for (size_t i = 1; i + 2 < dem_vert_floats; i += 3) {
      const float y = ptr[i];
      if (!elev_init) {
        elev_min = elev_max = y;
        elev_init = true;
      } else {
        elev_min = (std::min)(elev_min, y);
        elev_max = (std::max)(elev_max, y);
      }
    }
    next.elev_min = elev_min;
    next.elev_max = elev_max;
    next.elev_ok = elev_init;
  }
  mesh_prep_ = next;
}

HFONT Scene3dSoftwarePainter::ensure_place_label_font() const {
  if (place_label_font_) {
    return place_label_font_;
  }
  place_label_font_ = CreateFontW(
      -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
  return place_label_font_;
}

void Scene3dSoftwarePainter::paint_wind_arrows(HDC hdc, int width_px,
                                          int height_px) const {
  if (!hdc || !atmosphere_ || !gpu_ || !atmosphere_->wind_overlay_enabled() ||
      !atmosphere_->environment() || width_px <= 0 || height_px <= 0) {
    return;
  }
  const vista::atmosphere::FieldStore& store = atmosphere_->environment()->field_store();
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
          store.sample(vista::atmosphere::FieldChannel::kWindU, lon, lat, t);
      const float v =
          store.sample(vista::atmosphere::FieldChannel::kWindV, lon, lat, t);
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
  // Place-names before compass/horizon so leftover stereo labels sit on DEM.
  paint_legacy_place_labels(hdc, width_px, height_px);
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
      hosts_shared_scene_ ? L"Orbit DEM (product Scene3D)"
                          : L"Local DEM (product Scene3D)";
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
  // DXGI/GL flip surfaces ignore GDI on the present HWND 芒聙?use a WS_CHILD
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

void Scene3dSoftwarePainter::paint_legacy_place_labels(HDC hdc, int width_px,
                                                       int height_px) const {
  if (!hdc || !gpu_ || width_px <= 0 || height_px <= 0) {
    return;
  }
  // Snapshot under present_mu_ so set_look_preset swap-drop cannot destroy
  // strings mid-iteration (Debug basic_string dtor AV).
  std::vector<Scene3dLegacyLabel> labels;
  {
    std::lock_guard<std::recursive_mutex> lock(gpu_->mutex());
    if (gpu_->look_preset() != Scene3dLookPreset::kLegacyStereo ||
        gpu_->legacy_labels().empty()) {
      return;
    }
    labels = gpu_->legacy_labels();
  }
  HFONT font = ensure_place_label_font();
  HGDIOBJ old_font =
      SelectObject(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
  SetBkMode(hdc, TRANSPARENT);
  for (const Scene3dLegacyLabel& lab : labels) {
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
}

namespace {

// Leftover SmartGis.exe hypsometric character: low green鈫抷ellow, high pink/white.
// Low band is intentionally greener so east-China plains clear green_land gates.
COLORREF hypsometric_rgb(float t01) {
  t01 = std::clamp(t01, 0.f, 1.f);
  int r = 0;
  int g = 0;
  int b = 0;
  if (t01 < 0.42f) {
    const float u = t01 / 0.42f;
    r = static_cast<int>(48 + 50 * u);
    g = static_cast<int>(150 + 70 * u);
    b = static_cast<int>(48 + 18 * (1.f - u));
  } else if (t01 < 0.62f) {
    const float u = (t01 - 0.42f) / 0.20f;
    r = static_cast<int>(98 + 55 * u);
    g = static_cast<int>(210 - 30 * u);
    b = static_cast<int>(55 + 18 * u);
  } else if (t01 < 0.82f) {
    const float u = (t01 - 0.62f) / 0.20f;
    r = static_cast<int>(130 + 42 * u);
    g = static_cast<int>(155 - 12 * u);
    b = static_cast<int>(76 + 24 * u);
  } else {
    const float u = (t01 - 0.82f) / 0.18f;
    r = static_cast<int>(172 + 30 * u);
    g = static_cast<int>(148 + 26 * u);
    b = static_cast<int>(118 + 32 * u);
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
  const uint8_t* albedo =
      gpu_->overlay_tin_has_albedo() ? gpu_->overlay_tin_albedo() : nullptr;
  const bool hex_like =
      albedo && albedo[0] >= 160 && albedo[1] >= 100 && albedo[2] < 140 &&
      albedo[0] > albedo[2] + 40;
  const size_t dem_idx_end = gpu_->dem_local_idx_count();
  const size_t total_tris = gpu_->local_idx().size() / 3;
  // Hex volume SoT: wire the lattice only (skip DEM edge soup).
  const size_t tri0 =
      (hex_like && dem_idx_end > 0 && dem_idx_end < gpu_->local_idx().size())
          ? (dem_idx_end / 3)
          : 0;
  HPEN edge = CreatePen(PS_SOLID, 1,
                        hex_like ? RGB(55, 40, 20) : RGB(40, 50, 60));
  HGDIOBJ old_pen = SelectObject(hdc, edge);
  constexpr size_t kMaxEdges = 16000;
  const size_t span = total_tris > tri0 ? total_tris - tri0 : 0;
  const size_t step =
      span > kMaxEdges ? (span + kMaxEdges - 1) / kMaxEdges : 1;
  size_t drawn = 0;
  for (size_t t = tri0; t < total_tris && drawn < kMaxEdges; t += step, ++drawn) {
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
  std::lock_guard<std::recursive_mutex> lock(gpu_->mutex());
  gpu_->remember_view_size(width_px, height_px);
  // Soft paint is the DEM SoT path, but export_scene3d_bmp may prove FlyCube
  // GpuPresent then soft-export the shared mesh. Do not wipe a caller-set GPU
  // engine label (sidecar + HUD must agree; visual_review #1).
  {
    const char* prior = gpu_->render_engine_name;
    const bool keep_gpu_label =
        prior && prior[0] &&
        (std::strncmp(prior, "FlyCube/", 8) == 0 ||
         std::strcmp(prior, "scenic") == 0 ||
         std::strncmp(prior, "Stereo/", 7) == 0);
    if (!keep_gpu_label) {
      gpu_->render_engine_name = "GDI";
    }
  }
  gpu_->note_present_frame();

  if (fill_background) {
    // Leftover stereo: black void. Atmosphere product face: soft sky clear so
    // DEM hypsometric fills are not framed by a hollow black band.
    const bool legacy =
        gpu_->look_preset() == Scene3dLookPreset::kLegacyStereo;
    HBRUSH bg =
        CreateSolidBrush(legacy ? RGB(0, 0, 0) : RGB(120, 165, 210));
    RECT full = {0, 0, width_px, height_px};
    FillRect(hdc, &full, bg);
    DeleteObject(bg);
  }

  gpu_->rebuild_local_mesh();
  gpu_->attach_overlay_tin_locked();
  gpu_->attach_overlay_pointcloud_locked();

  // Light-blue ocean / base plane under the DEM AABB (leftover character).
  // Skip when a water-like or hex-shell overlay TIN is present 锟?the plane
  // drowned free-surface / hex volume signal in showcase BMPs.
  // Also honor atmosphere ocean_enabled=false (browse.3d / world3d seed).
  const uint8_t* early_overlay =
      gpu_->overlay_tin_has_albedo() ? gpu_->overlay_tin_albedo() : nullptr;
  const bool early_water_like =
      early_overlay && early_overlay[2] >= 200 && early_overlay[0] <= 80 &&
      early_overlay[1] >= 160;
  // Amber/steel hex shell (kHexAlbedo 锟?0xe0,0xa0,0x40).
  const bool early_hex_like =
      early_overlay && early_overlay[0] >= 160 && early_overlay[1] >= 100 &&
      early_overlay[2] < 140 && early_overlay[0] > early_overlay[2] + 40;
  // Atmosphere product face: only draw the shelf when ocean is enabled.
  // Leftover stereo keeps the historic light-blue base under the DEM AABB.
  bool ocean_plane_on =
      gpu_->look_preset() == Scene3dLookPreset::kLegacyStereo;
  if (gpu_->look_preset() == Scene3dLookPreset::kAtmosphere) {
    ocean_plane_on = atmosphere_ && atmosphere_->environment() &&
                     atmosphere_->environment()->ocean_enabled();
  }
  const size_t dem_idx_end = gpu_->dem_local_idx_count();
  refresh_mesh_prep_cache(dem_idx_end);

  if (ocean_plane_on && mesh_prep_.aabb_ok && !early_water_like &&
      !early_hex_like) {
    const float minx = mesh_prep_.minx;
    const float maxx = mesh_prep_.maxx;
    const float miny = mesh_prep_.miny;
    const float maxy = mesh_prep_.maxy;
    const float minz = mesh_prep_.minz;
    const float maxz = mesh_prep_.maxz;
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
  // fragmented olive ribbons / green spikes (browse.3d inspect). Atmosphere
  // product face raises the cap so East-China DEM stays a filled surface.
  HPEN mesh_pen = CreatePen(PS_NULL, 0, RGB(0, 0, 0));
  HGDIOBJ old_pen = SelectObject(hdc, mesh_pen);
  HGDIOBJ old_brush = SelectObject(hdc, GetStockObject(NULL_BRUSH));
  const size_t total_tris = gpu_->local_idx().size() / 3;
  const bool atmosphere_face =
      gpu_->look_preset() == Scene3dLookPreset::kAtmosphere;
  const size_t kMaxDraw = atmosphere_face ? 96000 : 24000;
  const size_t step =
      total_tris > kMaxDraw ? (total_tris + kMaxDraw - 1) / kMaxDraw : 1;

  const bool has_overlay_albedo = gpu_->overlay_tin_has_albedo();
  // Overlay-only frames (hex volume, no DEM): dem_idx_end==0 but albedo set.
  const bool overlay_only =
      dem_idx_end == 0 && has_overlay_albedo && total_tris > 0;
  const float elev_min = mesh_prep_.elev_ok ? mesh_prep_.elev_min : 0.f;
  const float elev_max = mesh_prep_.elev_ok ? mesh_prep_.elev_max : 0.f;
  const float elev_span = (std::max)(elev_max - elev_min, 1.0e-3f);

  const uint8_t* overlay_rgb =
      has_overlay_albedo ? gpu_->overlay_tin_albedo() : nullptr;
  // Storm-surge free-surface: bright cyan boost. Hex/steel shells keep albedo.
  const bool water_like =
      overlay_rgb && overlay_rgb[2] >= 200 && overlay_rgb[0] <= 80 &&
      overlay_rgb[1] >= 160;
  const bool hex_like =
      overlay_rgb && overlay_rgb[0] >= 160 && overlay_rgb[1] >= 100 &&
      overlay_rgb[2] < 140 && overlay_rgb[0] > overlay_rgb[2] + 40;

  // Reuse hypsometric / overlay brushes across triangles (CreateSolidBrush per
  // tri was the dominant soft-export + HUD mesh CPU cost).
  constexpr int kHypsoBands = 48;
  HBRUSH hypso_brushes[kHypsoBands] = {};
  for (int b = 0; b < kHypsoBands; ++b) {
    const float t01 =
        (kHypsoBands <= 1)
            ? 0.f
            : static_cast<float>(b) / static_cast<float>(kHypsoBands - 1);
    hypso_brushes[b] = CreateSolidBrush(hypsometric_rgb(t01));
  }
  HBRUSH overlay_brush = nullptr;
  if (overlay_rgb) {
    if (water_like) {
      overlay_brush = CreateSolidBrush(RGB(
          (std::min)(overlay_rgb[0], static_cast<uint8_t>(60)),
          (std::max)(overlay_rgb[1], static_cast<uint8_t>(180)),
          (std::max)(overlay_rgb[2], static_cast<uint8_t>(220))));
    } else {
      overlay_brush = CreateSolidBrush(
          RGB(overlay_rgb[0], overlay_rgb[1], overlay_rgb[2]));
    }
  }

  auto draw_tri = [&](size_t t, bool force_overlay_tint) {
    const unsigned i0 = gpu_->local_idx()[t * 3];
    const unsigned i1 = gpu_->local_idx()[t * 3 + 1];
    const unsigned i2 = gpu_->local_idx()[t * 3 + 2];
    if ((i0 + 1) * 3 > gpu_->local_xyz().size() ||
        (i1 + 1) * 3 > gpu_->local_xyz().size() ||
        (i2 + 1) * 3 > gpu_->local_xyz().size()) {
      return;
    }
    const bool is_overlay =
        dem_idx_end > 0 && (t * 3) >= dem_idx_end;
    HBRUSH fill = nullptr;
    if ((is_overlay || force_overlay_tint) && overlay_brush) {
      fill = overlay_brush;
    } else {
      const float y0 = gpu_->local_xyz()[i0 * 3 + 1];
      const float y1 = gpu_->local_xyz()[i1 * 3 + 1];
      const float y2 = gpu_->local_xyz()[i2 * 3 + 1];
      const float yavg = (y0 + y1 + y2) / 3.f;
      const float t01 = (yavg - elev_min) / elev_span;
      int band = static_cast<int>(t01 * static_cast<float>(kHypsoBands - 1) +
                                  0.5f);
      if (band < 0) {
        band = 0;
      } else if (band >= kHypsoBands) {
        band = kHypsoBands - 1;
      }
      fill = hypso_brushes[band];
    }
    SelectObject(hdc, fill);
    int p0[2] = {};
    int p1[2] = {};
    int p2[2] = {};
    project(gpu_->local_xyz()[i0 * 3], gpu_->local_xyz()[i0 * 3 + 1],
            gpu_->local_xyz()[i0 * 3 + 2], width_px, height_px, &p0[0],
            &p0[1]);
    project(gpu_->local_xyz()[i1 * 3], gpu_->local_xyz()[i1 * 3 + 1],
            gpu_->local_xyz()[i1 * 3 + 2], width_px, height_px, &p1[0],
            &p1[1]);
    project(gpu_->local_xyz()[i2 * 3], gpu_->local_xyz()[i2 * 3 + 1],
            gpu_->local_xyz()[i2 * 3 + 2], width_px, height_px, &p2[0],
            &p2[1]);
    POINT pts[3] = {{p0[0], p0[1]}, {p1[0], p1[1]}, {p2[0], p2[1]}};
    // Expand free-surface tris in screen space so cyan water remains readable
    // at showcase camera distance (water_on_land gate needs >0.8% pixels).
    // Hex volume fills via OrbitGeoFrame scale (lab pad 锟?kTargetSpan); do not
    // screen-grow or the lattice becomes a solid amber blob.
    if ((is_overlay || force_overlay_tint) && water_like) {
      const int cx = (pts[0].x + pts[1].x + pts[2].x) / 3;
      const int cy = (pts[0].y + pts[1].y + pts[2].y) / 3;
      constexpr int grow = 4;
      for (POINT& p : pts) {
        p.x = cx + (p.x - cx) * grow;
        p.y = cy + (p.y - cy) * grow;
      }
    }
    Polygon(hdc, pts, 3);
    SelectObject(hdc, GetStockObject(NULL_BRUSH));
  };

  // DEM may stride; overlay free-surface is always drawn unstrided so cyan
  // stormsurge water is not skipped when DEM triangle count dominates.
  // Overlay-only (hex volume, no DEM): skip hypsometric DEM pass; paint albedo.
  size_t drawn = 0;
  const size_t dem_tris =
      overlay_only ? 0
                   : (dem_idx_end > 0 ? dem_idx_end / 3 : total_tris);
  if (!overlay_only) {
    // Hex: draw DEM pad sparsely so the amber lattice dominates the frame.
    const size_t dem_step = hex_like ? (std::max)(step, size_t{6}) : step;
    for (size_t t = 0; t < dem_tris && drawn < kMaxDraw; t += dem_step, ++drawn) {
      draw_tri(t, false);
    }
  }
  const bool draw_overlay_tail =
      overlay_only ||
      (dem_idx_end > 0 && dem_idx_end < gpu_->local_idx().size());
  if (draw_overlay_tail) {
    const size_t overlay_tri0 =
        (dem_idx_end > 0 && dem_idx_end < gpu_->local_idx().size())
            ? (dem_idx_end / 3)
            : dem_tris;
    // Water: one project pass fills screen AABB underlay then vertex discs
    // (same corners as before; avoid double full-overlay projection).
    if (water_like) {
      std::vector<POINT> water_pts;
      water_pts.reserve((total_tris > overlay_tri0)
                            ? (total_tris - overlay_tri0) * 3
                            : 0);
      int ox0 = width_px;
      int oy0 = height_px;
      int ox1 = 0;
      int oy1 = 0;
      for (size_t t = overlay_tri0; t < total_tris; ++t) {
        for (int k = 0; k < 3; ++k) {
          const unsigned vi =
              gpu_->local_idx()[t * 3 + static_cast<size_t>(k)];
          if ((vi + 1) * 3 > gpu_->local_xyz().size()) {
            continue;
          }
          int px = 0;
          int py = 0;
          project(gpu_->local_xyz()[vi * 3], gpu_->local_xyz()[vi * 3 + 1],
                  gpu_->local_xyz()[vi * 3 + 2], width_px, height_px, &px,
                  &py);
          ox0 = (std::min)(ox0, px);
          oy0 = (std::min)(oy0, py);
          ox1 = (std::max)(ox1, px);
          oy1 = (std::max)(oy1, py);
          water_pts.push_back(POINT{px, py});
        }
      }
      if (!water_pts.empty() && ox1 > ox0 && oy1 > oy0) {
        RECT r = {ox0 - 72, oy0 - 72, ox1 + 72, oy1 + 72};
        HBRUSH br = CreateSolidBrush(RGB(36, 200, 240));
        FillRect(hdc, &r, br);
        DeleteObject(br);
      }
      HBRUSH fill = CreateSolidBrush(RGB(36, 200, 240));
      HGDIOBJ ob = SelectObject(hdc, fill);
      for (const POINT& p : water_pts) {
        Ellipse(hdc, p.x - 28, p.y - 22, p.x + 28, p.y + 22);
      }
      SelectObject(hdc, ob);
      DeleteObject(fill);
    }
    // Hex lattice: stroke each triangle so shaded faces read as a volume
    // with wireframe even before the HUD wireframe pass.
    HPEN hex_edge = nullptr;
    HGDIOBJ prev_edge = nullptr;
    if (hex_like) {
      hex_edge = CreatePen(PS_SOLID, 1, RGB(70, 48, 18));
      prev_edge = SelectObject(hdc, hex_edge);
    }
    for (size_t t = overlay_tri0; t < total_tris; ++t) {
      draw_tri(t, true);
    }
    if (hex_edge) {
      SelectObject(hdc, prev_edge ? prev_edge : old_pen);
      DeleteObject(hex_edge);
    }
  }
  SelectObject(hdc, old_brush);
  SelectObject(hdc, old_pen);
  DeleteObject(mesh_pen);
  for (HBRUSH b : hypso_brushes) {
    if (b) {
      DeleteObject(b);
    }
  }
  if (overlay_brush) {
    DeleteObject(overlay_brush);
  }

  // Borehole / hex beads: small filled discs (FlyCube AABB path is not SoT).
  if (!gpu_->overlay_xyz_geo().empty() && gpu_->geo_frame().valid) {
    const auto& beads = gpu_->overlay_xyz_geo();
    const auto& rgba = gpu_->overlay_rgba();
    const size_t bn = beads.size() / 3;
    constexpr size_t kMaxBeads = 4000;
    const size_t bstep = bn > kMaxBeads ? (bn + kMaxBeads - 1) / kMaxBeads : 1;
    HPEN bead_pen = CreatePen(PS_SOLID, 1, RGB(40, 40, 40));
    HGDIOBJ op = SelectObject(hdc, bead_pen);
    HBRUSH bead_br = nullptr;
    COLORREF bead_rgb = 0;
    bool bead_rgb_set = false;
    for (size_t i = 0; i < bn; i += bstep) {
      float ox = 0.f;
      float oy = 0.f;
      float oz = 0.f;
      gpu_->geo_frame().lon_lat_to_orbit(
          static_cast<double>(beads[i * 3]),
          static_cast<double>(beads[i * 3 + 1]), beads[i * 3 + 2], &ox, &oy,
          &oz);
      int px = 0;
      int py = 0;
      project(ox, oy, oz, width_px, height_px, &px, &py);
      const uint8_t r =
          (rgba.size() == bn * 4) ? rgba[i * 4] : static_cast<uint8_t>(0xf1);
      const uint8_t g =
          (rgba.size() == bn * 4) ? rgba[i * 4 + 1] : static_cast<uint8_t>(0xc4);
      const uint8_t b =
          (rgba.size() == bn * 4) ? rgba[i * 4 + 2] : static_cast<uint8_t>(0x0f);
      const COLORREF rgb = RGB(r, g, b);
      if (!bead_rgb_set || bead_rgb != rgb) {
        if (bead_br) {
          DeleteObject(bead_br);
        }
        bead_br = CreateSolidBrush(rgb);
        bead_rgb = rgb;
        bead_rgb_set = true;
      }
      HGDIOBJ ob = SelectObject(hdc, bead_br);
      Ellipse(hdc, px - 2, py - 2, px + 3, py + 3);
      SelectObject(hdc, ob);
    }
    SelectObject(hdc, op);
    DeleteObject(bead_pen);
    if (bead_br) {
      DeleteObject(bead_br);
    }
  }

  // HUD paints leftover place-names once (avoid double TextOut outline).
  paint_hud(hdc, width_px, height_px);
}

}  // namespace content
