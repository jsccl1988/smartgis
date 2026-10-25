// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/browser/present/scene3d/frame/prep/scene3d_prep_mailbox.h"

#include "base/execution/executor/pool/global_executor.h"
#include "base/execution/futures/combinators/async.h"

namespace content {
namespace detail {
namespace {

int64_t steady_ms_between(std::chrono::steady_clock::time_point a,
                          std::chrono::steady_clock::time_point b) {
  if (a == std::chrono::steady_clock::time_point{} ||
      b == std::chrono::steady_clock::time_point{}) {
    return 0;
  }
  const auto delta = b - a;
  if (delta.count() <= 0) {
    return 0;
  }
  return static_cast<int64_t>(
      std::chrono::duration_cast<std::chrono::milliseconds>(delta).count());
}

}  // namespace

Scene3dPrepMailbox::Scene3dPrepMailbox() : sync_(std::make_shared<Sync>()) {}

Scene3dPrepMailbox::~Scene3dPrepMailbox() {
  invalidate();
}

void Scene3dPrepMailbox::invalidate() {
  if (!sync_) {
    return;
  }
  sync_->live_prep_gen.fetch_add(1, std::memory_order_acq_rel);
  std::shared_ptr<PendingPrep> wait_job;
  {
    std::lock_guard<std::mutex> lock(sync_->mu);
    wait_job = pending_;
  }
  if (wait_job) {
    std::unique_lock<std::mutex> lock(sync_->mu);
    sync_->cv.wait(lock, [&] {
      return wait_job->done.load(std::memory_order_acquire);
    });
    if (pending_ == wait_job) {
      pending_.reset();
    }
  } else {
    std::lock_guard<std::mutex> lock(sync_->mu);
    pending_.reset();
  }
  sync_->cv.notify_all();
}

uint64_t Scene3dPrepMailbox::live_gen() const {
  return sync_->live_prep_gen.load(std::memory_order_acquire);
}

uint64_t Scene3dPrepMailbox::published_gen() const {
  return sync_->published_prep_gen.load(std::memory_order_acquire);
}

uint64_t Scene3dPrepMailbox::prep_build_count() const {
  return sync_->prep_build_count.load(std::memory_order_acquire);
}

std::shared_ptr<std::atomic<uint64_t>> Scene3dPrepMailbox::live_gen_atomic()
    const {
  // Aliasing shared_ptr: keeps Sync alive; points at live_prep_gen.
  return std::shared_ptr<std::atomic<uint64_t>>(sync_,
                                                &sync_->live_prep_gen);
}

uint64_t Scene3dPrepMailbox::stage_locked(
    Scene3dPrepAction action, std::shared_ptr<PendingPrep> job) {
  job->action = action;
  {
    std::lock_guard<std::mutex> lock(sync_->mu);
    if (pending_ && !pending_->done.load(std::memory_order_acquire)) {
      sync_->live_prep_gen.fetch_add(1, std::memory_order_acq_rel);
    }
    const uint64_t gen =
        sync_->live_prep_gen.fetch_add(1, std::memory_order_acq_rel) + 1;
    job->gen = gen;
    pending_ = job;
  }
  return job->gen;
}

uint64_t Scene3dPrepMailbox::stage(Scene3dPrepAction action) {
  auto job = std::make_shared<PendingPrep>();
  const uint64_t gen = stage_locked(action, job);
  job->posted_at = std::chrono::steady_clock::now();
  job->done_at = job->posted_at;
  job->done.store(true, std::memory_order_release);
  sync_->cv.notify_all();
  return gen;
}

uint64_t Scene3dPrepMailbox::stage_async(
    Scene3dPrepAction action, std::function<void(uint64_t gen)> work) {
  auto job = std::make_shared<PendingPrep>();
  job->posted_at = std::chrono::steady_clock::now();
  job->done.store(false, std::memory_order_release);
  const uint64_t gen = stage_locked(action, job);
  std::shared_ptr<PendingPrep> job_keep = job;
  std::shared_ptr<Sync> sync = sync_;
  (void)base::execution::async(
      base::execution::GlobalNThreadPoolExecutor{},
      [job_keep, sync, w = std::move(work), gen]() mutable {
        if (w) {
          w(gen);
        }
        job_keep->done_at = std::chrono::steady_clock::now();
        job_keep->done.store(true, std::memory_order_release);
        {
          std::lock_guard<std::mutex> lock(sync->mu);
          sync->cv.notify_all();
        }
      });
  return gen;
}

bool Scene3dPrepMailbox::drain(bool wait_if_empty,
                               Scene3dPrepAction* drained_action,
                               int64_t* overlap_ms) {
  bool waited = false;
  std::unique_lock<std::mutex> lock(sync_->mu);
  if (wait_if_empty) {
    if (pending_ && !pending_->done.load(std::memory_order_acquire)) {
      sync_->cv.wait(lock, [&] {
        return !pending_ ||
               pending_->done.load(std::memory_order_acquire);
      });
      waited = true;
    }
  }
  if (!pending_ || !pending_->done.load(std::memory_order_acquire)) {
    return false;
  }
  if (overlap_ms) {
    if (waited) {
      *overlap_ms = 0;
    } else {
      *overlap_ms =
          steady_ms_between(pending_->posted_at, pending_->done_at);
    }
  }
  const Scene3dPrepAction action = pending_->action;
  sync_->published_prep_gen.store(pending_->gen, std::memory_order_release);
  sync_->prep_build_count.fetch_add(1, std::memory_order_acq_rel);
  pending_.reset();
  if (drained_action) {
    *drained_action = action;
  }
  return true;
}

}  // namespace detail
}  // namespace content
