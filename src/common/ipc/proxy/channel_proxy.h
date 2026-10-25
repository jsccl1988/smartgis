// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef COMMON_IPC_PROXY_CHANNEL_PROXY_H_
#define COMMON_IPC_PROXY_CHANNEL_PROXY_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "common/ipc/channel/channel.h"
#include "common/ipc/codec/codec.h"
#include "common/ipc/handle/handle.h"
#include "common/ipc/receiver/receiver.h"

namespace common {
namespace ipc {

// Thread-aware send facade over Channel (Chromium ChannelProxy shape).
// When ScopedIpcSupport is active, Send is posted to the IO thread; otherwise
// Send is synchronous on the caller. Inbound messages use Receiver.
class ChannelProxy {
 public:
  ChannelProxy() = default;
  explicit ChannelProxy(Channel&& channel);
  ~ChannelProxy();

  ChannelProxy(const ChannelProxy&) = delete;
  ChannelProxy& operator=(const ChannelProxy&) = delete;
  ChannelProxy(ChannelProxy&&) = delete;
  ChannelProxy& operator=(ChannelProxy&&) = delete;

  bool adopt(Channel&& channel);
  bool bind(MessageListener* listener);
  void reset();

  bool is_open() const { return channel_.is_open(); }
  Channel& channel() { return channel_; }
  const Channel& channel() const { return channel_; }

  void set_peer_process(HANDLE process) { channel_.set_peer_process(process); }

  // Queued when ScopedIpcSupport is active; otherwise sync. Returns false only
  // when the channel is closed or a sync send fails.
  bool send(uint16_t type,
            uint32_t view_id,
            uint32_t seq,
            std::vector<uint8_t> payload,
            const PlatformHandle* handles = nullptr,
            uint32_t handle_count = 0);

  bool send(uint16_t type,
            uint32_t view_id,
            const void* payload,
            uint32_t payload_bytes);

  template <typename T>
  bool send_msg(uint16_t type, uint32_t view_id, const T& body) {
    const std::string bytes = encode(body);
    return send(type, view_id, 0,
                std::vector<uint8_t>(bytes.begin(), bytes.end()));
  }

  template <typename T>
  bool send_msg(uint16_t type,
                uint32_t view_id,
                const T& body,
                const PlatformHandle* handles,
                uint32_t handle_count) {
    const std::string bytes = encode(body);
    return send(type, view_id, 0,
                std::vector<uint8_t>(bytes.begin(), bytes.end()), handles,
                handle_count);
  }

  bool send_empty(uint16_t type, uint32_t view_id);

 private:
  struct SendState;
  static bool write_frame(const std::shared_ptr<SendState>& state,
                          uint16_t type,
                          uint32_t view_id,
                          uint32_t seq,
                          const std::vector<uint8_t>& payload,
                          const std::vector<HANDLE>& owned_handles);

  Channel channel_;
  std::shared_ptr<SendState> send_state_;
  std::unique_ptr<Receiver> receiver_;
};

}  // namespace ipc
}  // namespace common

#endif  // COMMON_IPC_PROXY_CHANNEL_PROXY_H_
