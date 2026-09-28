// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/display/present_mailbox.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <utility>

#include "ui/gfx/display/vblank_wait.h"

namespace gpu {
namespace {

// DWM / DXGI BeginFrame: WaitForVBlank on the primary monitor, with a Sleep
// fallback when DXGI output cannot be resolved (interval_ms).
class DwmBeginFrameSource final : public BeginFrameSource {
 public:
  explicit DwmBeginFrameSource(std::function<void()> on_tick)
      : on_tick_(std::move(on_tick)) {}

  ~DwmBeginFrameSource() override { stop(); }

  void set_interval_ms(uint32_t interval_ms) override {
    interval_ms_.store(interval_ms == 0 ? 16u : interval_ms,
                       std::memory_order_relaxed);
  }

  void start() override {
    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
      return;
    }
    thread_ = std::thread([this]() { run(); });
  }

  void stop() override {
    if (!running_.exchange(false)) {
      return;
    }
    // WaitForVBlank may block up to one refresh; join after it returns.
    if (thread_.joinable()) {
      thread_.join();
    }
  }

  bool is_running() const override {
    return running_.load(std::memory_order_acquire);
  }

 private:
  void run() {
    ui::gfx::VblankClock clock;
    clock.set_hwnd(nullptr);
    while (running_.load(std::memory_order_acquire)) {
      const uint32_t fallback_ms =
          interval_ms_.load(std::memory_order_relaxed);
      clock.wait_next(fallback_ms);
      if (!running_.load(std::memory_order_acquire)) {
        break;
      }
      if (on_tick_) {
        on_tick_();
      }
    }
  }

  std::function<void()> on_tick_;
  std::atomic<uint32_t> interval_ms_{16};
  std::atomic<bool> running_{false};
  std::thread thread_;
};

}  // namespace

struct PresentMailbox::Impl {
  std::mutex mu;
  std::condition_variable cv;
  std::deque<PresentSubmit> queue;
  bool stop = false;
  bool busy = false;
  std::thread worker;
  std::function<bool()> should_produce;
  std::function<PresentSubmit()> produce_frame;
  std::unique_ptr<DwmBeginFrameSource> begin_frame;

  void ensure_worker() {
    if (worker.joinable()) {
      return;
    }
    worker = std::thread([this]() { worker_main(); });
  }

  void worker_main() {
    for (;;) {
      PresentSubmit job;
      {
        std::unique_lock<std::mutex> lock(mu);
        cv.wait(lock, [this]() { return stop || !queue.empty(); });
        if (stop && queue.empty()) {
          busy = false;
          return;
        }
        job = std::move(queue.front());
        queue.pop_front();
        busy = true;
      }
      bool ok = false;
      if (job.surface) {
        ok = draw_and_swap(job.surface, job.request);
      }
      if (job.completion) {
        job.completion(ok, job.frame_token);
      }
      {
        std::lock_guard<std::mutex> lock(mu);
        busy = false;
        if (queue.empty()) {
          cv.notify_all();
        }
      }
    }
  }

  void on_begin_frame_tick() {
    std::function<bool()> should;
    std::function<PresentSubmit()> produce;
    {
      std::lock_guard<std::mutex> lock(mu);
      should = should_produce;
      produce = produce_frame;
    }
    if (!should || !produce) {
      return;
    }
    if (!should()) {
      return;
    }
    PresentSubmit submit = produce();
    if (!submit.surface) {
      return;
    }
    PresentMailbox::instance().submit(std::move(submit));
  }
};

PresentMailbox::PresentMailbox() : impl_(std::make_unique<Impl>()) {
  impl_->begin_frame = std::make_unique<DwmBeginFrameSource>([this]() {
    impl_->on_begin_frame_tick();
  });
}

PresentMailbox::~PresentMailbox() {
  drain_for_shutdown();
  if (impl_->begin_frame) {
    impl_->begin_frame->stop();
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->stop = true;
  }
  impl_->cv.notify_all();
  if (impl_->worker.joinable()) {
    impl_->worker.join();
  }
}

PresentMailbox& PresentMailbox::instance() {
  static PresentMailbox mailbox;
  return mailbox;
}

void PresentMailbox::submit(PresentSubmit submit) {
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    // Coalesce: keep only the latest frame per surface.
    if (submit.surface) {
      for (auto it = impl_->queue.begin(); it != impl_->queue.end();) {
        if (it->surface == submit.surface) {
          it = impl_->queue.erase(it);
        } else {
          ++it;
        }
      }
    }
    impl_->queue.push_back(std::move(submit));
    impl_->ensure_worker();
  }
  impl_->cv.notify_one();
}

void PresentMailbox::start_begin_frame(uint32_t interval_ms) {
  impl_->begin_frame->set_interval_ms(interval_ms);
  impl_->begin_frame->start();
}

void PresentMailbox::stop_begin_frame() {
  impl_->begin_frame->stop();
}

BeginFrameSource* PresentMailbox::begin_frame_source() {
  return impl_->begin_frame.get();
}

void PresentMailbox::set_should_produce(std::function<bool()> should_produce) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->should_produce = std::move(should_produce);
}

void PresentMailbox::set_produce_frame(
    std::function<PresentSubmit()> produce_frame) {
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->produce_frame = std::move(produce_frame);
}

void PresentMailbox::drain_for_shutdown() {
  stop_begin_frame();
  std::unique_lock<std::mutex> lock(impl_->mu);
  impl_->cv.wait(lock, [this]() {
    return impl_->queue.empty() && !impl_->busy;
  });
}

}  // namespace gpu
