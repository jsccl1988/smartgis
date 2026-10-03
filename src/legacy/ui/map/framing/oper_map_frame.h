// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef LEGACY_UI_MAP_FRAMING_OPER_MAP_FRAME_H_
#define LEGACY_UI_MAP_FRAMING_OPER_MAP_FRAME_H_

#include "gis/model/map/map.h"
#include "legacy/render/rhi2d/public/device/renderdevice.h"

namespace ui {
namespace detail {

// Shared framing flags for deferred ZoomToRect (avoids nested-pump AV).
struct OperMapFrameState {
  bool framed = false;
  bool framing = false;
};

// Fit windowport to the oper-map envelope and paint. Updates |state|.
bool frame_oper_map(HWND hwnd,
                    render::LPRENDERDEVICE device,
                    gis::SmtMap* oper_map,
                    OperMapFrameState* state,
                    bool realtime);

// Arm a one-shot timer (id 71) when client size may still be 0x0.
void request_oper_map_frame(HWND hwnd,
                            gis::SmtMap* oper_map,
                            const OperMapFrameState& state);

}  // namespace detail
}  // namespace ui

#endif  // LEGACY_UI_MAP_FRAMING_OPER_MAP_FRAME_H_