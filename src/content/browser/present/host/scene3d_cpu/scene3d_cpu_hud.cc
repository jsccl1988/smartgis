// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/hdc/scene3d_hdc_hud.h"

#include "content/browser/camera/orbit_frame.h"
#include "content/browser/present/host/gdi/gdi_primitives.h"
#include "content/browser/present/scene3d/atmosphere/atmosphere_session.h"
#include "content/browser/present/scene3d/gpu/scene3d_gpu_present.h"
#include "content/browser/present/scene3d/hdc/scene3d_hdc_logo.h"
#include "vista/component/world/atmosphere/field/field_channel.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <mutex>
#include <string>
#include <vector>

namespace content {
namespace detail {
namespace {

void project_lon_lat(const OrbitFrame* orbit, double lon, double lat,
                     int width_px, int height_px, int* sx, int* sy) {
  if (!orbit) {
    if (sx) {
      *sx = 0;
    }
    if (sy) {
      *sy = 0;
    }
    return;
  }
  orbit->project_lon_lat(lon, lat, width_px, height_px, sx, sy);
}

HFONT ensure_place_label_font(HFONT* font_slot) {
  if (!font_slot) {
    return nullptr;
  }
  if (*font_slot) {
    return *font_slot;
  }
  *font_slot = CreateFontW(
      -16, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
      OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
      DEFAULT_PITCH | FF_SWISS, L"Microsoft YaHei");
  return *font_slot;
}

}  // namespace

void paint_soft_wind_arrows(HDC hdc, int width_px, int height_px,
                            AtmosphereSession* atmosphere,
                            Scene3dGpuPresent* gpu, const OrbitFrame* orbit) {
  if (!hdc || !atmosphere || !gpu || !atmosphere->wind_overlay_enabled() ||
      !atmosphere->environment() || width_px <= 0 || height_px <= 0) {
    return;
  }
  const vista::atmosphere::FieldStore& store =
      atmosphere->environment()->field_store();
  if (store.layer_count() == 0) {
    return;
  }
  const content::Extent2 e = gpu->world_extent();
  const double lon_span = e.xmax - e.xmin;
  const double lat_span = e.ymax - e.ymin;
  if (!(lon_span > 0.0) || !(lat_span > 0.0)) {
    return;
  }

  constexpr int kGrid = 10;
  const double t = atmosphere->time_sec();
  ScopedGdiPen pen(hdc, RGB(120, 200, 255), 1);
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
      project_lon_lat(orbit, lon, lat, width_px, height_px, &sx, &sy);
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
      gdi_stroke_line(hdc, sx, sy, ex, ey);
      // Simple arrowhead.
      const float hx = -dx * 0.25f;
      const float hy = -dy * 0.25f;
      const float px = -hy * 0.6f;
      const float py = hx * 0.6f;
      gdi_stroke_line(hdc, ex, ey,
                      ex + static_cast<int>(std::lround(hx + px)),
                      ey + static_cast<int>(std::lround(hy + py)));
      gdi_stroke_line(hdc, ex, ey,
                      ex + static_cast<int>(std::lround(hx - px)),
                      ey + static_cast<int>(std::lround(hy - py)));
    }
  }
}

void paint_soft_legacy_place_labels(HDC hdc, int width_px, int height_px,
                                    Scene3dGpuPresent* gpu,
                                    const OrbitFrame* orbit,
                                    HFONT* font_slot) {
  if (!hdc || !gpu || width_px <= 0 || height_px <= 0) {
    return;
  }
  // Snapshot under present_mu_ so set_look_preset swap-drop cannot destroy
  // strings mid-iteration (Debug basic_string dtor AV).
  std::vector<Scene3dLegacyLabel> labels;
  {
    std::lock_guard<std::recursive_mutex> lock(gpu->mutex());
    if (gpu->look_preset() != Scene3dLookPreset::kLegacyStereo ||
        gpu->legacy_labels().empty()) {
      return;
    }
    labels = gpu->legacy_labels();
  }
  HFONT font = ensure_place_label_font(font_slot);
  ScopedGdiSelect font_sel(hdc, font ? font : GetStockObject(DEFAULT_GUI_FONT));
  for (const Scene3dLegacyLabel& lab : labels) {
    int sx = 0;
    int sy = 0;
    project_lon_lat(orbit, lab.lon, lab.lat, width_px, height_px, &sx, &sy);
    if (sx < -40 || sy < -20 || sx > width_px + 40 || sy > height_px + 20) {
      continue;
    }
    wchar_t wbuf[64] = {};
    MultiByteToWideChar(CP_UTF8, 0, lab.text.c_str(), -1, wbuf, 63);
    const int len = static_cast<int>(wcslen(wbuf));
    gdi_draw_halo_text_w(hdc, sx, sy, wbuf, len, RGB(255, 255, 255),
                         RGB(0, 0, 0), 2);
  }
}

