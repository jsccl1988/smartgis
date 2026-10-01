// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef SMT_LEGACY_RENDER_RHI2D_MAP_PREP_PIPELINE_H_
#define SMT_LEGACY_RENDER_RHI2D_MAP_PREP_PIPELINE_H_

#include <algorithm>
#include <atomic>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "base/execution/pipeline/pipeline.h"

namespace render {
namespace detail {

// Parallel CPU prep (LP→DP + thin) then serial GDI play. HDC stays
// single-thread.
inline constexpr size_t kMinParallelFeatures = 16;
// Pipeline produce→map chunk size: one Context covers many features so queue
// overhead does not dominate china-scale layers (1k–3k feats).
inline constexpr size_t kPrepChunk = 48;

// Run prepare_at(i) for i in [0, job_count) via chunked Pipeline + freelist.
template <typename PrepareAt>
void run_chunked_prep_pipeline(size_t job_count, PrepareAt&& prepare_at) {
  if (job_count == 0) {
    return;
  }
  struct PrepCtx {
    size_t begin = 0;
    size_t end = 0;
  };
  std::atomic<size_t> next_chunk{0};
  const size_t n_chunks = (job_count + kPrepChunk - 1) / kPrepChunk;
  const size_t workers = (std::max)(
      size_t{1},
      (std::min)(n_chunks,
                 static_cast<size_t>(
                     (std::max)(1u, std::thread::hardware_concurrency()))));

  std::mutex pool_mu;
  std::vector<PrepCtx*> free_list;
  auto acquire = [&]() -> PrepCtx* {
    std::lock_guard<std::mutex> lock(pool_mu);
    if (!free_list.empty()) {
      PrepCtx* p = free_list.back();
      free_list.pop_back();
      return p;
    }
    return new PrepCtx();
  };
  auto release = [&](PrepCtx* p) {
    if (!p) {
      return;
    }
    std::lock_guard<std::mutex> lock(pool_mu);
    free_list.push_back(p);
  };

  using Status = base::execution::detail::Status;
  base::execution::Pipeline<PrepCtx> pipe({
      {1,
       [&](PrepCtx& ctx) -> Status {
         const size_t c = next_chunk.fetch_add(1, std::memory_order_relaxed);
         if (c >= n_chunks) {
           return Status::COMPLETE;
         }
         ctx.begin = c * kPrepChunk;
         ctx.end = (std::min)(job_count, ctx.begin + kPrepChunk);
         return Status::SUCCESS;
       }},
      {workers,
       [&](PrepCtx& ctx) -> Status {
         for (size_t i = ctx.begin; i < ctx.end; ++i) {
           prepare_at(i);
         }
         return Status::SUCCESS;
       }},
      {1, [&](PrepCtx&) -> Status { return Status::CONSUMED; }},
  });
  pipe.set_context_hooks(acquire, release);
  pipe.set_stage_names(
      {"gdi.prep.produce", "gdi.prep.map", "gdi.prep.sink"});
  pipe.run();
  pipe.wait();
  for (PrepCtx* p : free_list) {
    delete p;
  }
}

}  // namespace detail
}  // namespace render

#endif  // SMT_LEGACY_RENDER_RHI2D_MAP_PREP_PIPELINE_H_
