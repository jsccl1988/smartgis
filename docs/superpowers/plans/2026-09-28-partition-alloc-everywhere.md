<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# PartitionAlloc-Everywhere (PA-E) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Align process-heap replacement with Chromium PartitionAlloc-Everywhere + allocator shim; never enable product PA-E until Windows multi-DLL and compiler gates pass.

**Architecture:** Mirror Chromium GN args (`use_partition_alloc`, `use_allocator_shim`, `use_partition_alloc_as_malloc`) defaulting **off**. Vendor Chromium standalone PartitionAlloc (`build_with_chromium = false` + `//build_overrides/partition_alloc.gni`). Prove on a single-exe smoke target before any product DLL wiring. Product multi-DLL path requires a shared `allocator_shim` owned by every `shared_library` and `executable` (Chromium component rule).

**Tech Stack:** Chromium PartitionAlloc (standalone git), GN/Ninja (`build.bat`), MSVC today / possible clang-cl for PA; C++20 floor for PA (product stays C++23).

**Living spec:** [`../specs/2026-09-14-base-root-hybrid-design.md`](../specs/2026-09-14-base-root-hybrid-design.md) §Process malloc / PartitionAlloc-Everywhere.

## Global Constraints

- Stay on `master`; `out/` only; no Qt; comments English; no `base::mutex`.
- **Reject** tcmalloc / gperftools / mimalloc / jemalloc as process malloc.
- §Memory (`src/base/memory`) stays orthogonal (explicit PMR pools).
- Windows Debug CRT: PA-E **off** (Chromium).
- Sanitizers: shim **off**.
- Product DLL stems (`base` / `gis` / `render` / `content` / `tool` / …) are component-like — do not turn on PA-E for them until Task 5.
- Do not treat `USE_TCMALLOC` or `//build/config/posix:gperftools` as enabled.

### Dependency graph (target)

```
[optional] use_partition_alloc_as_malloc
        │
        ▼
┌───────────────────────┐
│ allocator_shim        │  ← single heap owner (static in Phase 2 exe,
│  (PA dispatch)        │     shared DLL in Phase 3 product graph)
└───────────┬───────────┘
            │
            ▼
┌───────────────────────┐
│ partition_alloc       │  ← //third_party/.src/partition_alloc (pin)
└───────────────────────┘
            ▲
            │ deps only when flags true
┌───────────┴───────────┐
│ Phase 2: pae_smoke.exe│  single executable, no product DLLs
│ Phase 3: EVERY        │  shared_library + executable default deps
│   product DLL + exe   │
└───────────────────────┘
```

### Compiler gate (must resolve in Task 1)

Chromium `partition_alloc.gni` typically gates `use_partition_alloc` on **`is_clang_or_gcc`**. SmartGIS Windows default toolchain is **MSVC** (`//build/toolchain/win:msvc`). Embedder docs (`external_builds.md`) note PDFium/MSVC work is ongoing (no inline asm on MSVC x64).

**Decision rule after Task 1 spike:**

| Outcome | Action |
| --- | --- |
| PA compiles + links under MSVC for smoke | Proceed Task 2–5 on MSVC |
| PA requires clang / clang-cl | Document clang-cl (or Chromium clang) as **required** for PA-enabled configs; keep default product builds on MSVC with PA-E **off** |
| Neither path viable soon | Stop after Task 1; leave flags permanently default-off; as-built note in `docs/build/` |

Do **not** invent a third allocator while blocked.

---

### Task 1: Compiler / pin feasibility spike

**Files:**
- Create: `docs/build/partition-alloc.md` (as-built notes: pin URL, rev, MSVC vs clang-cl result)
- Modify: `docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md` §PA-E (record spike outcome under Phases)
- Optional scratch only under `.tmp/` — do not commit failed trees

**Interfaces:**
- Consumes: Chromium standalone PA repo  
  `https://chromium.googlesource.com/chromium/src/base/allocator/partition_allocator.git`  
  and `external_builds.md` (`build_with_chromium = false`, `//build_overrides/partition_alloc.gni`)
