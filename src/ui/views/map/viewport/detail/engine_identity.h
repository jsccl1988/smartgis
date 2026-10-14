// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_VIEWS_MAP_VIEWPORT_DETAIL_ENGINE_IDENTITY_H_
#define UI_VIEWS_MAP_VIEWPORT_DETAIL_ENGINE_IDENTITY_H_

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstring>

#include "base/process/switches.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/viewport/paint_policy.h"

namespace ui {
namespace views {
namespace detail {

// Machine id + human title for identity HUD / window title. String literals
// are byte-stable (harness engine-shot contract); do not rename casually.
struct EngineIdentity {
  const wchar_t* role_name = L"MapEdit";
  const wchar_t* engine_id = L"views-map2d";
  const wchar_t* engine_title = L"Map2D";
};

inline const wchar_t* role_display_name(DrawHost::Role role) {
  switch (role) {
    case DrawHost::Role::kMapData:
      return L"MapData";
    case DrawHost::Role::kScene3d:
      return L"Scene3d";
    case DrawHost::Role::kMapEdit:
    default:
      return L"MapEdit";
  }
}

inline EngineIdentity resolve_engine_identity(DrawHost::Role role,
                                              DrawHost::AttachMode mode) {
  EngineIdentity out;
  out.role_name = role_display_name(role);

  if (role == DrawHost::Role::kScene3d) {
#if defined(HAS_SCENE3D_ENGINE)
    if (content::prefer_scene3d_scenic()) {
      out.engine_id = L"views-scene3d-scenic";
      out.engine_title = L"Views Scene3D (Scenic)";
      return out;
    }
    if (content::prefer_scene3d_flycube() &&
        mode == DrawHost::AttachMode::kGpuPresent) {
      out.engine_id = L"views-scene3d-dx12";
      out.engine_title = L"Views Scene3D (GPU/DX12)";
      return out;
    }
    if (content::prefer_scene3d_stereo_gl()) {
      // Default leftover stereo is D3D11; OpenGL is opt-in.
      bool d3d = true;
      if (const char* api = base::switch_cstr("stereo-api")) {
        if (_stricmp(api, "OpenGL") == 0) {
          d3d = false;
        } else if (_stricmp(api, "Direct3D") == 0) {
          d3d = true;
        }
      }
      if (d3d) {
        out.engine_id = L"legacy-scene3d-d3d";
        out.engine_title = L"Legacy Scene3D (D3D11)";
      } else {
        out.engine_id = L"legacy-scene3d-gl";
        out.engine_title = L"Legacy Scene3D (OpenGL)";
      }
      return out;
    }
    if (content::prefer_scene3d_gdi()) {
      out.engine_id = L"views-scene3d-gdi";
      out.engine_title = L"Views Scene3D (GDI)";
      return out;
    }
#endif
    if (mode == DrawHost::AttachMode::kGpuPresent) {
      out.engine_id = L"views-scene3d-dx12";
      out.engine_title = L"Views Scene3D (GPU/DX12)";
    } else if (mode == DrawHost::AttachMode::kContentMapView) {
      out.engine_id = L"views-scene3d-content";
      out.engine_title = L"Views Scene3D (Content)";
    } else {
      out.engine_id = L"views-scene3d";
      out.engine_title = L"Scene3D";
    }
    return out;
  }

  if (prefer_map2d_scenic_engine()) {
    out.engine_id = L"views-map2d-scenic";
    out.engine_title = L"Views Map2D (Scenic)";
  } else if (mode == DrawHost::AttachMode::kGpuPresent) {
    // GpuPresent SoT is FlyCube/DX12 (same face as Scene3d), not Skia canvas.
    out.engine_id = L"views-map2d-dx12";
    out.engine_title = L"Views Map2D (GPU/DX12)";
  } else if (mode == DrawHost::AttachMode::kLocalDevice) {
    out.engine_id = L"legacy-map2d-gdi";
    out.engine_title = L"Legacy Map2D (GDI+)";
  } else if (mode == DrawHost::AttachMode::kContentMapView) {
    out.engine_id = L"views-map2d-content";
    out.engine_title = L"Views Map2D (Content)";
  } else if (mode == DrawHost::AttachMode::kOopRender) {
    out.engine_id = L"views-map2d-oop";
    out.engine_title = L"Views Map2D (OOP)";
  } else {
    out.engine_id = L"views-map2d";
    out.engine_title = L"Map2D";
  }
  return out;
}

}  // namespace detail
}  // namespace views
}  // namespace ui

#endif  // UI_VIEWS_MAP_VIEWPORT_DETAIL_ENGINE_IDENTITY_H_
