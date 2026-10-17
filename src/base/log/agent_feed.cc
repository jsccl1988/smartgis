// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/log/agent_feed.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <intrin.h>
#include <utility>

#include "base/concurrency/hazard_ptr.h"
#include "base/core/log.h"

namespace base {
namespace {

constexpr std::size_t kCapacity = 256;
constexpr int kSubSlots = 16;
constexpr int kReadAttempts = 64;

struct Record {
  std::uint64_t ticket = 0;
  AgentFeedEntry entry;
};

using SubFn = std::function<void(const AgentFeedEntry&)>;

void cpu_pause() {
  _mm_pause();
}

}  // namespace

struct AgentFeed::Impl {
  // seq starts at the slot index. A writer that owns ticket n waits until
  // seq == n, publishes the record, then stores seq = n + capacity so the
  // next writer for this slot (n + capacity) can enter.
  struct alignas(64) Slot {
    std::atomic<std::uint64_t> seq{0};
    std::atomic<Record*> ptr{nullptr};
  };

  struct alignas(64) SubSlot {
    std::atomic<std::uint64_t> id{0};
    std::atomic<SubFn*> fn{nullptr};
  };

  Impl() {
    for (std::size_t i = 0; i < kCapacity; ++i) {
      slots[i].seq.store(i, std::memory_order_relaxed);
    }
  }

  std::array<Slot, kCapacity> slots{};
  std::array<SubSlot, kSubSlots> subs{};
  std::atomic<std::uint64_t> next_ticket{0};
  std::atomic<std::uint64_t> min_keep{0};
  std::atomic<std::uint64_t> next_sub_id{1};
};

AgentFeed::AgentFeed() : impl_(new Impl) {}

AgentFeed::~AgentFeed() {
  if (impl_ != nullptr) {
    hazard_ptr<Record>::reclaim();
    for (auto& slot : impl_->slots) {
      delete slot.ptr.exchange(nullptr, std::memory_order_acq_rel);
    }
    hazard_ptr<SubFn>::reclaim();
    for (auto& sub : impl_->subs) {
      delete sub.fn.exchange(nullptr, std::memory_order_acq_rel);
    }
  }
  delete impl_;
  impl_ = nullptr;
}

void AgentFeed::append(AgentFeedKind kind, std::string text) {
  append(kind, std::move(text), std::string());
}

void AgentFeed::append(AgentFeedKind kind, std::string text,
                       std::string session) {
  if (!impl_ || text.empty()) {
    return;
  }
  auto* rec = new Record;
  rec->entry.kind = kind;
  rec->entry.timestamp = detail::log_timestamp_cached();
  rec->entry.text = std::move(text);
  rec->entry.session = std::move(session);
  const AgentFeedEntry published = rec->entry;

  const std::uint64_t n =
      impl_->next_ticket.fetch_add(1, std::memory_order_acq_rel);
  rec->ticket = n;
  Impl::Slot& slot = impl_->slots[n % kCapacity];
  while (slot.seq.load(std::memory_order_acquire) != n) {
    cpu_pause();
  }
  hazard_ptr<Record>::update(slot.ptr, rec);
  slot.seq.store(n + kCapacity, std::memory_order_release);

  for (Impl::SubSlot& sub : impl_->subs) {
    auto holder = hazard_ptr<SubFn>::acquire(sub.fn);
    if (holder) {
      (*holder)(published);
    }
  }
}

void AgentFeed::clear() {
  if (!impl_) {
    return;
  }
  const std::uint64_t next =
      impl_->next_ticket.load(std::memory_order_acquire);
  impl_->min_keep.store(next, std::memory_order_release);
}

std::vector<AgentFeedEntry> AgentFeed::snapshot() const {
  std::vector<AgentFeedEntry> out;
  if (!impl_) {
    return out;
  }
  const std::uint64_t keep =
      impl_->min_keep.load(std::memory_order_acquire);
  std::vector<std::pair<std::uint64_t, AgentFeedEntry>> found;
  found.reserve(kCapacity);
  for (std::size_t i = 0; i < kCapacity; ++i) {
    Impl::Slot& slot = impl_->slots[i];
    for (int attempt = 0; attempt < kReadAttempts; ++attempt) {
      const std::uint64_t s1 = slot.seq.load(std::memory_order_acquire);
      if (s1 == i) {
        break;
      }
      auto holder = hazard_ptr<Record>::acquire(slot.ptr);
      const std::uint64_t s2 = slot.seq.load(std::memory_order_acquire);
      if (!holder || s1 != s2 || holder->ticket + kCapacity != s2) {
        cpu_pause();
        continue;
      }
      if (holder->ticket >= keep) {
        found.emplace_back(holder->ticket, holder->entry);
      }
      break;
    }
  }
  std::sort(found.begin(), found.end(),
            [](const auto& a, const auto& b) { return a.first < b.first; });
  out.reserve(found.size());
  for (auto& item : found) {
    out.push_back(std::move(item.second));
  }
  return out;
}

std::size_t AgentFeed::size() const {
  if (!impl_) {
    return 0;
  }
  const std::uint64_t keep =
      impl_->min_keep.load(std::memory_order_acquire);
  std::size_t n = 0;
  for (std::size_t i = 0; i < kCapacity; ++i) {
    const std::uint64_t seq =
        impl_->slots[i].seq.load(std::memory_order_acquire);
    if (seq == i) {
      continue;
    }
    const std::uint64_t ticket = seq - kCapacity;
    if (ticket >= keep) {
      ++n;
    }
  }
  return n;
}

std::uint64_t AgentFeed::subscribe(Subscriber cb) {
  if (!impl_ || !cb) {
    return 0;
  }
  auto* fn = new SubFn(std::move(cb));
  const std::uint64_t id =
      impl_->next_sub_id.fetch_add(1, std::memory_order_relaxed);
  for (Impl::SubSlot& sub : impl_->subs) {
    std::uint64_t expected = 0;
    if (sub.id.compare_exchange_strong(expected, id, std::memory_order_acq_rel,
                                       std::memory_order_acquire)) {
      sub.fn.store(fn, std::memory_order_release);
      return id;
    }
  }
  delete fn;
  return 0;
}

void AgentFeed::unsubscribe(std::uint64_t id) {
  if (!impl_ || id == 0) {
    return;
  }
  for (Impl::SubSlot& sub : impl_->subs) {
    if (sub.id.load(std::memory_order_acquire) != id) {
      continue;
    }
    hazard_ptr<SubFn>::update(sub.fn, nullptr);
    sub.id.store(0, std::memory_order_release);
    return;
  }
}

AgentFeed& agent_feed() {
  static AgentFeed feed;
  return feed;
}

}  // namespace base
