// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// Display mailbox thread: BeginFrame pacing, RHI init/resize/destroy, present.

#include "ui/views/map/viewport/draw_host.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windowsx.h>

#include "base/core/log.h"
#include "base/process/switches.h"
#include "render/rhi/rhi.h"
#include "ui/gfx/raster/paint_stats.h"
#include "ui/views/kernel/shell/dpi.h"
#include "ui/views/kernel/shell/theme.h"
#include "ui/views/kernel/widget/widget.h"
#include "ui/views/map/frame/identity_hud.h"
#include "ui/views/map/device/device_load.h"
#include "ui/views/map/frame/embed_fill.h"
#include "ui/views/map/viewport/features.h"
#include "ui/views/map/input/viewport_input.h"

namespace ui {
namespace views {

using detail::CreateRenderDeviceFn;
using detail::DeviceObj;
using detail::clamp_scene3d_swapchain_size;
using detail::exe_dir;
using detail::file_exists;
using detail::fill_map_embed_opaque;
using detail::init_device_seh;
using detail::load_first;
using detail::register_identity_hud_class;
using detail::route_view_host_input;
using detail::route_view_host_pointer;

namespace {

// SEH must not share a frame with C++ objects that need unwind. Keeps a
// Scene3d/GPU present AV inside present_gpu from killing the process on tab switch.
bool call_gpu_present_seh(bool (*fn)(void*, void*, uint32_t, uint32_t),
                          void* ctx, void* device, uint32_t w, uint32_t h,
                          DWORD* exception_code) {
  // Avoid GetExceptionCode() here: under this TU's includes /EHsc it yields
  // C2064. Callers only need "present faulted" vs success.
  __try {
    return fn(ctx, device, w, h);
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    if (exception_code) {
      *exception_code = 0xC0000005u;  // generic AV; exact code not required
    }
    return false;
  }
}

bool invoke_shared_gpu_present(void* ctx, void* device, uint32_t w,
                               uint32_t h) {
  auto* present = static_cast<DrawHost::GpuPresentFn*>(ctx);
  return present && *present && (*present)(device, w, h);
}

}  // namespace

void DrawHost::ensure_display_thread() {
  std::lock_guard<std::mutex> lock(display_mu_);
  if (display_started_) {
    return;
  }
  display_stop_ = false;
  display_started_ = true;
  display_thread_ = std::thread([this]() { display_thread_main(); });
}

void DrawHost::stop_display_thread() {
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    if (!display_started_) {
      return;
    }
    display_stop_ = true;
  }
  display_cv_.notify_all();
  if (display_thread_.joinable()) {
    display_thread_.join();
  }
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    display_started_ = false;
    display_queue_.clear();
    // release_rhi_device may still be waiting on destroy ack; wake it.
    display_destroy_ack_ = true;
  }
  display_cv_.notify_all();
}

void DrawHost::signal_display() {
  display_cv_.notify_all();
}

void DrawHost::enqueue_display_task(DisplayTask task) {
  ensure_display_thread();
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    // Coalesce resize ops; keep ordering for init/destroy.
    if (task.op == DisplayOp::kResize) {
      for (auto it = display_queue_.begin(); it != display_queue_.end();) {
        if (it->op == DisplayOp::kResize) {
          it = display_queue_.erase(it);
        } else {
          ++it;
        }
      }
    }
    display_queue_.push_back(task);
  }
  display_cv_.notify_one();
}

