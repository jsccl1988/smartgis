// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_COMMON_IPC_H
#define CONTENT_COMMON_IPC_H

#include <cstdint>
#include <string>
#include <utility>
#include <vector>


#include "common/ipc/bulk/bulk.h"
#include "common/ipc/channel/channel.h"
#include "common/ipc/codec/codec.h"
#include "common/ipc/handle/handle.h"
#include "content/common/host_protocol.h"
#include "content/content_export.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
namespace detail {

// Host named pipe: length-prefixed pickle frames (common::ipc::Channel).
class CONTENT_EXPORT Pipe {
 public:
  bool create_server(const std::wstring& path) {
    return ch_.create_server(path);
  }
  bool wait_client(uint32_t timeout_ms) { return ch_.wait_client(timeout_ms); }
  bool connect_client(const std::wstring& path, uint32_t timeout_ms) {
    return ch_.connect_client(path, timeout_ms);
  }
  bool adopt(common::ipc::Channel&& ch) {
    ch_ = std::move(ch);
    return ch_.is_open();
  }
  void set_peer_process(HANDLE process) { ch_.set_peer_process(process); }
  void close() { ch_.close(); }
  bool is_open() const { return ch_.is_open(); }
  common::ipc::Channel& channel() { return ch_; }

  // Large bodies ride DataPipe (k_msg_data_pipe_offer); small stay inline.
  template <typename T>
  bool send_msg(HostMsg type, uint32_t view_id, const T& body) {
    const std::string bytes = common::ipc::encode(body);
    return common::ipc::send_bulk(&ch_, static_cast<uint16_t>(type), view_id,
                                bytes);
  }

  // Handle-bearing frames stay inline (SharedHandle / DataPipe ends, etc.).
  template <typename T>
  bool send_msg(HostMsg type,
                uint32_t view_id,
                const T& body,
                const common::ipc::PlatformHandle* handles,
                uint32_t handle_count) {
    return ch_.send_msg(static_cast<uint16_t>(type), view_id, body, handles,
                        handle_count);
  }

  bool send_empty(HostMsg type, uint32_t view_id) {
    return ch_.send_empty(static_cast<uint16_t>(type), view_id);
  }

  bool recv(FrameHeader* header,
            std::vector<uint8_t>* payload,
            uint32_t timeout_ms);
  bool recv(FrameHeader* header,
            std::vector<uint8_t>* payload,
            std::vector<common::ipc::PlatformHandle>* handles,
            uint32_t timeout_ms);

 private:
  common::ipc::Channel ch_;
};

template <typename T>
bool decode_payload(const std::vector<uint8_t>& payload, T* out) {
  return common::ipc::decode(payload.data(), payload.size(), out);
}

}  // namespace detail
}  // namespace content

#endif  // CONTENT_COMMON_IPC_H
