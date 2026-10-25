// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_PREP_SCENE3D_PREP_MAILBOX_H_
#define CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_PREP_SCENE3D_PREP_MAILBOX_H_

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>

namespace content {
namespace detail {

// Staged CPU prep intent for Scene3d present (T2 dual-track mailbox).
// T2.1 posts atmosphere POD / terrain remesh to the executor using this
// action; Display still owns Device and graph::present.
enum class Scene3dPrepAction : uint8_t {
  kWarmReuse = 0,
  kColdRebuild = 1,
};

// Map2d-shaped prep mailbox: stage gen + action without joining workers.
// Sync state is shared_ptr so async workers outlive mailbox teardown safely.
class Scene3dPrepMailbox {
 public:
  Scene3dPrepMailbox();
  ~Scene3dPrepMailbox();

  // Bumps live gen and waits for any in-flight async job before returning.
  void invalidate();

  uint64_t live_gen() const;
  uint64_t published_gen() const;
  uint64_t prep_build_count() const;

  // Shared so async workers can cancel without capturing mailbox `this`.
  std::shared_ptr<std::atomic<uint64_t>> live_gen_atomic() const;

  // L0 warm / first-frame cold: bump gen, mark done immediately (Display prep).
  uint64_t stage(Scene3dPrepAction action);

  // L2 cold overlap: bump gen, post |work(gen)| on GlobalNThreadPool.
  // Worker must not touch rhi::Device. |work| receives the staged gen.
  uint64_t stage_async(Scene3dPrepAction action,
                       std::function<void(uint64_t gen)> work);

  // L3: consume staged prep when done. |overlap_ms| is set only when
  // |wait_if_empty| is false and the job finished before this drain (no cv wait).
  bool drain(bool wait_if_empty, Scene3dPrepAction* drained_action = nullptr,
             int64_t* overlap_ms = nullptr);

 private:
  struct Sync {
    std::mutex mu;
    std::condition_variable cv;
    std::atomic<uint64_t> live_prep_gen{0};
    std::atomic<uint64_t> published_prep_gen{0};
    std::atomic<uint64_t> prep_build_count{0};
  };

  struct PendingPrep {
    uint64_t gen = 0;
    Scene3dPrepAction action = Scene3dPrepAction::kColdRebuild;
    std::atomic<bool> done{false};
    std::chrono::steady_clock::time_point posted_at{};
    std::chrono::steady_clock::time_point done_at{};
  };

  uint64_t stage_locked(Scene3dPrepAction action,
                        std::shared_ptr<PendingPrep> job);

  std::shared_ptr<Sync> sync_;
  std::shared_ptr<PendingPrep> pending_;
};

}  // namespace detail
}  // namespace content

#endif  // CONTENT_BROWSER_PRESENT_SCENE3D_FRAME_PREP_SCENE3D_PREP_MAILBOX_H_
