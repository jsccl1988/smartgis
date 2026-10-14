// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef VISTA_PASS_WORLD_HUD_HUD_BUILD_H_
#define VISTA_PASS_WORLD_HUD_HUD_BUILD_H_

#include <cstddef>
#include <cstdint>

#include "vista/pass/world/hud/hud_ir.h"
#include "vista/vista_export.h"

namespace vista {
namespace atmosphere {
class FieldStore;
}

// Append status / compass / badge / wind / place-name items into |frame|.
// Callers supply already-projected screen coords for place labels.
VISTA_EXPORT void hud_append_status_lines(
    HudFrame* frame, float yaw, float pitch, float distance, float fps,
    const char* engine_name, bool wireframe, bool hosts_shared_scene,
    bool has_atmosphere, float atmosphere_time_sec, bool ocean_on,
    bool cloud_on, bool wind_on);

VISTA_EXPORT void hud_append_compass(HudFrame* frame, float yaw_rad,
                                     float default_yaw_rad);

VISTA_EXPORT void hud_append_engine_badge(HudFrame* frame,
                                          const char* engine_fps_label);

// |project| returns false to skip a sample. lon/lat in degrees.
using HudProjectFn = bool (*)(void* ctx, double lon, double lat, float* sx,
                              float* sy);

VISTA_EXPORT void hud_append_wind_arrows(
    HudFrame* frame, const atmosphere::FieldStore* store, double time_sec,
    double xmin, double ymin, double xmax, double ymax, HudProjectFn project,
    void* project_ctx);

VISTA_EXPORT void hud_append_place_label(HudFrame* frame, const char* utf8,
                                         float sx, float sy);

}  // namespace vista

#endif  // VISTA_PASS_WORLD_HUD_HUD_BUILD_H_
