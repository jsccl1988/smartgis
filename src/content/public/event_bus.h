// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_EVENT_BUS_H_
#define CONTENT_PUBLIC_EVENT_BUS_H_

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

// Session-scoped typed pub/sub. Not a process singleton. Main thread only.
namespace content {

class EventBus {
  struct Slot {
    std::uint64_t id = 0;
    const void* type = nullptr;
    std::function<void(const void*)> fn;
  };

  struct Hub {
    std::uint64_t next_id = 1;
    std::vector<Slot> slots;

    void unsubscribe(std::uint64_t id) {
      slots.erase(std::remove_if(slots.begin(), slots.end(),
                                   [id](const Slot& s) { return s.id == id; }),
                  slots.end());
    }
  };

  template <typename E>
  static const void* event_key() {
    static const char key = 0;
    return &key;
  }

 public:
  class Connection {
   public:
    Connection() = default;
    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept { *this = std::move(other); }

    Connection& operator=(Connection&& other) noexcept {
      if (this != &other) {
        disconnect();
        hub_ = std::move(other.hub_);
        id_ = other.id_;
        other.id_ = 0;
      }
      return *this;
    }

    ~Connection() { disconnect(); }

    void disconnect() {
      if (auto hub = hub_.lock()) {
        if (id_ != 0) {
          hub->unsubscribe(id_);
        }
      }
      hub_.reset();
      id_ = 0;
    }

    explicit operator bool() const { return id_ != 0 && !hub_.expired(); }

   private:
    friend class EventBus;
    std::weak_ptr<Hub> hub_;
    std::uint64_t id_ = 0;
  };

  EventBus() : hub_(std::make_shared<Hub>()) {}
  EventBus(const EventBus&) = delete;
  EventBus& operator=(const EventBus&) = delete;

  template <typename E>
  Connection subscribe(std::function<void(const E&)> fn) {
    Connection c;
    if (!fn || !hub_) {
      return c;
    }
    const std::uint64_t id = hub_->next_id++;
    Slot slot;
    slot.id = id;
    slot.type = event_key<E>();
    slot.fn = [fn = std::move(fn)](const void* p) {
      fn(*static_cast<const E*>(p));
    };
    hub_->slots.push_back(std::move(slot));
    c.hub_ = hub_;
    c.id_ = id;
    return c;
  }

  template <typename E>
  void publish(const E& e) const {
    if (!hub_) {
      return;
    }
    const void* type = event_key<E>();
    std::vector<std::function<void(const void*)>> fns;
    for (const Slot& slot : hub_->slots) {
      if (slot.type == type) {
        fns.push_back(slot.fn);
      }
    }
    for (const auto& fn : fns) {
      fn(&e);
    }
  }

 private:
  std::shared_ptr<Hub> hub_;
};

}  // namespace content

#endif  // CONTENT_PUBLIC_EVENT_BUS_H_
