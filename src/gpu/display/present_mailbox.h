// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef GPU_DISPLAY_PRESENT_MAILBOX_H_
#define GPU_DISPLAY_PRESENT_MAILBOX_H_

#include "gpu/frame_sink.h"

#include <cstdint>
#include <functional>
#include <memory>

// Process-local Display / present mailbox (Chromium Display analogue).
// A dedicated thread exclusively runs draw_and_swap / OutputSurface present.
// UI / shell threads enqueue work; they must not call draw_and_swap themselves.
//
// MapViewport FlyCube HWND present uses a sibling mailbox in ui::views with
// the same Submit / BeginFrame shape (views cannot hard-link this target yet).
// UI agent: wire MapViewport to PresentMailbox::instance() once
// //src/ui/views depends on //src/gpu:gpu_backend, or keep the views sibling.
namespace gpu {

// One frame request for the display thread.
struct PresentSubmit {
  detail::OutputSurface* surface = nullptr;  // non-owning; valid until drain
  DrawRequest request;
  // Caller frame token (MapViewport frame_request_ / surface generation).
  uint32_t frame_token = 0;
  // Invoked on the display thread after draw_and_swap. Keep it short; post
  // back to the UI thread if HWND work is required.
  std::function<void(bool ok, uint32_t frame_token)> completion;
};

// Vsync-oriented BeginFrame source. Uses IDXGIOutput::WaitForVBlank
// (ui::gfx::VblankClock); |set_interval_ms| is the Sleep fallback only.
class BeginFrameSource {
 public:
  virtual ~BeginFrameSource() = default;
  // Fallback Sleep interval in milliseconds when DXGI WaitForVBlank fails.
  virtual void set_interval_ms(uint32_t interval_ms) = 0;
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual bool is_running() const = 0;
};

// Process-local mailbox. Thread-safe Submit; device/compose work stays inside.
class PresentMailbox {
 public:
  static PresentMailbox& instance();

  PresentMailbox(const PresentMailbox&) = delete;
  PresentMailbox& operator=(const PresentMailbox&) = delete;

  // Enqueue one draw_and_swap. Coalesces to the latest submit per surface when
  // the queue already holds a pending frame for that surface pointer.
  void submit(PresentSubmit submit);

  // BeginFrame: posts "should present" into this mailbox (not WM_PAINT).
  void start_begin_frame(uint32_t interval_ms = 16);
  void stop_begin_frame();
  BeginFrameSource* begin_frame_source();

  // Scheduler hooks for BeginFrame ticks (optional).
  // should_produce: true → call produce_frame and submit the result.
  void set_should_produce(std::function<bool()> should_produce);
  void set_produce_frame(std::function<PresentSubmit()> produce_frame);

  // Block until the queue is empty and the display thread is idle. Call before
  // destroying OutputSurface / HWND. Does not join a second GetMessage loop.
  void drain_for_shutdown();

 private:
  PresentMailbox();
  ~PresentMailbox();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace gpu

#endif  // GPU_DISPLAY_PRESENT_MAILBOX_H_
