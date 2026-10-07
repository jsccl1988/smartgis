// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.
//
// mogu-style table load: produce (serial GetNextFeature) → decode (N) →
// ordered sink. Same OGRLayer must not be read from multiple workers.
// Produce clones each feature and DestroyFeature's the layer-owned pointer
// on that same thread; decode workers only see the clone (GDAL layer
// cursors are not safe to destroy off the GetNextFeature thread).
//
// When FeatureLoadOptions::ordered_window > 0, sink(Out&&) runs inside the
// pipeline sink stage as soon as consecutive indices are ready, so in-flight
// Out storage stays bounded. ordered_window == 0 keeps the legacy buffer-all
// then sink-after-wait behavior.
//
// Pipeline Context* are recycled via a mutex freelist (set_context_hooks).

#ifndef GIS_DATASOURCE_PIPELINE_FEATURE_LOAD_PIPELINE_H_
#define GIS_DATASOURCE_PIPELINE_FEATURE_LOAD_PIPELINE_H_

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

#include "base/execution/pipeline/pipeline.h"
#include "ogrsf_frmts.h"

namespace gis {
namespace datasource {

// Caps and worker hints for load_ogr_layer_pipeline.
struct FeatureLoadOptions {
  // 0 = unlimited (caller still may stop via decode/sink policy).
  size_t max_features = 0;
  // 0 = min(hw_concurrency, 8).
  size_t decode_workers = 0;
  // Layers with GetFeatureCount < this use a serial loop (no thread spawn).
  size_t serial_threshold = 64;
  // 0 = buffer all decoded Out then sink after wait (legacy).
  // >0 = max in-flight slot count; produce blocks until sink drains.
  size_t ordered_window = 256;
};

namespace detail {

inline size_t resolve_decode_workers(size_t hint) {
  if (hint > 0) {
    return hint;
  }
  const unsigned hw = std::thread::hardware_concurrency();
  const size_t n = hw == 0 ? size_t{2} : static_cast<size_t>(hw);
  return (std::min)(n, size_t{8});
}

template <typename Out, typename Decode, typename Sink>
bool load_ogr_layer_serial(OGRLayer* layer, Decode& decode, Sink& sink,
                           size_t max_features) {
  layer->ResetReading();
  size_t taken = 0;
  while (max_features == 0 || taken < max_features) {
    OGRFeature* feat = layer->GetNextFeature();
    if (!feat) {
      break;
    }
    Out out{};
    const bool ok = decode(feat, &out);
    OGRFeature::DestroyFeature(feat);
    if (!ok) {
      continue;
    }
    sink(std::move(out));
    ++taken;
  }
  return true;
}

template <typename Ctx>
struct CtxFreelist {
  std::mutex mu;
  std::vector<Ctx*> free_list;

  ~CtxFreelist() {
    for (Ctx* p : free_list) {
      delete p;
    }
  }

  Ctx* acquire() {
    std::lock_guard<std::mutex> lock(mu);
    if (!free_list.empty()) {
      Ctx* p = free_list.back();
      free_list.pop_back();
      return p;
    }
    return new Ctx();
  }