- Produces: Written decision: `pa_msvc_supported = true|false`, `pa_requires_clang_cl = true|false`

- [ ] **Step 1: Read Chromium embedder docs**

Open and skim:

- `https://chromium.googlesource.com/chromium/src/+/HEAD/base/allocator/partition_allocator/external_builds.md`
- `build_config.md` in the same tree (GN args semantics)

Record required override keys from Chromium `build_overrides/partition_alloc.gni` (copy names into Task 2 stub).

- [ ] **Step 2: Shallow clone for inspection (not yet manifest pin)**

```bat
git clone --depth 1 https://chromium.googlesource.com/chromium/src/base/allocator/partition_allocator.git .tmp/partition_alloc_probe
```

Inspect:

- Root `BUILD.gn` / `partition_alloc.gni` for `is_clang_or_gcc` / `COMPILER_MSVC`
- Whether Windows shim sources exist under `src/partition_alloc/shim/`

- [ ] **Step 3: Attempt minimal MSVC compile of PA library only**

Prefer a throwaway GN leaf under `.tmp` or a one-off `cl` smoke if GN integration is too heavy for the spike. Goal: **one** `partition_alloc` static lib object set builds, or a clear first error.

Run (adjust once probe BUILD is known):

```bat
.\build.bat
```

If no GN target yet, document the **first** MSVC error (inline asm, missing chromium macros, etc.) in `docs/build/partition-alloc.md`.

- [ ] **Step 4: Lock decision in living § + as-built stub**

Write `docs/build/partition-alloc.md` with:

| Field | Value |
| --- | --- |
| Standalone repo | URL above |
| Probe rev | `git rev-parse HEAD` from clone |
| MSVC result | pass / fail + one-line cause |
| clang-cl path | required / not required |
| PA-E product default | remains **false** |

Amend §PA-E Phase table with “Task 1 outcome: …”.

- [ ] **Step 5: Stop or continue**

If neither MSVC nor an agreed clang-cl product path exists, **stop the plan here** (do not vendor). Otherwise continue Task 2.

---

### Task 2: GN args + build_overrides stubs (defaults off)

**Files:**
- Create: `build_overrides/partition_alloc.gni`
- Create: `build/config/allocator.gni`
- Modify: `build/BUILDCONFIG.gn` (import allocator.gni after platform defs)
- Modify: living §PA-E (point at these files)

**Interfaces:**
- Consumes: Task 1 override key list; Chromium default semantics
- Produces: GN args always safe for current product graph

- [ ] **Step 1: Add `build/config/allocator.gni`**

```gn
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Chromium-aligned allocator flags. Defaults keep system CRT.
# Do not enable use_partition_alloc_as_malloc for product DLLs until
# §PA-E Phase 3 gates (shared allocator_shim + every module deps).

declare_args() {
  # Library available (headers / link). Does not replace malloc by itself.
  use_partition_alloc = false

  # Route malloc/new through allocator_shim.
  use_allocator_shim = false

  # PA-E: shim dispatches to PartitionAlloc.
  use_partition_alloc_as_malloc = false
}

# Chromium-shaped safety: never claim PA-E without both prerequisites.
if (!use_partition_alloc || !use_allocator_shim) {
  use_partition_alloc_as_malloc = false
}

# Windows Debug CRT: force PA-E off (Chromium).
if (is_win && is_debug) {
  use_partition_alloc_as_malloc = false
}

# Sanitizers replace the allocator.
if (is_asan || is_lsan || is_msan || is_tsan) {
  use_allocator_shim = false
  use_partition_alloc_as_malloc = false
}
```

- [ ] **Step 2: Add minimal `build_overrides/partition_alloc.gni`**

Copy Chromium embedder override **shape** from the probe (Task 1). At minimum provide:

```gn
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
#
# Expected by PartitionAlloc at //build_overrides/partition_alloc.gni
# when build_with_chromium = false. Values must stay conservative.

# Defaults: PA present only when explicitly enabled; never PA-E by default.
use_partition_alloc_as_malloc_default = false
use_allocator_shim_default = false

# Fill remaining keys from Chromium's build_overrides/partition_alloc.gni
# after Task 1 probe (keep BackupRefPtr / dangling checks off unless needed).
```

