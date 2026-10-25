// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef COMMON_IPC_BROKER_BROKER_H_
#define COMMON_IPC_BROKER_BROKER_H_

#include <cstdint>
#include <vector>

#include "common/ipc/channel/channel.h"
#include "common/ipc/handle/handle.h"

namespace common {
namespace ipc {

// Broker control messages multiplexed on the product Channel (portal-style).
inline constexpr uint16_t k_msg_broker_create_section = 0xFF20;
inline constexpr uint16_t k_msg_broker_reply = 0xFF22;

struct BrokerCreateSectionBody {
  uint32_t request_id = 0;
  uint32_t byte_count = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(request_id, byte_count);
  }
};

struct BrokerReplyBody {
  uint32_t request_id = 0;
  uint32_t ok = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(request_id, ok);
  }
};

inline bool is_broker_type(uint16_t type) {
  return type == k_msg_broker_create_section || type == k_msg_broker_reply;
}

// Parent-side broker: creates shared sections for a child that cannot.
// |client_process| is the child PROCESS handle (PROCESS_DUP_HANDLE).
class BrokerHost {
 public:
  BrokerHost() = default;

  void set_channel(Channel* channel) { channel_ = channel; }
  void set_client_process(HANDLE process) { client_ = process; }

  // Returns true when |frame.type| was a broker request and was handled.
  bool try_handle(const Frame& frame,
                  const std::vector<uint8_t>& payload,
                  std::vector<PlatformHandle>* handles);

 private:
  Channel* channel_ = nullptr;
  HANDLE client_ = nullptr;
};

// Child helper: ask the parent BrokerHost to CreateFileMapping and return the
// section HANDLE already duplicated into this process (as attachment).
bool broker_create_section(Channel* channel,
                           uint32_t byte_count,
                           PlatformHandle* section_out,
                           uint32_t timeout_ms = 5000);

}  // namespace ipc
}  // namespace common

#endif  // COMMON_IPC_BROKER_BROKER_H_
