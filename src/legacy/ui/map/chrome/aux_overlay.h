// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_CHROME_AUX_OVERLAY_H_
#define LEGACY_UI_MAP_CHROME_AUX_OVERLAY_H_

#include "legacy/render/rhi2d/public/device/renderdevice.h"
#include "tool/draft/draft.h"

namespace ui {
namespace detail {

// Rubber-band after RenderMap onto the window DC (MRD_BL_DIRECT).
void paint_aux_overlay(render::LPRENDERDEVICE device,
                       const tool::AuxOverlay* overlay);

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_MAP_CHROME_AUX_OVERLAY_H_