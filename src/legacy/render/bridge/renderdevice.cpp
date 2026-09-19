// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "legacy/render/bridge/renderdevice.h"

#include "legacy/render/bridge/leftover_record.h"

using namespace base;

namespace render {
RENDER_EXPORT_API void bind_rhi_present(void* native_window) {
  // Strangler: Init(HWND) → process-wide leftover session. Does not create
  // FlyCube on this HWND (shared with SmtGdi/Gl present; dual ownership
  // caused 0xC000041D). Recording stays on Null until attach() from a
  // dedicated GPU HWND (MapViewport / gpu).
  render::scene::leftover_session().bind_present_hwnd(native_window);
}
}  // namespace render