Set `build_with_chromium = false` wherever PA’s BUILD expects it (often via override or root args) — match probe, do not invent.

- [ ] **Step 3: Import from `BUILDCONFIG.gn`**

After `is_win` / sanitizer args exist, add:

```gn
import("//build/config/allocator.gni")
```

- [ ] **Step 4: Verify defaults**

```bat
gn gen out --root=./ --args="is_debug=true is_build_third_party=false"
gn args out --list=use_partition_alloc_as_malloc
```

Expected: `use_partition_alloc_as_malloc` is **false**. Full `build.bat` still green with no PA linked.

- [ ] **Step 5: Commit** (only when user asks)

```bat
git add build/config/allocator.gni build_overrides/partition_alloc.gni build/BUILDCONFIG.gn docs/build/partition-alloc.md docs/superpowers/specs/2026-09-14-base-root-hybrid-design.md docs/superpowers/plans/2026-09-28-partition-alloc-everywhere.md docs/superpowers/README.md
git commit -m "docs/build: add Chromium-aligned PA-E GN stubs (default off)"
```

---

### Task 3: Manifest pin + thin GN forwarder (library only, not malloc)

**Files:**
- Modify: `third_party/manifest.json` (add `partition_alloc` package)
- Create: `third_party/partition_alloc/BUILD.gn` (forwarder / wrap)
- Modify: `third_party/BUILD.gn` + `third_party/gn/BUILD.gn` (group)
- Sources: `third_party/.src/partition_alloc` via existing fetch path (`build.bat t` / batch)

**Interfaces:**
- Consumes: Task 1 pin rev; Task 2 overrides
- Produces: `//third_party:partition_alloc` usable **only when** `use_partition_alloc=true`

- [ ] **Step 1: Add manifest entry**

```json
{
  "name": "partition_alloc",
  "git_url": "https://chromium.googlesource.com/chromium/src/base/allocator/partition_allocator.git",
  "git_url_fallbacks": [],
  "git_ref": "<REV_FROM_TASK_1>",
  "install_skip": true,
  "note": "Chromium standalone PartitionAlloc. build_with_chromium=false. Not process malloc until use_partition_alloc_as_malloc (Phase 3 gates)."
}
```

Fetch into `third_party/.src/partition_alloc` per repo third_party tooling.

- [ ] **Step 2: Wire GN so default `all` does not compile PA**

```gn
# third_party/partition_alloc/BUILD.gn (sketch — match probe layout)
import("//build/config/allocator.gni")

group("partition_alloc") {
  if (use_partition_alloc) {
    public_deps = [ # path into .src BUILD after probe
    ]
  }
}
```

Default product `//src:src_all` must **not** depend on this group until Phase 3.

- [ ] **Step 3: Build library-only with explicit args**

```bat
gn gen out --args="is_debug=false use_partition_alloc=true use_allocator_shim=false use_partition_alloc_as_malloc=false"
ninja -C out <pa_library_target>
```

Expected: PA static lib builds; no malloc override; product exes unchanged.

---

### Task 4: Phase 2 smoke exe (single binary PA-E)

**Files:**
- Create: `src/base/allocator/pae_smoke_main.cc`
- Create: `src/base/allocator/BUILD.gn` (`pae_smoke` executable, `testonly`)
- Modify: root `BUILD.gn` or `test_shell` only if explicitly desired (prefer opt-in target)

**Interfaces:**
- Consumes: `use_partition_alloc=true`, `use_allocator_shim=true`, `use_partition_alloc_as_malloc=true` on a **static** exe with **no** product DLL deps
- Produces: `out/pae_smoke.exe` (name may gain `_d` only if templates force it — prefer console test without `_d` confusion)

