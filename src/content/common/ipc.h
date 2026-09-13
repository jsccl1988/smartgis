// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_COMMON_IPC_H
#define CONTENT_COMMON_IPC_H

#include <cstdint>
#include <string>
#include <vector>

#include "content/public/host_protocol.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
namespace detail {

// Duplex byte-stream named pipe (Host ABI 0.6 control plane).
class Pipe {
 public:
  Pipe();
  ~Pipe();

  Pipe(const Pipe&) = delete;
  Pipe& operator=(const Pipe&) = delete;

  bool create_server(const std::wstring& path);
  bool wait_client(uint32_t timeout_ms);
  bool connect_client(const std::wstring& path, uint32_t timeout_ms);
  void close();
  bool is_open() const;

  bool send(HostMsg type,
            uint16_t flags,
            uint32_t view_id,
            const void* payload,
            uint32_t payload_bytes);
  bool send_json(HostMsg type, uint32_t view_id, const std::string& json);
  bool send_binary(HostMsg type,
                   uint32_t view_id,
                   const void* payload,
                   uint32_t payload_bytes);

  bool recv(FrameHeader* header,
            std::vector<uint8_t>* payload,
            uint32_t timeout_ms);

 private:
  bool write_all(const void* data, uint32_t bytes, uint32_t timeout_ms);
  bool read_all(void* data, uint32_t bytes, uint32_t timeout_ms);

  HANDLE pipe_;
  CRITICAL_SECTION write_lock_;
};

std::string json_get_string(const std::string& json, const char* key);
int json_get_int(const std::string& json, const char* key, int fallback);
double json_get_double(const std::string& json, const char* key, double fallback);
bool json_has_key(const std::string& json, const char* key);

}  // namespace detail
}  // namespace content

#endif  // CONTENT_COMMON_IPC_H