void DrawHost::display_run_present(uint32_t width_px, uint32_t height_px,
                                     uint32_t frame_token) {
  if (display_stop_) {
    return;
  }
  // Scenic MemFrame path must never invoke DXGI GPU present present (SEH).
  if (const char* map_eng = base::switch_cstr("map2d-engine")) {
    if (map_eng[0] && _stricmp(map_eng, "scenic") == 0) {
      return;
    }
  }
  if (const char* scene_eng = base::switch_cstr("scene3d-engine")) {
    if (scene_eng[0] && _stricmp(scene_eng, "scenic") == 0) {
      return;
    }
  }
  if (!rhi_device_) {
    LOGGING(LOG_WARNING, "rhi.present skip: no device token=%u", frame_token);
    return;
  }
  // atomic_load shared_ptr: refcount bump only; invoke outside any mutex.
  auto submit =
      std::atomic_load_explicit(&gpu_submit_, std::memory_order_acquire);
  auto present =
      std::atomic_load_explicit(&gpu_present_, std::memory_order_acquire);
  bool ok = false;
  DWORD seh_code = 0;
  if (submit && *submit) {
    // Submit path is UI-agent only; keep direct call (no Scene3d AV class).
    ok = (*submit)(rhi_device_, width_px, height_px, frame_token);
  } else if (present && *present) {
    ok = call_gpu_present_seh(invoke_shared_gpu_present, present.get(),
                              rhi_device_, width_px, height_px, &seh_code);
    if (!ok && seh_code != 0) {
      LOGGING(LOG_ERROR,
              "rhi.present SEH code=0x%08lX size=%ux%u token=%u role=%d",
              static_cast<unsigned long>(seh_code), width_px, height_px,
              frame_token, static_cast<int>(role_));
    }
  } else {
    LOGGING(LOG_WARNING,
            "rhi.present skip: no gpu_present/submit callback size=%ux%u",
            width_px, height_px);
    return;
  }
  const bool was_ok = last_gpu_present_ok_.load(std::memory_order_acquire);
  last_gpu_present_ok_.store(ok, std::memory_order_release);
  if (ok) {
    frame_presented_.store(frame_token, std::memory_order_release);
    note_hud_frame();
    // Safe to lift NOREDIRECTION after a real Present (Resize path hides first).
    reveal_gpu_present_if_ready();
    if (!was_ok) {
      LOGGING(LOG_INFO, "rhi.present recovered size=%ux%u token=%u role=%d",
              width_px, height_px, frame_token, static_cast<int>(role_));
    }
  } else if (was_ok || frame_token <= 2) {
    LOGGING(LOG_ERROR,
            "rhi.present FAIL size=%ux%u token=%u role=%d (DXGI SoT; no GDI "
            "BitBlt overlay)",
            width_px, height_px, frame_token, static_cast<int>(role_));
  }
  if (last_begin_frame_qpc_ != 0) {
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    const std::uint64_t qpc = static_cast<std::uint64_t>(now.QuadPart);
    if (qpc >= last_begin_frame_qpc_) {
      ui::gfx::note_begin_frame_to_present_qpc(qpc - last_begin_frame_qpc_);
    }
  }
}

void DrawHost::display_run_begin_frame() {
  // Scheduler: produce only when a frame was requested and not yet presented.
  // Steady-state orbit must not force shell full repaints (no InvalidateRect).
  LARGE_INTEGER begin_qpc = {};
  QueryPerformanceCounter(&begin_qpc);
  last_begin_frame_qpc_ = static_cast<std::uint64_t>(begin_qpc.QuadPart);
  ui::gfx::note_begin_frame_qpc(last_begin_frame_qpc_);

  if (role_ != Role::kScene3d) {
    if (const char* env = base::switch_cstr("force-gdi-map-overlay")) {
      if (env[0] == '1' && env[1] == '\0') {
        return;
      }
    }
  }
  const uint32_t requested =
      frame_request_.load(std::memory_order_acquire);
  const uint32_t presented =
      frame_presented_.load(std::memory_order_acquire);
  if (requested == presented) {
    return;
  }
  if (!rhi_device_) {
    return;
  }
  if (!has_gpu_cb_.load(std::memory_order_acquire)) {
    return;
  }
  uint32_t w = 0;
  uint32_t h = 0;
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    w = display_client_w_ != 0 ? display_client_w_ : 1;
    h = display_client_h_ != 0 ? display_client_h_ : 1;
  }
  display_run_present(w, h, requested);
}

void DrawHost::shutdown_rhi_on_display_thread() {
#ifdef HAS_FLYCUBE
  render::rhi::Device* device = nullptr;
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    device = static_cast<render::rhi::Device*>(rhi_device_);
    rhi_device_ = nullptr;
    last_gpu_present_ok_.store(false, std::memory_order_release);
  }
  if (!device) {
    return;
  }
  device->shutdown();
#else
  rhi_device_ = nullptr;
#endif
}

