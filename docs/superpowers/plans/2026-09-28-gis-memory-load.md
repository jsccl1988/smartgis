<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# GIS load path × base/memory (Batch3a) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Cut peak memory and allocator churn on OGR layer open by streaming ordered sink + recycling pipeline contexts, using `src/base/memory` scratch rules.

**Architecture:** Keep durable `MapFeature` / `LayerStore` on the default heap. Bound in-flight decoded `Out` with an ordered window inside `load_ogr_layer_pipeline`. Recycle `Ctx` via a mutex freelist. Decode workers clear TLS hybrid scratch on a cadence (not every feature). Amend living §Memory Batch3a (no new dated design).

**Tech Stack:** C++23, `base::execution::Pipeline`, `base::tls_memory_resource`, GDAL/OGR, GN/`build.bat`.

## Global Constraints

- No `base::mutex` invent — use `<mutex>` / `std::mutex`.
- No Qt; work on `master`; `out/` only.
- Living doc: amend `2026-09-14-base-root-hybrid-design.md` §Memory (Batch3a).
- Scratch vs durable: never put long-lived `MapFeature` bytes in monotonic Arena.
- `OGRGeometry::clone()` stays on GDAL heap (out of scope).
- Do not commit unless the user asks.

---

### File map

| File | Role |
| --- | --- |
| `src/gis/datasource/pipeline/feature_load_pipeline.h` | Ordered window + Ctx freelist |
| `src/gis/datasource/ogr/ogr_feature_codec.cc` | TLS clear cadence (every N features / worker) |
| `src/gis/datasource/pipeline/feature_load_pipeline_test.cc` | Order + window smoke (OGR Memory driver) |
| `src/gis/datasource/ogr/BUILD.gn` | Wire test target |
| `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md` | §Memory Batch3a |
| `docs/superpowers/README.md` | Link plan under base row |

---

### Task 1: Spec amendment (Batch3a)

**Files:**
- Modify: `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md` §Memory
- Modify: `docs/superpowers/README.md` Active row for base
- Modify: `docs/superpowers/plans/2026-09-28-base-memory.md` (pointer to Batch3a plan)

**Interfaces:**
- Produces: Written Batch3a contract (ordered window, Ctx freelist, TLS cadence, durable rule)

- [x] **Step 1: Amend §Memory Adoption batches**

Replace / extend batch list with:

```markdown
3. GIS OGR load (Batch3a) — `load_ogr_layer_pipeline` ordered window + Ctx freelist; decode TLS clear cadence. Plan: [`../plans/2026-09-28-gis-memory-load.md`](../plans/2026-09-28-gis-memory-load.md).
4. Diagnostic Tools **Memory** tab — `sample_memory_counters_to_process_trace()` + stats strip (`AllocationTracker` optional).
```

Keep batches 1–2 as landed.

- [x] **Step 2: Link from README + base-memory plan footer**

Active table plan cell adds `2026-09-28-gis-memory-load.md`.  
`2026-09-28-base-memory.md` add note: Batch1/2 landed; Batch3a = gis-memory-load plan.

---

### Task 2: Ordered window + Ctx freelist in pipeline

**Files:**
- Modify: `src/gis/datasource/pipeline/feature_load_pipeline.h`
- Test: `src/gis/datasource/pipeline/feature_load_pipeline_test.cc` (Task 3)

**Interfaces:**
- Consumes: `base::execution::Pipeline`, OGR `GetNextFeature`
- Produces:
  - `FeatureLoadOptions::ordered_window` — `size_t`; `0` = legacy (buffer all, sink after `wait`); default `256`
  - In-pipeline ordered `sink(Out&&)` when window > 0
  - Ctx recycled via freelist (`new` only on miss)

- [x] **Step 1: Extend options**

```cpp
struct FeatureLoadOptions {
  size_t max_features = 0;
  size_t decode_workers = 0;
  size_t serial_threshold = 64;
  // 0 = buffer all decoded Out then sink after wait (legacy).
  // >0 = max in-flight ready slots; produce blocks until sink drains.
  size_t ordered_window = 256;
};
```

- [x] **Step 2: Ctx freelist helpers (file-local in header)**

```cpp
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
    p->feat = nullptr;
    p->ok = false;
    p->out = Out{};  // Out is Ctx::out type — reset in caller
    std::lock_guard<std::mutex> lock(mu);
    free_list.push_back(p);
  }
};
```

Wire produce to `acquire()` instead of Pipeline’s `new Context()` — **note:** `Pipeline` always `new Context()` in stage0. So freelist must live **inside** the produce handler by replacing the pointer:

Actually stage0 does `ctx = new Context()` before calling the handler. The current sink does `delete &ctx`. To recycle without changing `Pipeline`:

1. Produce handler: leave Pipeline’s `new`; on COMPLETE path Pipeline deletes.
2. Sink: instead of `delete &ctx`, push to freelist — **but** next produce still `new`s. Leak-free only if we change Pipeline.

**Chosen approach (minimal Pipeline change):** add optional freelist hook **only in GIS pipeline** by not using Pipeline’s stage0 allocation path… Pipeline always news.

**Practical fix for this task:** extend `base::execution::Pipeline` / `Stage` with optional `Context* (*acquire)()` / `void (*release)(Context*)` — too broad.

