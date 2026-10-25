// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/map2d/gpu/damage/map2d_damage_rect.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "content/browser/camera/view_frame.h"
#include "content/browser/document/gis_scene.h"

namespace content {
namespace {

using Reasons = Map2dFrameCache::PresentDamageReasons;

constexpr int32_t kSelectionPadPx = 8;

bool reasons_allow_local_scissor(uint32_t reasons) {
  if (reasons == Reasons::kNone) {
    return false;
  }
  constexpr uint32_t k_forbidden =
      Reasons::kCameraOnly | Reasons::kContent | Reasons::kSizeZoom;
  if ((reasons & k_forbidden) != 0) {
    return false;
  }
  constexpr uint32_t k_allowed = Reasons::kSelection | Reasons::kShellOverlay;
  return (reasons & ~k_allowed) == 0;
}

void expand_px_bounds(int32_t vx, int32_t vy, int32_t pad, int32_t* min_x,
                      int32_t* min_y, int32_t* max_x, int32_t* max_y) {
  const int32_t lo_x = vx - pad;
  const int32_t hi_x = vx + pad;
  const int32_t lo_y = vy - pad;
  const int32_t hi_y = vy + pad;
  *min_x = (std::min)(*min_x, lo_x);
  *min_y = (std::min)(*min_y, lo_y);
  *max_x = (std::max)(*max_x, hi_x);
  *max_y = (std::max)(*max_y, hi_y);
}

bool compute_selection_pixel_aabb(const GisScene* scene, const ViewFrame* frame,
                                uint32_t width_px, uint32_t height_px,
                                bool flash_pulse, DamageRect* out) {
  if (!scene || !frame || width_px == 0 || height_px == 0 || !out) {
    return false;
  }
  int32_t min_x = width_px;
  int32_t min_y = height_px;
  int32_t max_x = 0;
  int32_t max_y = 0;
  bool any = false;

  auto visit_feature = [&](const GisScene::Feature& f) {
    for (const GisScene::Vertex& v : f.points) {
      int vx = 0;
      int vy = 0;
      frame->map_to_view(v.x, v.y, &vx, &vy);
      expand_px_bounds(vx, vy, kSelectionPadPx, &min_x, &min_y, &max_x, &max_y);
      any = true;
    }
  };

  {
    std::lock_guard<std::recursive_mutex> lock(scene->layer_mutex());
    for (const GisScene::Layer& layer : scene->layers()) {
      if (!layer.visible) {
        continue;
      }
      for (const GisScene::Feature& f : layer.features) {
        if (f.selected) {
          visit_feature(f);
        }
      }
    }
    if (flash_pulse) {
      if (const GisScene::Feature* flash = scene->selected_feature()) {
        visit_feature(*flash);
      }
    }
  }

  if (!any) {
    return false;
  }
  out->x = min_x;
  out->y = min_y;
  out->w = static_cast<uint32_t>((std::max)(0, max_x - min_x + 1));
  out->h = static_cast<uint32_t>((std::max)(0, max_y - min_y + 1));
  return !out->empty();
}

}  // namespace

void union_damage_rect(DamageRect* acc, const DamageRect& add) {
  if (!acc || add.empty()) {
    return;
  }
  if (acc->empty()) {
    *acc = add;
    return;
  }
  const int32_t x0 = (std::min)(acc->x, add.x);
  const int32_t y0 = (std::min)(acc->y, add.y);
  const int32_t x1 =
      (std::max)(acc->x + static_cast<int32_t>(acc->w),
                 add.x + static_cast<int32_t>(add.w));
  const int32_t y1 =
      (std::max)(acc->y + static_cast<int32_t>(acc->h),
                 add.y + static_cast<int32_t>(add.h));
  acc->x = x0;
  acc->y = y0;
  acc->w = static_cast<uint32_t>((std::max)(0, x1 - x0));
  acc->h = static_cast<uint32_t>((std::max)(0, y1 - y0));
}

void clamp_damage_rect(DamageRect* rect, uint32_t width_px, uint32_t height_px) {
  if (!rect || rect->empty() || width_px == 0 || height_px == 0) {
    return;
  }
  if (rect->x < 0) {
    rect->w = rect->w > static_cast<uint32_t>(-rect->x)
                  ? rect->w + static_cast<uint32_t>(rect->x)
                  : 0;
    rect->x = 0;
  }
  if (rect->y < 0) {
    rect->h = rect->h > static_cast<uint32_t>(-rect->y)
                  ? rect->h + static_cast<uint32_t>(rect->y)
                  : 0;
    rect->y = 0;
  }
  if (static_cast<uint32_t>(rect->x) >= width_px ||
      static_cast<uint32_t>(rect->y) >= height_px) {
    rect->w = 0;
    rect->h = 0;
    return;
  }
  const uint32_t max_w = width_px - static_cast<uint32_t>(rect->x);
  const uint32_t max_h = height_px - static_cast<uint32_t>(rect->y);
  if (rect->w > max_w) {
    rect->w = max_w;
  }
  if (rect->h > max_h) {
    rect->h = max_h;
  }
}

bool try_local_damage_rect(uint32_t reasons, const GisScene* scene,
                           const ViewFrame* frame,
                           const Map2dFrameCache::CameraKey& cam,
                           bool flash_pulse, DamageRect* last_selection_rect,
                           DamageRect* out) {
  if (!out || !reasons_allow_local_scissor(reasons)) {
    return false;
  }
  // Shell-only: keep underlay StaticReuse — no map scissor (overlay is separate
  // ShellOverlayEffect in the same command list).
  const bool has_selection = (reasons & Reasons::kSelection) != 0;
  if (!has_selection) {
    return false;
  }

  DamageRect current{};
  const bool have_current = compute_selection_pixel_aabb(
      scene, frame, cam.width_px, cam.height_px, flash_pulse, &current);

  DamageRect merged{};
  if (have_current) {
    merged = current;
    if (last_selection_rect != nullptr) {
      union_damage_rect(&merged, *last_selection_rect);
    }
  } else if (last_selection_rect != nullptr && !last_selection_rect->empty()) {
    merged = *last_selection_rect;
  } else {
    return false;
  }

  clamp_damage_rect(&merged, cam.width_px, cam.height_px);
  if (merged.empty()) {
    return false;
  }
  if (last_selection_rect != nullptr && have_current) {
    *last_selection_rect = merged;
  }
  *out = merged;
  return true;
}

}  // namespace content
