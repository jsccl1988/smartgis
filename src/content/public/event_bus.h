// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_EVENT_BUS_H_
#define CONTENT_PUBLIC_EVENT_BUS_H_

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "content/public/types.h"

// Domain facts that already happened, and the session bus that publishes them.
// Supports GisContents / ToolSession observers; not a capability root and not
// GisContentsClient. Not RPC. Not a process singleton. Main thread only.
namespace content {

struct SelectionChanged {
  uint32_t view_id = 0;
  std::vector<FeatureId> ids;
};

struct ExtentChanged {
  uint32_t view_id = 0;
  Extent2 extent{};
};

// Shell / Workspace asked the GPU process to switch map paint.
// kind: 0 = Track B RHI / WorldPass, 1 = Track A MapLibre.
struct RenderBackendChanged {
  uint32_t view_id = 0;
  uint32_t kind = 0;
};

// Catalog / inspector refresh after plugin present mutates layers.
// Stable type name is "document.layers_changed" (not a content.* prefix).
struct LayersChanged {
  uint32_t view_id = 0;
  uint32_t layer_count = 0;
};

// Style document replaced or cleared on the bound GisDocument.
struct StyleChanged {
  uint32_t view_id = 0;
};

// Fired after EditSession::commit succeeds (e.g. draw.* draft → append).
// Shell status / inspectors subscribe; widgets never hold Feature*.
struct EditCommitted {
  uint32_t view_id = 0;
  FeatureId id{};
  // Mirrors gis::EditOp without pulling sdb into the event header.
  enum class Op { kAppend = 0, kDelete = 1, kModify = 2 };
  Op op = Op::kAppend;
};

// Stable id string, not a per-module static address. publish() is compiled
// into tool.dll while tests subscribe from the exe; a function-local static
// would be a different pointer in each module and the slot would never match.
template <typename E>
const char* event_type_name();

template <>
inline const char* event_type_name<SelectionChanged>() {
  return "content.SelectionChanged";
}
template <>
inline const char* event_type_name<ExtentChanged>() {
  return "content.ExtentChanged";
}
template <>
inline const char* event_type_name<RenderBackendChanged>() {
  return "content.RenderBackendChanged";
}
template <>
inline const char* event_type_name<EditCommitted>() {
  return "content.EditCommitted";
}
template <>
inline const char* event_type_name<LayersChanged>() {
  return "document.layers_changed";
}
template <>
inline const char* event_type_name<StyleChanged>() {
  return "document.style_changed";
}

class EventBus {
  struct Slot {
    std::uint64_t id = 0;
    const char* type_name = nullptr;
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
      // Default-constructed / moved-from: nothing to unsubscribe.
      if (id_ == 0) {
        hub_.reset();
        return;
      }
      // Uninitialized Connection (debug 0xCD fill / ABI-skewed Browser member)
      // must not call weak_ptr::lock — that AVs on the poison control block.
      const auto id_bytes = reinterpret_cast<const unsigned char*>(&id_);
      if (id_bytes[0] == 0xcd && id_bytes[1] == 0xcd && id_bytes[2] == 0xcd &&
          id_bytes[3] == 0xcd) {
        hub_.reset();
        id_ = 0;
        return;
      }
      if (auto hub = hub_.lock()) {
        hub->unsubscribe(id_);
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
    slot.type_name = event_type_name<E>();
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
    const char* type_name = event_type_name<E>();
    std::vector<std::function<void(const void*)>> fns;
    for (const Slot& slot : hub_->slots) {
      if (slot.type_name != nullptr &&
          std::strcmp(slot.type_name, type_name) == 0) {
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