void paint_soft_hud_status(HDC hdc, int width_px, int height_px,
                           Scene3dGpuPresent* gpu,
                           AtmosphereSession* atmosphere,
                           bool hosts_shared_scene) {
  if (!hdc || !gpu || width_px <= 0 || height_px <= 0) {
    return;
  }

  // Compass rose: needle points to geographic north on screen (default orbit
  // frames north toward the top of the viewport).
  {
    const int cx = width_px - 56;
    const int cy = 56;
    const int r = 28;
    {
      ScopedGdiPen ring(hdc, RGB(210, 225, 240), 2);
      ScopedGdiSelect null_brush(hdc, GetStockObject(NULL_BRUSH));
      gdi_draw_ellipse(hdc, cx - r, cy - r, cx + r, cy + r);
    }
    const float angle = gpu->yaw() - kScene3dDefaultYaw;
    const float nx = std::sin(angle);
    const float ny = -std::cos(angle);
    const int tip_x = cx + static_cast<int>(std::lround(nx * (r - 6)));
    const int tip_y = cy + static_cast<int>(std::lround(ny * (r - 6)));
    {
      ScopedGdiPen needle(hdc, RGB(220, 60, 50), 2);
      gdi_stroke_line(hdc, cx, cy, tip_x, tip_y);
    }
    gdi_draw_text_w(hdc, cx - 5, cy - r - 18, L"N", 1, RGB(235, 245, 255));
  }

  wchar_t line[220];
  swprintf_s(line,
             L"Shared scene  yaw=%.2f pitch=%.2f dist=%.2f  "
             L"(wheel@cursor, pan, pinch)",
             gpu->yaw(), gpu->pitch(), gpu->distance());
  gdi_draw_text_w(hdc, 12, 12, line, lstrlenW(line), RGB(220, 235, 250));
  const wchar_t* so_t = hosts_shared_scene ? L"Orbit DEM (product Scene3D)"
                                           : L"Local DEM (product Scene3D)";
  gdi_draw_text_w(hdc, 12, 32, so_t, lstrlenW(so_t), RGB(220, 235, 250));

  wchar_t eng[140];
  const char* name =
      gpu->render_engine_name ? gpu->render_engine_name : "unknown";
  swprintf_s(eng, L"Engine  %hs  Fps%.3f%s", name, gpu->last_fps,
             gpu->wireframe_enabled() ? L"  wireframe=on" : L"");
  gdi_draw_text_w(hdc, 12, 52, eng, lstrlenW(eng), RGB(220, 235, 250));

  const wchar_t* wasd = L"WASD  orbit / wheel zoom";
  gdi_draw_text_w(hdc, 12, height_px > 40 ? height_px - 28 : 72, wasd,
                  lstrlenW(wasd), RGB(220, 235, 250));
  if (atmosphere && atmosphere->environment()) {
    wchar_t atmo[160];
    swprintf_s(atmo, L"Atmosphere  t=%.1fs  ocean=%d cloud=%d wind=%d",
               atmosphere->time_sec(),
               atmosphere->environment()->ocean_enabled() ? 1 : 0,
               atmosphere->environment()->cloud_enabled() ? 1 : 0,
               atmosphere->wind_overlay_enabled() ? 1 : 0);
    gdi_draw_text_w(hdc, 12, 72, atmo, lstrlenW(atmo), RGB(220, 235, 250));
  }
}

void paint_soft_hud_engine_badge(HDC hdc, int width_px, int height_px,
                                 Scene3dGpuPresent* gpu, HWND* logo_hwnd) {
  if (!hdc || !gpu || width_px <= 0 || height_px <= 0) {
    return;
  }
  const char* name =
      gpu->render_engine_name ? gpu->render_engine_name : "unknown";
  // Bottom-right engine badge last so it stays readable over wireframe.
  // DXGI/GL flip surfaces ignore GDI on the present HWND — use a WS_CHILD
  // badge. Mem-DC BitBlt paths and soft GDI / ContentMapView bake into HDC.
  const HWND hwnd = WindowFromDC(hdc);
  const bool window_dc = hwnd != nullptr && GetObjectType(hdc) != OBJ_MEMDC;
  const bool swapchain_surface = std::strncmp(name, "FlyCube/", 8) == 0 ||
                                 std::strcmp(name, "Stereo/GL") == 0;
  if (window_dc && swapchain_surface) {
    sync_engine_logo_overlay(
        logo_hwnd, hwnd, width_px, height_px,
        gpu->engine_fps_label_[0] ? gpu->engine_fps_label_
                                  : gpu->render_engine_name);
    if (!logo_hwnd || !*logo_hwnd || !IsWindow(*logo_hwnd)) {
      paint_engine_logo_corner(hdc, width_px, height_px,
                               gpu->engine_fps_label_[0]
                                   ? gpu->engine_fps_label_
                                   : gpu->render_engine_name);
    }
  } else {
    hide_engine_logo_overlay(logo_hwnd);
    paint_engine_logo_corner(hdc, width_px, height_px,
                             gpu->engine_fps_label_[0]
                                 ? gpu->engine_fps_label_
                                 : gpu->render_engine_name);
  }
}

}  // namespace detail
}  // namespace content
