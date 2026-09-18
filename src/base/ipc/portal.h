// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_PORTAL_H
#define BASE_IPC_PORTAL_H

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "base/ipc/channel.h"
#include "base/ipc/handle.h"
#include "base/ipc/receiver.h"

namespace base {
namespace ipc {

inline constexpr uint16_t k_msg_portal_open = 0xFF10;
inline constexpr uint16_t k_msg_portal_bytes = 0xFF11;
inline constexpr uint16_t k_msg_portal_close = 0xFF12;

struct PortalOpenBody {
  uint32_t keep_id = 0;
  uint32_t move_id = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(keep_id, move_id);
  }
};

struct PortalInboxMsg {
  std::vector<uint8_t> bytes;
  std::vector<PlatformHandle> handles;
};

class Node;

// Bidirectional message end (ipcz portal / Mojo message pipe).
class Portal {
 public:
  Portal() = default;
  Portal(Portal&&) noexcept;
  Portal& operator=(Portal&&) noexcept;
  Portal(const Portal&) = delete;
  Portal& operator=(const Portal&) = delete;
  ~Portal();

  bool is_valid() const;
  uint32_t id() const;
  void reset();

  bool send(const void* data,
            uint32_t bytes,
            const PlatformHandle* handles = nullptr,
            uint32_t handle_count = 0);
  bool recv(std::vector<uint8_t>* bytes,
            std::vector<PlatformHandle>* handles,
            uint32_t timeout_ms);

 private:
  friend class Node;
  friend bool create_local_portal_pair(Portal* a, Portal* b);

  struct State;
  std::shared_ptr<State> state_;
};

bool create_local_portal_pair(Portal* a, Portal* b);

// One process node. Peer traffic uses an existing Channel (invitation).
class Node : public MessageListener {
 public:
  Node();
  ~Node() override;

  Node(const Node&) = delete;
  Node& operator=(const Node&) = delete;

  bool attach_peer(Channel* channel);
  bool create_portal_pair(Portal* a, Portal* b);
  bool offer_portal(Portal* moving);
  bool take_portal(Portal* out, uint32_t timeout_ms);

  void on_message(const Frame& frame,
                  std::vector<uint8_t> payload,
                  std::vector<PlatformHandle> handles) override;

 private:
  void deliver(uint32_t dest_id,
               std::vector<uint8_t> bytes,
               std::vector<PlatformHandle> handles);

  Channel* link_ = nullptr;
  std::unique_ptr<Receiver> receiver_;
  std::mutex mu_;
  std::condition_variable cv_;
  std::map<uint32_t, std::shared_ptr<Portal::State>> table_;
  std::deque<uint32_t> incoming_;
  uint32_t next_id_ = 1;
};

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_PORTAL_H