  void release(Ctx* p) {
    if (!p) {
      return;
    }
    p->index = 0;
    p->feat = nullptr;
    p->ok = false;
    // Prefer clear() so vector-like Out keeps capacity across recycles.
    if constexpr (requires(decltype(p->out)& o) { o.clear(); }) {
      p->out.clear();
    } else {
      p->out = {};
    }
    std::lock_guard<std::mutex> lock(mu);
    free_list.push_back(p);
  }
};

}  // namespace detail

// Decode: bool(OGRFeature* feat, Out* out) — owns nothing; feat valid only
// during the call. Sink: void(Out&&) invoked in GetNextFeature order.
// Returns false only when |layer| is null.
template <typename Out, typename Decode, typename Sink>
bool load_ogr_layer_pipeline(OGRLayer* layer, Decode decode, Sink sink,
                             const FeatureLoadOptions& opts = {}) {
  if (!layer) {
    return false;
  }

  const GIntBig counted = layer->GetFeatureCount(/*bForce=*/FALSE);
  if (counted >= 0 &&
      static_cast<size_t>(counted) < opts.serial_threshold) {
    return detail::load_ogr_layer_serial<Out>(layer, decode, sink,
                                              opts.max_features);
  }

  const bool stream_sink = opts.ordered_window > 0;

  struct Slot {
    Out value{};
    bool ok = false;
    bool ready = false;
  };
  struct Ctx {
    size_t index = 0;
    OGRFeature* feat = nullptr;
    Out out{};
    bool ok = false;
  };

  detail::CtxFreelist<Ctx> ctx_pool;
  std::mutex slots_mu;
  std::condition_variable slots_cv;
  std::vector<Slot> slots;
  size_t next_sink = 0;
  std::atomic<size_t> produced{0};
  const size_t max_features = opts.max_features;
  const size_t workers = detail::resolve_decode_workers(opts.decode_workers);
  const size_t ordered_window = opts.ordered_window;

  layer->ResetReading();

  using Status = base::execution::detail::Status;
  base::execution::Pipeline<Ctx> pipe({
      {1,
       [&](Ctx& ctx) -> Status {
         if (max_features > 0 &&
             produced.load(std::memory_order_relaxed) >= max_features) {
           return Status::COMPLETE;
         }
         OGRFeature* owned = nullptr;
         for (;;) {
           OGRFeature* feat = layer->GetNextFeature();
           if (!feat) {
             return Status::COMPLETE;
           }
           // Detach from the layer cursor before decode workers run.
           owned = feat->Clone();
           OGRFeature::DestroyFeature(feat);
           if (owned) {
             break;
           }
           // Clone failed — skip this row; do not enqueue an empty Ctx
           // (that used to SUCCESS with index=0 and corrupt slots[0]).
         }
         size_t index = 0;
         {
           std::unique_lock<std::mutex> lock(slots_mu);
           slots_cv.wait(lock, [&] {
             if (!stream_sink) {
               return true;
             }
             return (slots.size() - next_sink) < ordered_window;
           });
           index = slots.size();
           slots.emplace_back();
         }
         produced.fetch_add(1, std::memory_order_relaxed);
         ctx.index = index;
         ctx.feat = owned;
         ctx.out = Out{};
         ctx.ok = false;
         return Status::SUCCESS;
       }},
      {workers,
       [&](Ctx& ctx) -> Status {
         if (ctx.feat) {
           ctx.ok = decode(ctx.feat, &ctx.out);
           OGRFeature::DestroyFeature(ctx.feat);
           ctx.feat = nullptr;
         }
         return Status::SUCCESS;
       }},
      {1,
       [&](Ctx& ctx) -> Status {
         {
           std::lock_guard<std::mutex> lock(slots_mu);
           if (ctx.index < slots.size()) {
             slots[ctx.index].ok = ctx.ok;
             if (ctx.ok) {
               slots[ctx.index].value = std::move(ctx.out);
             }
             slots[ctx.index].ready = true;
             if (stream_sink) {
               while (next_sink < slots.size() && slots[next_sink].ready) {
                 if (slots[next_sink].ok) {
                   sink(std::move(slots[next_sink].value));
                 }
                 slots[next_sink] = Slot{};
                 ++next_sink;
               }
               slots_cv.notify_all();
             }
           }
         }
         // Stage releases via set_context_hooks (freelist).
         return Status::CONSUMED;
       }},
  });
  pipe.set_context_hooks([&]() { return ctx_pool.acquire(); },
                         [&](Ctx* p) { ctx_pool.release(p); });
  pipe.set_stage_names(
      {"ogr.produce", "ogr.decode", "ogr.sink"});
  pipe.run();
  pipe.wait();

  if (!stream_sink) {
    for (Slot& slot : slots) {
      if (slot.ok) {
        sink(std::move(slot.value));
      }
    }
  } else {
    // Drain any trailing ready slots (should already be empty).
    std::lock_guard<std::mutex> lock(slots_mu);
    while (next_sink < slots.size() && slots[next_sink].ready) {
      if (slots[next_sink].ok) {
        sink(std::move(slots[next_sink].value));
      }
      slots[next_sink] = Slot{};
      ++next_sink;
    }
  }
  return true;
}

}  // namespace datasource
}  // namespace gis

#endif  // GIS_DATASOURCE_PIPELINE_FEATURE_LOAD_PIPELINE_H_
