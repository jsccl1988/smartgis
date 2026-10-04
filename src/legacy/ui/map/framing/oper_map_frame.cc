// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "stdafx.h"

#include "legacy/ui/map/framing/oper_map_frame.h"

#include <algorithm>

#include "base/core/log.h"
#include "legacy/core/macros/macros.h"
#include "legacy/gis/present/carto/style_api.h"

namespace ui {
namespace detail {

bool frame_oper_map(HWND hwnd,
                    render::LPRENDERDEVICE device,
                    gis::Map* oper_map,
                    OperMapFrameState* state,
                    bool realtime) {
  if (!hwnd || !device || !oper_map || !state) {
    return false;
  }
  if (state->framing) {
    LOGGING(LOG_INFO, "frame_oper_map: re-entrant skip");
    return false;
  }
  RECT client = {};
  ::GetClientRect(hwnd, &client);
  const int cx = client.right - client.left;
  const int cy = client.bottom - client.top;
  if (cx <= 0 || cy <= 0) {
    LOGGING(LOG_INFO, "frame_oper_map: skip (client %dx%d)", cx, cy);
    return false;
  }

  state->framing = true;
  device->Resize(0, 0, cx, cy);

  gis::Envelope env;
  oper_map->CalEnvelope();
  oper_map->get_envelope(env);
  bool framed = false;
  if (env.is_init()) {
    base::fRect frt;
    envelope_to_rect(frt, env);
    const float pad_x = (std::max)(frt.width() / 40.f, 0.01f);
    const float pad_y = (std::max)(frt.height() / 40.f, 0.01f);
    frt.rt.x += pad_x;
    frt.rt.y += pad_y;
    frt.lb.x -= pad_x;
    frt.lb.y -= pad_y;
    const int zr = device->ZoomToRect(oper_map, frt, realtime);
    framed = (zr == SMT_ERR_NONE);
    LOGGING(LOG_INFO,
            "frame_oper_map: ZoomToRect rt=%d zr=%d env=(%.3f,%.3f)-(%.3f,%.3f)",
            realtime ? 1 : 0, zr, env.MinX, env.MinY, env.MaxX, env.MaxY);
  } else {
    base::lRect lrt;
    lrt.lb.x = 0;
    lrt.rt.y = 0;
    lrt.rt.x = cx;
    lrt.lb.y = cy;
    const int rr = device->RefreshDirectly(oper_map, lrt, realtime);
    framed = (rr == SMT_ERR_NONE);
    LOGGING(LOG_INFO, "frame_oper_map: empty envelope RefreshDirectly=%d", rr);
  }

  state->framed = framed;
  state->framing = false;
  return framed;
}

void request_oper_map_frame(HWND hwnd,
                            gis::Map* oper_map,
                            const OperMapFrameState& state) {
  if (!hwnd || !oper_map || state.framed || state.framing) {
    return;
  }
  // Timer (not PostMessage): BCG OnInitialUpdate nests a pump that would
  // run a posted frame mid-construction and AV in ZoomToRect.
  ::SetTimer(hwnd, 71, 50, nullptr);
}

}  // namespace detail
}  // namespace ui
