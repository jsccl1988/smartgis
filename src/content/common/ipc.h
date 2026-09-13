// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_COMMON_IPC_H
#define CONTENT_COMMON_IPC_H

#include <cstdint>
#include <string>
#include <vector>

#include "base/ipc/channel.h"
#include "content/public/host_protocol.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
namespace detail {

// Host named pipe: length-prefixed pickle frames (base::ipc::Channel).
class Pipe {
 public:
  bool create_server(const std::wstring& path) {
    return ch_.create_server(path);
  }
  bool wait_client(uint32_t timeout_ms) { return ch_.wait_client(timeout_ms); }
  bool connect_client(const std::wstring& path, uint32_t timeout_ms) {
    return ch_.connect_client(path, timeout_ms);
  }
  void close() { ch_.close(); }
  bool is_open() const { return ch_.is_open(); }

  template <typename T>
  bool send_msg(HostMsg type, uint32_t view_id, const T& body) {
    return ch_.send_msg(static_cast<uint16_t>(type), view_id, body);
  }

  bool send_empty(HostMsg type, uint32_t view_id) {
    return ch_.send_empty(static_cast<uint16_t>(type), view_id);
  }

  bool recv(FrameHeader* header,
            std::vector<uint8_t>* payload,
            uint32_t timeout_ms);

 private:
  base::ipc::Channel ch_;
};

template <typename T>
bool decode_payload(const std::vector<uint8_t>& payload, T* out) {
  return base::ipc::decode(payload.data(), payload.size(), out);
}

}  // namespace detail
}  // namespace content

#endif  // CONTENT_COMMON_IPC_H
