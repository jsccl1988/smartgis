// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_CHANNEL_H
#define BASE_IPC_CHANNEL_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/ipc/codec.h"

namespace base {
namespace ipc {

// Length-prefixed named-pipe frame. Payload is pickle (not JSON, not CRLF).
struct Frame {
  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t type = 0;
  uint16_t flags = 0;
  uint32_t view_id = 0;
  uint32_t payload_bytes = 0;
};

inline constexpr uint32_t k_frame_magic = 0x31544D53u;  // 'SMT1' LE
inline constexpr uint16_t k_frame_version = 2;           // pickle bodies
inline constexpr uint16_t k_flag_binary = 2;

#pragma pack(push, 1)
struct FrameWire {
  uint32_t magic;
  uint16_t version;
  uint16_t type;
  uint16_t flags;
  uint32_t view_id;
  uint32_t payload_bytes;
};
#pragma pack(pop)

static_assert(sizeof(FrameWire) == 18, "ipc frame header");

// Duplex Win32 named pipe: length-prefixed frames, pickle payloads.
// Static foundation (//src/base/ipc); not exported from the product platform DLL.
class Channel {
 public:
  Channel();
  ~Channel();

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;

  bool create_server(const std::wstring& path);
  bool wait_client(uint32_t timeout_ms);
  bool connect_client(const std::wstring& path, uint32_t timeout_ms);
  void close();
  bool is_open() const;

  bool send(uint16_t type,
            uint32_t view_id,
            const void* payload,
            uint32_t payload_bytes);

  template <typename T>
  bool send_msg(uint16_t type, uint32_t view_id, const T& body) {
    const std::string bytes = encode(body);
    return send(type, view_id, bytes.data(),
                static_cast<uint32_t>(bytes.size()));
  }

  bool send_empty(uint16_t type, uint32_t view_id);

  bool recv(Frame* header, std::vector<uint8_t>* payload, uint32_t timeout_ms);

 private:
  bool write_all(const void* data, uint32_t bytes, uint32_t timeout_ms);
  bool read_all(void* data, uint32_t bytes, uint32_t timeout_ms);

  HANDLE pipe_;
  std::mutex write_mu_;
};

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_CHANNEL_H
