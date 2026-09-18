// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef BASE_IPC_CHANNEL_H
#define BASE_IPC_CHANNEL_H

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/ipc/codec.h"
#include "base/ipc/handle.h"

namespace base {
namespace ipc {

inline constexpr uint32_t k_frame_magic = 0x31544D53u;  // 'SMT1' LE
inline constexpr uint16_t k_frame_version = 3;           // seq + handle list
inline constexpr uint16_t k_flag_binary = 2;
inline constexpr uint16_t k_flag_has_handles = 8;
inline constexpr uint32_t k_max_payload_bytes = 16u * 1024u * 1024u;
inline constexpr uint16_t k_max_handles = 8;

// Length-prefixed named-pipe frame. Payload is pickle (not JSON, not CRLF).
// pack(1): unpacked Frame is 28 on MSVC (2 bytes after handle_count).
#pragma pack(push, 1)
struct Frame {
  uint32_t magic = 0;
  uint16_t version = 0;
  uint16_t type = 0;
  uint16_t flags = 0;
  uint16_t handle_count = 0;
  uint32_t view_id = 0;
  uint32_t seq = 0;
  uint32_t payload_bytes = 0;
};

struct FrameWire {
  uint32_t magic;
  uint16_t version;
  uint16_t type;
  uint16_t flags;
  uint16_t handle_count;
  uint32_t view_id;
  uint32_t seq;
  uint32_t payload_bytes;
};
#pragma pack(pop)

static_assert(sizeof(Frame) == 24, "ipc frame header");
static_assert(sizeof(FrameWire) == 24, "ipc frame wire");
static_assert(sizeof(Frame) == sizeof(FrameWire), "Frame/FrameWire ABI");

// Duplex Win32 named pipe: length-prefixed frames, pickle payloads.
// Static foundation (//src/base/ipc); not exported from the product platform DLL.
class Channel {
 public:
  Channel();
  ~Channel();

  Channel(const Channel&) = delete;
  Channel& operator=(const Channel&) = delete;
  Channel(Channel&& other) noexcept;
  Channel& operator=(Channel&& other) noexcept;

  bool create_server(const std::wstring& path);
  bool wait_client(uint32_t timeout_ms);
  bool connect_client(const std::wstring& path, uint32_t timeout_ms);
  bool adopt(HANDLE pipe, bool take_ownership = true);
  void close();
  // Wake wait_readable / pending ReadFile without CloseHandle (no UAF/hang).
  void interrupt();
  bool is_open() const;

  // Duplicate attached handles into this process when sending.
  void set_peer_process(HANDLE process);

  bool send(uint16_t type,
            uint32_t view_id,
            const void* payload,
            uint32_t payload_bytes);
  bool send(uint16_t type,
            uint32_t view_id,
            uint32_t seq,
            const void* payload,
            uint32_t payload_bytes,
            const PlatformHandle* handles,
            uint32_t handle_count);

  template <typename T>
  bool send_msg(uint16_t type, uint32_t view_id, const T& body) {
    const std::string bytes = encode(body);
    return send(type, view_id, bytes.data(),
                static_cast<uint32_t>(bytes.size()));
  }

  template <typename T>
  bool send_msg(uint16_t type,
                uint32_t view_id,
                const T& body,
                const PlatformHandle* handles,
                uint32_t handle_count) {
    const std::string bytes = encode(body);
    return send(type, view_id, 0, bytes.data(),
                static_cast<uint32_t>(bytes.size()), handles, handle_count);
  }

  bool send_empty(uint16_t type, uint32_t view_id);

  bool recv(Frame* header, std::vector<uint8_t>* payload, uint32_t timeout_ms);
  bool recv(Frame* header,
            std::vector<uint8_t>* payload,
            std::vector<PlatformHandle>* handles,
            uint32_t timeout_ms);

  uint32_t next_seq();

  // Signaled when a header ReadFile completes. Pair with arm_header_read().
  // Do not arm until wait_client / connect_client / adopt has connected.
  HANDLE readable_event() const { return read_event_; }
  bool arm_header_read();
  bool has_ready_header() const { return header_ready_; }
  bool wait_readable(HANDLE extra_abort, uint32_t timeout_ms);

 private:
  void reset_io_event();
  void reset_write_event();
  void reset_abort();
  bool write_all(const void* data, uint32_t bytes, uint32_t timeout_ms);
  bool read_all(void* data, uint32_t bytes, uint32_t timeout_ms);
  bool wait_io(OVERLAPPED* ov, uint32_t timeout_ms, DWORD* transferred);
  bool finish_header_read();

  HANDLE pipe_;
  HANDLE io_event_;    // payload reads + ConnectNamedPipe
  HANDLE write_event_; // WriteFile only (must not share with reads)
  HANDLE read_event_;  // async frame-header ReadFile
  HANDLE abort_event_;
  HANDLE peer_;
  bool owns_pipe_;
  bool connected_;
  bool header_pending_;
  bool header_ready_;
  FrameWire pending_header_{};
  OVERLAPPED read_ov_{};
  std::mutex write_mu_;
  std::atomic<uint32_t> next_seq_;
};

}  // namespace ipc
}  // namespace base

#endif  // BASE_IPC_CHANNEL_H
