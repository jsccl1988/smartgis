// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_EXECUTION_EXECUTOR_DEVICE_RESIDENT_STORE_H
#define BASE_EXECUTION_EXECUTOR_DEVICE_RESIDENT_STORE_H

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <map>
#include <string>
#include <utility>

#include "base/execution/executor/device/device_column.h"

namespace base {
namespace execution {

// Identifies a resident DeviceColumn; version changes replace the buffer.
struct ResidentKey {
  std::string ns;
  std::string name;
  std::uint64_t version = 0;
};

inline bool operator<(const ResidentKey& a, const ResidentKey& b) {
  if (a.ns != b.ns) {
    return a.ns < b.ns;
  }
  if (a.name != b.name) {
    return a.name < b.name;
  }
  return a.version < b.version;
}

// Host-backed pin/LRU cache for DeviceColumn. Constructor requires a byte cap.
class ResidentStore {
 public:
  explicit ResidentStore(std::size_t cap_bytes, std::size_t serve_reserve_bytes)
      : cap_(cap_bytes), serve_reserve_(serve_reserve_bytes) {}

  bool pin(const ResidentKey& key, DeviceColumn col, bool pinned) {
    const std::size_t nbytes = column_bytes(col);

    if (auto it = entries_.find(key); it != entries_.end()) {
      drop_entry(it);
    }

    std::size_t new_pinned = pinned_bytes_ + (pinned ? nbytes : 0);
    std::size_t new_unpinned = unpinned_bytes_ + (pinned ? 0 : nbytes);
    // Evict until the admit rule holds. An unpinned insert that would fill
    // the cap exactly also turns over LRU (oldest unpinned goes first).
    while (needs_evict(new_pinned, new_unpinned, pinned) &&
           evict_lru_unpinned()) {
      new_pinned = pinned_bytes_ + (pinned ? nbytes : 0);
      new_unpinned = unpinned_bytes_ + (pinned ? 0 : nbytes);
    }
    if (!fits(new_pinned, new_unpinned)) {
      return false;
    }

    Entry e;
    e.col = std::move(col);
    e.pinned = pinned;
    e.nbytes = nbytes;
    if (!pinned) {
      lru_.push_back(key);
      e.lru_it = std::prev(lru_.end());
    }
    entries_.emplace(key, std::move(e));
    if (pinned) {
      pinned_bytes_ += nbytes;
    } else {
      unpinned_bytes_ += nbytes;
    }
    return true;
  }

  const DeviceColumn* lookup(const ResidentKey& key) const {
    auto it = entries_.find(key);
    return it == entries_.end() ? nullptr : &it->second.col;
  }

  std::size_t bytes() const { return pinned_bytes_ + unpinned_bytes_; }

 private:
  struct Entry {
    DeviceColumn col;
    bool pinned = false;
    std::size_t nbytes = 0;
    std::list<ResidentKey>::iterator lru_it;
  };

  static std::size_t column_bytes(const DeviceColumn& col) {
    return col.size() * sizeof(std::int64_t);
  }

  std::size_t unpinned_max() const {
    return serve_reserve_ >= cap_ ? 0 : (cap_ - serve_reserve_);
  }

  bool fits(std::size_t pinned, std::size_t unpinned) const {
    if (pinned + unpinned > cap_) {
      return false;
    }
    return unpinned <= unpinned_max();
  }

  bool needs_evict(std::size_t pinned, std::size_t unpinned,
                   bool incoming_pinned) const {
    if (!fits(pinned, unpinned)) {
      return true;
    }
    return !incoming_pinned && (pinned + unpinned) == cap_ && !lru_.empty();
  }

  void drop_entry(std::map<ResidentKey, Entry>::iterator it) {
    if (!it->second.pinned) {
      lru_.erase(it->second.lru_it);
      unpinned_bytes_ -= it->second.nbytes;
    } else {
      pinned_bytes_ -= it->second.nbytes;
    }
    entries_.erase(it);
  }

  bool evict_lru_unpinned() {
    if (lru_.empty()) {
      return false;
    }
    auto it = entries_.find(lru_.front());
    if (it == entries_.end() || it->second.pinned) {
      return false;
    }
    drop_entry(it);
    return true;
  }

  std::size_t cap_;
  std::size_t serve_reserve_;
  std::size_t pinned_bytes_ = 0;
  std::size_t unpinned_bytes_ = 0;
  std::list<ResidentKey> lru_;
  std::map<ResidentKey, Entry> entries_;
};

}  // namespace execution
}  // namespace base

#endif  // BASE_EXECUTION_EXECUTOR_DEVICE_RESIDENT_STORE_H
