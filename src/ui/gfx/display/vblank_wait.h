// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef UI_GFX_DISPLAY_VBLANK_WAIT_H_
#define UI_GFX_DISPLAY_VBLANK_WAIT_H_

#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

struct IDXGIOutput;

namespace ui {
namespace gfx {

// Paces BeginFrame to the monitor refresh via IDXGIOutput::WaitForVBlank.
// Shared by MapViewport Display thread and gpu::PresentMailbox (no HWND →
// primary monitor). DXGI failure falls back to Sleep(|fallback_ms|).
class VblankClock {
 public:
  VblankClock() = default;
  ~VblankClock();

  VblankClock(const VblankClock&) = delete;
  VblankClock& operator=(const VblankClock&) = delete;

  // Bind to the monitor that contains |hwnd|. nullptr → primary monitor.
  // Recreates the cached IDXGIOutput when the monitor changes.
  void set_hwnd(HWND hwnd);

  // Block until the next vertical blank. On DXGI failure: Sleep(|fallback_ms|)
  // when non-zero, or return immediately when |fallback_ms| == 0 (caller paced).
  // Returns true when WaitForVBlank succeeded.
  bool wait_next(uint32_t fallback_ms = 16);

  void reset();

 private:
  bool ensure_output();

  HWND hwnd_ = nullptr;
  HMONITOR monitor_ = nullptr;
  IDXGIOutput* output_ = nullptr;
};

}  // namespace gfx
}  // namespace ui

#endif  // UI_GFX_DISPLAY_VBLANK_WAIT_H_
