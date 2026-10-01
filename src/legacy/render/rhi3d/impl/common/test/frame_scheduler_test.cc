// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <atomic>
#include <cstdio>

#include "legacy/render/rhi3d/impl/common/frame/scheduler.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_scheduler_submit_and_abort() {
  render::detail::Rhi3dFrameScheduler sched;
  std::atomic<int> paints{0};
  sched.set_paint_fn([&]() { ++paints; });
  sched.start();

  render::detail::Rhi3dFrameRequest req{};
  req.yaw = 1.f;
  sched.set_request(req);
  sched.submit();
  expect(sched.wait_idle(5000), "scheduler idle");
  expect(paints.load() == 1, "one paint");
  expect(sched.request().yaw == 1.f, "request yaw applied");

  const uint64_t gen = sched.job_generation();
  sched.mark_published(gen);
  expect(sched.published_generation() == gen, "published gen");
  expect(!sched.should_abort(gen), "current gen not aborted");
  sched.cancel();
  expect(sched.should_abort(gen), "cancel aborts gen");

  (void)sched.shutdown();
}

void test_scheduler_coalesce_pending() {
  render::detail::Rhi3dFrameScheduler sched;
  std::atomic<int> paints{0};
  std::atomic<float> last_yaw{0.f};
  sched.set_paint_fn([&]() {
    last_yaw.store(sched.request().yaw, std::memory_order_relaxed);
    ++paints;
    ::Sleep(50);
  });
  sched.start();

  render::detail::Rhi3dFrameRequest a{};
  a.yaw = 2.f;
  sched.stage_request(a);
  sched.submit();

  render::detail::Rhi3dFrameRequest b{};
  b.yaw = 3.f;
  // While busy, stage should coalesce and cancel in-flight.
  for (int i = 0; i < 200 && !sched.is_busy(); ++i) {
    ::Sleep(1);
  }
  sched.stage_request(b);
  expect(sched.wait_idle(5000), "coalesce idle");
  expect(paints.load() >= 1, "at least one paint");
  expect(last_yaw.load() == 3.f, "coalesced yaw wins");

  (void)sched.shutdown();
}

}  // namespace

int main() {
  test_scheduler_submit_and_abort();
  test_scheduler_coalesce_pending();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("rhi3d_frame_scheduler_test OK\n");
  return 0;
}