void DrawHost::display_thread_main() {
  // P5: interruptible idle wait (enqueue / signal_display) + DXGI vblank
  // phase-align before BeginFrame. WaitForVBlank alone cannot be woken by
  // display_cv_, which left init/destroy and frame requests delayed or stuck
  // while 2D WM_PAINT trusted a stale last_gpu_present_ok_ clear.
  for (;;) {
    DisplayTask task;
    bool have_task = false;
    {
      std::unique_lock<std::mutex> lock(display_mu_);
      if (display_stop_ && display_queue_.empty()) {
        break;
      }
      if (display_queue_.empty() && !display_stop_) {
        display_cv_.wait_for(lock, std::chrono::milliseconds(16), [this]() {
          return display_stop_ || !display_queue_.empty();
        });
      }
      if (display_stop_ && display_queue_.empty()) {
        break;
      }
      if (!display_queue_.empty()) {
        task = display_queue_.front();
        display_queue_.pop_front();
        have_task = true;
      }
    }
    if (!have_task) {
      if (display_stop_) {
        break;
      }
      HWND hwnd = nullptr;
      {
        std::lock_guard<std::mutex> lock(display_mu_);
        if (!display_queue_.empty()) {
          continue;
        }
        hwnd = display_hwnd_;
      }
      // Do NOT call IDXGIOutput::WaitForVBlank here. It cannot be woken by
      // display_cv_, so a close-time kDestroy / display_stop_ posted while
      // WaitForVBlank blocks leaves release_rhi_device waiting forever
      // (SmartGIS.exe hang on WM_CLOSE). The wait_for(~16ms) above is
      // the interruptible pace; skip DXGI phase-align on the mailbox thread.
      (void)hwnd;
      {
        std::lock_guard<std::mutex> lock(display_mu_);
        if (display_stop_) {
          break;
        }
        if (!display_queue_.empty()) {
          continue;
        }
      }
      display_run_begin_frame();
      continue;
    }
#ifdef HAS_FLYCUBE
      if (task.op == DisplayOp::kInit) {
        LOGGING(LOG_INFO, "rhi.display Init hwnd=%p size=%ux%u", task.hwnd,
                task.width_px, task.height_px);
        if (rhi_device_) {
          auto* old = static_cast<render::rhi::Device*>(rhi_device_);
          old->shutdown();
          rhi_device_ = nullptr;
        }
        const render::rhi::Backend backend = render::rhi::preferred_gpu_backend();
        render::rhi::Device* device = render::rhi::create_device(backend);
        if (!device) {
          LOGGING(LOG_ERROR,
                  "rhi.display Init create_device(%s) returned null",
                  render::rhi::backend_display_name(backend));
          {
            std::lock_guard<std::mutex> lock(display_mu_);
            display_init_ = DisplayInit::kFail;
          }
          display_cv_.notify_all();
          continue;
        }
        render::rhi::DeviceDesc desc;
        desc.native_window = task.hwnd;
        desc.width = task.width_px != 0 ? task.width_px : 1;
        desc.height = task.height_px != 0 ? task.height_px : 1;
        if (!device->initialize(desc)) {
          LOGGING(LOG_ERROR,
                  "rhi.display Init initialize failed backend=%s hwnd=%p "
                  "%ux%u",
                  render::rhi::backend_display_name(backend), task.hwnd,
                  desc.width, desc.height);
          device->shutdown();
          {
            std::lock_guard<std::mutex> lock(display_mu_);
            display_init_ = DisplayInit::kFail;
          }
          display_cv_.notify_all();
          continue;
        }
        LOGGING(LOG_INFO, "rhi.display Init device ok backend=%s %ux%u",
                render::rhi::backend_display_name(device->backend()), desc.width,
                desc.height);
        render::rhi::CommandList* list = device->create_command_list();
        if (list) {
          render::rhi::RenderPassDesc pass;
          pass.clear_r = 0.05f;
          pass.clear_g = 0.12f;
          pass.clear_b = 0.18f;
          pass.clear_a = 1.f;
          pass.width = desc.width;
          pass.height = desc.height;
          list->begin_render_pass(pass);
          list->set_viewport(0, 0, static_cast<float>(desc.width),
                             static_cast<float>(desc.height), 0, 1);
          list->end_render_pass();
          list->close();
          device->execute(list);
          device->destroy_command_list(list);
        }
        device->present();
        {
          std::lock_guard<std::mutex> lock(display_mu_);
          rhi_device_ = device;
          display_client_w_ = desc.width;
          display_client_h_ = desc.height;
          display_hwnd_ = task.hwnd;
          display_vblank_.set_hwnd(task.hwnd);
          display_init_ = DisplayInit::kOk;
        }
        display_cv_.notify_all();
        // Do NOT call present_gpu here. Pre-show layout often hands a stale
        // multi-k client rect; map2d present_gpu at that size blocks the
        // Display thread so the post-attach WM_SIZE Resize never runs and the
        // HWND stays on this navy clear. First real frame is Resize below
        // (and BeginFrame). Keep last_gpu_present_ok_ false so GDI overlay
        // is not skipped while waiting.
        last_gpu_present_ok_.store(false, std::memory_order_release);
        mark_gpu_surface_dirty();
        // Scene3d Init Present is a navy clear — do not lift the popup until
        // present_gpu actually draws DEM (reveal_gpu_present_if_ready).
        if (role_ != Role::kScene3d) {
          reveal_gpu_present_if_ready();
        }
        display_cv_.notify_all();
        continue;
      }
      if (task.op == DisplayOp::kResize && rhi_device_) {
        // Flip-model NOREDIRECTIONBITMAP shows the desktop while initialize()
        // rebuilds the swapchain. Hide until Present restores opaque pixels.
        if (gpu_present_hwnd_ && IsWindow(gpu_present_hwnd_)) {
          // Display thread: must not block on UI (SWP_ASYNCWINDOWPOS).
          SetWindowPos(gpu_present_hwnd_, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE |
                           SWP_HIDEWINDOW | SWP_ASYNCWINDOWPOS);
        }
        if (HWND embed = native_view()) {
          if (IsWindow(embed) && IsWindowVisible(embed)) {
            InvalidateRect(embed, nullptr, FALSE);
          }
        }
        auto* device = static_cast<render::rhi::Device*>(rhi_device_);
        render::rhi::DeviceDesc desc;
        desc.native_window = task.hwnd;
        desc.width = task.width_px != 0 ? task.width_px : 1;
        desc.height = task.height_px != 0 ? task.height_px : 1;
        // Ignoring initialize() failure left a half-built swapchain; the next
        // Scene3d execute then AVd on a null backbuffer RTV (lazy 3D attach
        // posts WM_SIZE right after GPU present Init).
        const bool resized = device->initialize(desc);
        {
          std::lock_guard<std::mutex> lock(display_mu_);
          display_client_w_ = desc.width;
          display_client_h_ = desc.height;
          display_hwnd_ = task.hwnd;
          display_vblank_.set_hwnd(task.hwnd);
        }
        last_gpu_present_ok_.store(false, std::memory_order_release);
        mark_gpu_surface_dirty();
        if (!resized) {
          LOGGING(LOG_ERROR,
                  "rhi.display Resize initialize failed hwnd=%p %ux%u "
                  "role=%d - skip present",
                  task.hwnd, desc.width, desc.height,
                  static_cast<int>(role_));
          continue;
        }
        // initialize() rebuilds the flip swapchain (navy clear). Redraw
        // immediately — waiting for a later BeginFrame token left Map2d on
        // the clear after the post-attach WM_SIZE shrink.
        if (has_gpu_cb_.load(std::memory_order_acquire)) {
          const uint32_t token =
              frame_request_.fetch_add(1, std::memory_order_acq_rel) + 1;
          display_run_present(desc.width, desc.height, token);
        }
        reveal_gpu_present_if_ready();
        continue;
      }
      if (task.op == DisplayOp::kDestroy) {
        if (rhi_device_) {
          auto* device = static_cast<render::rhi::Device*>(rhi_device_);
          device->shutdown();
          // GPU present CRT / allocator can heap-corrupt on operator delete after
          // a live DX12 session; leak the facade (matches rhi_test).
          rhi_device_ = nullptr;
        }
        last_gpu_present_ok_.store(false, std::memory_order_release);
        {
          std::lock_guard<std::mutex> lock(display_mu_);
          display_hwnd_ = nullptr;
          display_vblank_.reset();
          display_destroy_ack_ = true;
        }
        display_cv_.notify_all();
        continue;
      }
#else
      (void)task;
#endif
  }
  // stop_display_thread joins without a kDestroy when init was abandoned.
  // Drop the swapchain here or GDI/content present cannot cover the HWND.
  shutdown_rhi_on_display_thread();
}

void DrawHost::release_rhi_device() {
  if (!rhi_device_ && !display_started_) {
    return;
  }
  if (!display_started_) {
    if (rhi_device_) {
      auto* device = static_cast<render::rhi::Device*>(rhi_device_);
      device->shutdown();
      rhi_device_ = nullptr;
    }
    return;
  }
  // Must not run on the Display thread (would deadlock).
  {
    std::lock_guard<std::mutex> lock(display_mu_);
    display_destroy_ack_ = false;
  }
  enqueue_display_task(DisplayTask{DisplayOp::kDestroy, nullptr, 0, 0});
  std::unique_lock<std::mutex> lock(display_mu_);
  // display_stop_ is set by stop_display_thread / detach so close cannot
  // block forever if kDestroy is raced with mailbox exit.
  display_cv_.wait(lock, [this]() {
    return display_destroy_ack_ || display_stop_;
  });
}

}  // namespace views
}  // namespace ui