- [ ] **Step 1: Write smoke main**

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main() {
  constexpr size_t kN = 64 * 1024;
  void* p = std::malloc(kN);
  if (!p) {
    return 1;
  }
  std::memset(p, 0xA5, kN);
  std::free(p);
  void* q = ::operator new(1024);
  ::operator delete(q);
  std::puts("pae_smoke ok");
  return 0;
}
```

- [ ] **Step 2: GN executable deps shim + PA only when PA-E args on**

Link shim the Chromium way for **static** Windows (not component): force reference so the linker keeps overrides (match Chromium `/INCLUDE` symbols from shim docs once probe lists them).

- [ ] **Step 3: Run**

```bat
gn gen out --args="is_debug=false use_partition_alloc=true use_allocator_shim=true use_partition_alloc_as_malloc=true"
ninja -C out pae_smoke
out\pae_smoke.exe
```

Expected stdout: `pae_smoke ok`. Debug+PA-E must remain unsupported (gen should force PA-E false or assert).

- [ ] **Step 4: Document how to confirm PA is active** (optional)

If Chromium exposes a PA version / stats API in the pin, call it once in smoke; otherwise note “link-time PA-E only” in `docs/build/partition-alloc.md`.

---

### Task 5: Phase 3 product multi-DLL (only after Task 4 green)

**Files:**
- Modify: `build/BUILDCONFIG.gn` `set_defaults("shared_library")` / `set_defaults("executable")` — **conditional** deps on allocator_shim when PA-E on
- Modify: product DLL graph only as required so **every** module that allocates links the **same** shim DLL
- Modify: `docs/build/partition-alloc.md` + §PA-E Phase 3 checkbox

**Interfaces:**
- Consumes: Chromium Windows component pattern (`allocator_shim` shared library; all DLLs+exe depend on it)
- Produces: Safe optional `use_partition_alloc_as_malloc=true` for Release non-sanitizer product builds

- [ ] **Step 1: Implement shared `allocator_shim` target** matching Chromium win-component split (do not partial-link shim into a single DLL).

- [ ] **Step 2: Default deps**

When `use_partition_alloc_as_malloc`:

- every `shared_library` and `executable` gets `deps += [ "//…:allocator_shim" ]`
- assert or document that FlyCube / third_party prebuilts either bypass (document risk) or also route through shim

- [ ] **Step 3: Enable only on Release** for a Views/self-test smoke; keep Debug off.

- [ ] **Step 4: `build.bat te` / e2e on that config before flipping any developer default.

**Stop condition:** If third_party closed-source DLLs allocate with CRT outside shim, either keep PA-E off for product or isolate those modules — do not ship mixed heaps.

---

### Task 6: Phase 4 hygiene

**Files:**
- Modify: `build/build_config.h` — remove or comment `USE_TCMALLOC` block; do not leave a false “enabled” signal
- Modify: `build/config/posix/BUILD.gn` — delete or comment unused `gperftools` config, or gate behind a never-on arg
- Modify: `docs/build/partition-alloc.md` as-built; archive note in §PA-E when product default remains off

- [ ] **Step 1: Neutralize `USE_TCMALLOC`**

```cpp
// Process heap: Chromium PA-E via GN (use_partition_alloc_as_malloc), not tcmalloc.
// See docs/build/partition-alloc.md and §Process malloc in base-root-hybrid.
```

- [ ] **Step 2: Remove dead posix gperftools libs list or mark `# Unused — do not link`.**

- [ ] **Step 3: Final verify default debug product build still has PA-E off and green `build.bat`.

---

## Spec coverage (self-check)

| Spec item | Task |
| --- | --- |
| PA-E + shim only; reject tcmalloc/mimalloc/jemalloc | Global + Task 6 |
| Defaults off; Debug/sanitizer off | Task 2 |
| Single heap / shared shim for multi-DLL | Task 5 |
| Phase 0 design | Done (living §) |
| Phase 1 pin + GN + graph | Task 1–3 |
| Phase 2 single exe | Task 4 |
| Phase 3 product DLL | Task 5 |
| Phase 4 USE_TCMALLOC / gperftools cleanup | Task 6 |
| Orthogonal §Memory | Global (no change to `src/base/memory`) |

## Non-goals in this plan

- Replacing mogu `ObjectAllocator` / PMR with PA partitions
- Enabling Linux system `libtcmalloc*`
- Changing default `is_debug=true` developer args to PA-E on
