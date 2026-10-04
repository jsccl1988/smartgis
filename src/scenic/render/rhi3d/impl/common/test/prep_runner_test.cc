// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <atomic>
#include <cstdio>
#include <vector>

#include "scenic/render/rhi3d/impl/common/frame/prep_runner.h"

namespace {

int g_fails = 0;

void expect(bool cond, const char* msg) {
  if (!cond) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

void test_prep_runner_drain() {
  scenic::detail::Rhi3dPrepRunner prep;
  prep.ensure_workers(4);
  expect(prep.worker_count() == 4, "4 workers");

  constexpr size_t kN = 64;
  std::vector<std::atomic<int>> hits(kN);
  for (size_t i = 0; i < kN; ++i) {
    hits[i].store(0);
  }

  prep.run_jobs(kN, [&](size_t i) { hits[i].fetch_add(1); });

  int sum = 0;
  for (size_t i = 0; i < kN; ++i) {
    sum += hits[i].load();
    expect(hits[i].load() == 1, "each job once");
  }
  expect(sum == static_cast<int>(kN), "all jobs ran");
  prep.shutdown();
}

void test_prep_runner_cancel() {
  scenic::detail::Rhi3dPrepRunner prep;
  prep.ensure_workers(2);
  std::atomic<int> ran{0};
  prep.request_cancel();
  prep.run_jobs(32, [&](size_t) { ran.fetch_add(1); });
  // Cancel before run_jobs clears via clear_cancel at start — so jobs run.
  // Request cancel mid-frame:
  prep.clear_cancel();
  ran.store(0);
  prep.run_jobs(8, [&](size_t) {
    prep.request_cancel();
    ran.fetch_add(1);
  });
  expect(ran.load() >= 1, "at least one job before/during cancel");
  prep.shutdown();
}

void test_worker_count_helper() {
  const int n = scenic::detail::rhi3d_prep_worker_count();
  expect(n >= 1 && n <= 4, "worker count in [1,4]");
}

}  // namespace

int main() {
  test_worker_count_helper();
  test_prep_runner_drain();
  test_prep_runner_cancel();
  if (g_fails != 0) {
    std::fprintf(stderr, "%d failure(s)\n", g_fails);
    return 1;
  }
  std::printf("rhi3d_prep_runner_test OK (workers=%d)\n",
              scenic::detail::rhi3d_prep_worker_count());
  return 0;
}
