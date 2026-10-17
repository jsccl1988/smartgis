// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/memory/allocation_tracker.h"

#include <algorithm>
#include <cstdio>
#include <unordered_map>
#include <utility>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace base {
namespace {

struct FlameBuild {
  void* pc = nullptr;
  const char* type = nullptr;
  uint64_t bytes = 0;
  uint32_t count = 0;
  std::unordered_map<void*, int> by_pc;
  std::unordered_map<const char*, int> by_type;
  std::vector<FlameBuild> children;
};

void add_type_leaf(FlameBuild& node, const char* type, size_t size) {
  const auto it = node.by_type.find(type);
  if (it == node.by_type.end()) {
    FlameBuild leaf;
    leaf.type = type;
    leaf.bytes = size;
    leaf.count = 1;
    node.by_type.emplace(type, static_cast<int>(node.children.size()));
    node.children.push_back(std::move(leaf));
    return;
  }
  FlameBuild& leaf = node.children[static_cast<size_t>(it->second)];
  leaf.bytes += size;
  leaf.count += 1;
}

void insert_frame(FlameBuild& node, void* const* frames, int index,
                  const char* type, size_t size) {
  if (index < 0) {
    add_type_leaf(node, type, size);
    return;
  }
  void* pc = frames[index];
  const auto it = node.by_pc.find(pc);
  int slot = 0;
  if (it == node.by_pc.end()) {
    FlameBuild child;
    child.pc = pc;
    slot = static_cast<int>(node.children.size());
    node.by_pc.emplace(pc, slot);
    node.children.push_back(std::move(child));
  } else {
    slot = it->second;
  }
  FlameBuild& child = node.children[static_cast<size_t>(slot)];
  child.bytes += size;
  child.count += 1;
  insert_frame(child, frames, index - 1, type, size);
}

AllocationTracker::FlameNode freeze_node(FlameBuild& node) {
  AllocationTracker::FlameNode out;
  out.pc = node.pc;
  out.type = node.type;
  out.bytes = node.bytes;
  out.count = node.count;
  out.children.reserve(node.children.size());
  for (FlameBuild& child : node.children) {
    out.children.push_back(freeze_node(child));
  }
  std::sort(out.children.begin(), out.children.end(),
            [](const AllocationTracker::FlameNode& a,
               const AllocationTracker::FlameNode& b) {
              return a.bytes > b.bytes;
            });
  return out;
}

}  // namespace

int AllocationTracker::capture_alloc_stack(void** frames, int max_frames) {
  if (!frames || max_frames <= 0) {
    return 0;
  }
  // Skip this frame only. track_allocation is inline and may not be a frame.
  const USHORT n = CaptureStackBackTrace(
      1, static_cast<ULONG>(max_frames), frames, nullptr);
  return static_cast<int>(n);
}

const char* AllocationTracker::intern_locked(std::string_view name) {
  State& s = state();
  const std::string key(name);
  const auto it = s.intern_index.find(key);
  if (it != s.intern_index.end()) {
    return it->second;
  }
  auto owned = std::make_unique<std::string>(key);
  const char* ptr = owned->c_str();
  s.intern_index.emplace(*owned, ptr);
  s.intern_store.push_back(std::move(owned));
  return ptr;
}

const char* AllocationTracker::site_label_locked(const char* file, int line) {
  const char* base = file ? file : "?";
  for (const char* p = base; *p; ++p) {
    if (*p == '\\' || *p == '/') {
      base = p + 1;
    }
  }
  char buf[160];
  std::snprintf(buf, sizeof(buf), "%s:%d", base, line);
  return intern_locked(buf);
}

std::vector<AllocationTracker::FlameNode> AllocationTracker::flame_snapshot() {
  State& s = state();
  std::lock_guard<std::mutex> lock(s.mutex);
  if (s.allocations.empty()) {
    return {};
  }
  FlameBuild root;
  for (const auto& [ptr, info] : s.allocations) {
    (void)ptr;
    root.bytes += info.size;
    root.count += 1;
    insert_frame(root, info.frames, info.depth - 1, info.type, info.size);
  }
  return {freeze_node(root)};
}

}  // namespace base
