// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/execution/execution_executor.h"
#include "base/execution/futures/combinators/async.h"
#include "base/execution/parallel/for.h"
#include "base/execution/pipeline/pipeline.h"

#include <atomic>
#include <cassert>
#include <cstdio>
#include <mutex>
#include <vector>

int main() {
  {
    base::execution::NThreadPoolExecutor pool(2);
    auto f = pool.execute([] { return 42; });
    assert(f.get() == 42);
  }

  {
    base::execution::NThreadPoolExecutor pool(4);
    constexpr int n = 1000;
    std::vector<int> data(n, 1);
    std::atomic<int> sum{0};
    base::execution::parallel_for(
        pool, 0, n, [&](int i) { sum.fetch_add(data[static_cast<size_t>(i)]); });
    assert(sum.load() == n);
  }

  {
    base::execution::NThreadPoolExecutor pool(2);
    auto f = base::execution::async(pool, [] { return 7; });
    auto g = base::execution::then(f, pool, [](int v) { return v * 3; });
    assert(g.get() == 21);
  }

  {
    // Produce → double → sink (CONSUMED deletes context).
    struct Ctx {
      int value = 0;
    };
    constexpr int kCount = 32;
    std::atomic<int> next{0};
    std::atomic<int> sum{0};
    using Status = base::execution::detail::Status;
    base::execution::Pipeline<Ctx> pipe({
        {1,
         [&](Ctx& ctx) -> Status {
           const int i = next.fetch_add(1);
           if (i >= kCount) {
             return Status::COMPLETE;
           }
           ctx.value = i + 1;
           return Status::SUCCESS;
         }},
        {2,
         [&](Ctx& ctx) -> Status {
           ctx.value *= 2;
           return Status::SUCCESS;
         }},
        {1,
         [&](Ctx& ctx) -> Status {
           sum.fetch_add(ctx.value);
           return Status::CONSUMED;
         }},
    });
    pipe.set_stage_names({"produce", "map", "sink"});
    pipe.run();
    pipe.wait();
    // 2*(1+...+32) = 2*32*33/2 = 1056
    assert(sum.load() == 1056);
  }

  {
    // Context freelist via set_context_hooks.
    struct Ctx {
      int value = 0;
    };
    std::mutex pool_mu;
    std::vector<Ctx*> free_list;
    size_t created = 0;
    auto acquire = [&]() -> Ctx* {
      std::lock_guard<std::mutex> lock(pool_mu);
      if (!free_list.empty()) {
        Ctx* p = free_list.back();
        free_list.pop_back();
        return p;
      }
      ++created;
      return new Ctx();
    };
    auto release = [&](Ctx* p) {
      if (!p) {
        return;
      }
      p->value = 0;
      std::lock_guard<std::mutex> lock(pool_mu);
      free_list.push_back(p);
    };
    constexpr int kCount = 64;
    std::atomic<int> next{0};
    std::atomic<int> sum{0};
    using Status = base::execution::detail::Status;
    base::execution::Pipeline<Ctx> pipe({
        {1,
         [&](Ctx& ctx) -> Status {
           const int i = next.fetch_add(1);
           if (i >= kCount) {
             return Status::COMPLETE;
           }
           ctx.value = 1;
           return Status::SUCCESS;
         }},
        {4,
         [&](Ctx& ctx) -> Status {
           ctx.value += 1;
           return Status::SUCCESS;
         }},
        {1,
         [&](Ctx& ctx) -> Status {
           sum.fetch_add(ctx.value);
           return Status::CONSUMED;
         }},
    });
    pipe.set_context_hooks(acquire, release);
    pipe.run();
    pipe.wait();
    assert(sum.load() == kCount * 2);
    // All contexts returned to the freelist (no leak); peak create may equal
    // kCount when stage queues are unbounded.
    assert(free_list.size() == created);
    assert(created > 0);
    for (Ctx* p : free_list) {
      delete p;
    }
  }

  std::puts("execution_test OK");
  return 0;
}
