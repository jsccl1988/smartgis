// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "vista/pass/world/hud/hud_build.h"

#include "vista/component/world/atmosphere/field/field_channel.h"
#include "vista/component/world/atmosphere/field/field_store.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace vista {
namespace {

void push_label(HudFrame* frame, float x, float y, const char* utf8,
                uint8_t r, uint8_t g, uint8_t b, bool halo) {
  if (!frame || !utf8 || !utf8[0]) {
    return;
  }
  HudLabel lab;
  lab.text = utf8;
  lab.x = x;
  lab.y = y;
  lab.rgba[0] = r;
  lab.rgba[1] = g;
  lab.rgba[2] = b;
  lab.rgba[3] = 255;
  lab.halo = halo;
  frame->labels.push_back(std::move(lab));
}

}  // namespace

void hud_append_status_lines(HudFrame* frame, float yaw, float pitch,
                             float distance, float fps, const char* engine_name,
                             bool wireframe, bool hosts_shared_scene,
                             bool has_atmosphere, float atmosphere_time_sec,
                             bool ocean_on, bool cloud_on, bool wind_on) {
  if (!frame || frame->width_px == 0 || frame->height_px == 0) {
    return;
  }
  char line[256];
  std::snprintf(line, sizeof(line),
                "Shared scene  yaw=%.2f pitch=%.2f dist=%.2f  "
                "(wheel@cursor, pan, pinch)",
                yaw, pitch, distance);
  push_label(frame, 12.f, 12.f, line, 220, 235, 250, false);
  push_label(frame, 12.f, 32.f,
             hosts_shared_scene ? "Orbit DEM (product Scene3D)"
                                : "Local DEM (product Scene3D)",
             220, 235, 250, false);
  const char* name =
      (engine_name && engine_name[0]) ? engine_name : "unknown";
  std::snprintf(line, sizeof(line), "Engine  %s  Fps%.3f%s", name, fps,
                wireframe ? "  wireframe=on" : "");
  push_label(frame, 12.f, 52.f, line, 220, 235, 250, false);
  const float wasd_y =
      frame->height_px > 40 ? static_cast<float>(frame->height_px) - 28.f
                            : 72.f;
  push_label(frame, 12.f, wasd_y, "WASD  orbit / wheel zoom", 220, 235, 250,
             false);
  if (has_atmosphere) {
    std::snprintf(line, sizeof(line),
                  "Atmosphere  t=%.1fs  ocean=%d cloud=%d wind=%d",
                  atmosphere_time_sec, ocean_on ? 1 : 0, cloud_on ? 1 : 0,
                  wind_on ? 1 : 0);
    push_label(frame, 12.f, 72.f, line, 220, 235, 250, false);
  }
}

void hud_append_compass(HudFrame* frame, float yaw_rad, float default_yaw_rad) {
  if (!frame || frame->width_px < 80 || frame->height_px < 80) {
    return;
  }
  frame->compass.enabled = true;
  frame->compass.cx = static_cast<float>(frame->width_px) - 56.f;
  frame->compass.cy = 56.f;
  frame->compass.radius = 28.f;
  frame->compass.north_angle_rad = yaw_rad - default_yaw_rad;
  push_label(frame, frame->compass.cx - 5.f,
             frame->compass.cy - frame->compass.radius - 18.f, "N", 235, 245,
             255, false);
}

void hud_append_engine_badge(HudFrame* frame, const char* engine_fps_label) {
  if (!frame) {
    return;
  }
  const char* text =
      (engine_fps_label && engine_fps_label[0]) ? engine_fps_label : "unknown";
  frame->badge.enabled = true;
  frame->badge.text = text;
}

void hud_append_wind_arrows(HudFrame* frame,
                            const atmosphere::FieldStore* store,
                            double time_sec, double xmin, double ymin,
                            double xmax, double ymax, HudProjectFn project,
                            void* project_ctx) {
  if (!frame || !store || !project || store->layer_count() == 0) {
    return;
  }
  const double lon_span = xmax - xmin;
  const double lat_span = ymax - ymin;
  if (!(lon_span > 0.0) || !(lat_span > 0.0)) {
    return;
  }
  constexpr int kGrid = 10;
  const int w = static_cast<int>(frame->width_px);
  const int h = static_cast<int>(frame->height_px);
  for (int j = 0; j < kGrid; ++j) {
    for (int i = 0; i < kGrid; ++i) {
      const double lon =
          xmin + (static_cast<double>(i) + 0.5) / kGrid * lon_span;
      const double lat =
          ymin + (static_cast<double>(j) + 0.5) / kGrid * lat_span;
      const float u =
          store->sample(atmosphere::FieldChannel::kWindU, lon, lat, time_sec);
      const float v =
          store->sample(atmosphere::FieldChannel::kWindV, lon, lat, time_sec);
      const float speed = std::sqrt(u * u + v * v);
      if (!(speed > 1.0e-3f)) {
        continue;
      }
      float sx = 0.f;
      float sy = 0.f;
      if (!project(project_ctx, lon, lat, &sx, &sy)) {
        continue;
      }
      if (sx < -20.f || sy < -20.f || sx > static_cast<float>(w) + 20.f ||
          sy > static_cast<float>(h) + 20.f) {
        continue;
      }
      const float len = (std::min)(28.f, 6.f + speed * 1.2f);
      const float inv = 1.f / speed;
      const float dx = u * inv * len;
      const float dy = -v * inv * len;
      const float ex = sx + dx;
      const float ey = sy + dy;
      HudLine stem;
      stem.x0 = sx;
      stem.y0 = sy;
      stem.x1 = ex;
      stem.y1 = ey;
      stem.rgba[0] = 120;
      stem.rgba[1] = 200;
      stem.rgba[2] = 255;
      stem.rgba[3] = 255;
      frame->lines.push_back(stem);
      const float hx = -dx * 0.25f;
      const float hy = -dy * 0.25f;
      const float px = -hy * 0.6f;
      const float py = hx * 0.6f;
      HudLine a0;
      a0.x0 = ex;
      a0.y0 = ey;
      a0.x1 = ex + hx + px;
      a0.y1 = ey + hy + py;
      a0.rgba[0] = 120;
      a0.rgba[1] = 200;
      a0.rgba[2] = 255;
      a0.rgba[3] = 255;
      frame->lines.push_back(a0);
      HudLine a1 = a0;
      a1.x1 = ex + hx - px;
      a1.y1 = ey + hy - py;
      frame->lines.push_back(a1);
    }
  }
}

void hud_append_place_label(HudFrame* frame, const char* utf8, float sx,
                            float sy) {
  push_label(frame, sx, sy, utf8, 255, 255, 255, true);
}

}  // namespace vista