**GIS-only fix:** keep Pipeline `new`/`delete` for Ctx **out of Batch3a scope**; deliver **ordered window** as the memory win. Defer Ctx freelist to a follow-up that teaches Pipeline a custom allocator, **or** patch stage0 locally.

**Ship ordered window + Ctx freelist.** Pipeline gains `set_context_hooks(acquire, release)`; Stage auto-releases on CONSUMED/COMPLETE/FAILED (handlers must not `delete &ctx`).

- [x] **Step 3: Ordered window algorithm**

Replace post-`wait` sink loop when `ordered_window > 0`:

```cpp
std::mutex slots_mu;
std::condition_variable slots_cv;
std::vector<Slot> slots;
size_t next_sink = 0;

// produce:
{
  std::unique_lock<std::mutex> lock(slots_mu);
  slots_cv.wait(lock, [&] {
    return (slots.size() - next_sink) < opts.ordered_window;
  });
  index = slots.size();
  slots.emplace_back();
}
// ... set ctx ...

// sink stage (still CONSUMED + delete &ctx):
{
  std::lock_guard<std::mutex> lock(slots_mu);
  slots[ctx.index].ok = ctx.ok;
  if (ctx.ok) {
    slots[ctx.index].value = std::move(ctx.out);
  }
  slots[ctx.index].ready = true;
  while (next_sink < slots.size() && slots[next_sink].ready) {
    if (slots[next_sink].ok) {
      sink(std::move(slots[next_sink].value));
    }
    slots[next_sink] = Slot{};  // drop Out storage
    ++next_sink;
  }
  slots_cv.notify_all();
}
delete &ctx;
return Status::CONSUMED;

// after wait: assert next_sink == slots.size() (or drain remainder)
```

When `ordered_window == 0`, keep legacy: no `ready` flush in sink stage; after `wait`, iterate `slots` and `sink`.

- [x] **Step 4: Serial path unchanged**

`load_ogr_layer_serial` already sinks one-by-one — no change.

---

### Task 3: Pipeline unit test (OGR Memory)

**Files:**
- Create: `src/gis/datasource/pipeline/feature_load_pipeline_test.cc`
- Modify: `src/gis/datasource/ogr/BUILD.gn`

**Interfaces:**
- Consumes: `load_ogr_layer_pipeline<int>`
- Produces: `feature_load_pipeline_test` GN target

- [x] **Step 1: Add test target**

```gn
test("feature_load_pipeline_test") {
  output_name = "feature_load_pipeline_test"
  sources = [ "../pipeline/feature_load_pipeline_test.cc" ]
  include_dirs += [ "//src" ]
  deps = [
    ":ogr_codec",
    "//third_party:gdal",
  ]
}
```

- [x] **Step 2: Write test**

Create Memory datasource with 500 point features (FID 0..499). Decode = copy FID into `int`. Sink pushes to `std::vector<int>`. Run with `ordered_window = 32`, `serial_threshold = 0` (force pipeline). Assert sink order equals 0..N-1 and size == N. Second run with `ordered_window = 0` (legacy) same assert.

- [x] **Step 3: Build and run**

```bat
.\build.bat feature_load_pipeline_test
.\out\feature_load_pipeline_test.exe
```

Expected: prints `feature_load_pipeline_test OK` and exit 0.

---

### Task 4: Decode TLS clear cadence

**Files:**
- Modify: `src/gis/datasource/ogr/ogr_feature_codec.cc` (`decode_ogr_geometry`)

**Interfaces:**
- Consumes: `base::tls_memory_resource()`
- Produces: clear every 64 calls per thread (thread_local counter), still clear when resource grows past a soft watermark if API allows — else cadence only

- [x] **Step 1: Replace per-call clear**

```cpp
thread_local size_t tls_decode_scratch_ticks = 0;
if (base::MemoryResource* tls = base::tls_memory_resource()) {
  if ((++tls_decode_scratch_ticks % 64) == 0) {
    tls->clear(64 * 1024);
  }
}
```

- [x] **Step 2: Smoke existing codec tests**

```bat
.\build.bat sde_gdal_test
.\out\sde_gdal_test.exe
```

Expected: existing asserts still pass (or same pre-existing skips).

---

### Task 5: Verify ingest path still green

**Files:** none (verify only)

- [x] **Step 1: Build content / map smoke if available**

```bat
.\build.bat feature_load_pipeline_test
.\build.bat gis_scene_test
```

Run both exes; both exit 0 (gis_scene_test may be heavy — if missing target, run `sde_gdal_test` instead).

---

## Self-review

| Spec Batch3a item | Task |
| --- | --- |
| Ordered window streaming sink | Task 2 |
| Durable vs scratch rule | Task 1 (doc) + Task 2 (no Arena for MapFeature) |
| TLS cadence | Task 4 |
| Ctx freelist | Landed: `Pipeline::set_context_hooks` + GIS freelist |
| Tessellate scratch (3c) | Landed: TLS clear + thread_local ObjectPool |
| Durable Feature ObjectPool | Explicitly rejected (capacity lost on move into LayerStore) |
| Tests | Task 3 + 5 + `execution_test` hooks + `tessellate_style_test` |

Placeholder scan: none intentional. Ctx freelist deferred is documented, not TBD.
