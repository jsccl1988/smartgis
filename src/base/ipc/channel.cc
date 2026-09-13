// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/channel.h"

#include <cstring>

namespace base {
namespace ipc {
namespace {

bool wait_overlapped(HANDLE file, OVERLAPPED* ov, uint32_t timeout_ms) {
  DWORD got = 0;
  if (GetOverlappedResult(file, ov, &got, FALSE)) {
    return true;
  }
  if (GetLastError() != ERROR_IO_INCOMPLETE) {
    return false;
  }
  const DWORD w = WaitForSingleObject(ov->hEvent, timeout_ms);
  if (w != WAIT_OBJECT_0) {
    CancelIo(file);
    return false;
  }
  return GetOverlappedResult(file, ov, &got, FALSE) != FALSE;
}

}  // namespace

Channel::Channel() : pipe_(INVALID_HANDLE_VALUE) {}

Channel::~Channel() {
  close();
}

bool Channel::create_server(const std::wstring& path) {
  close();
  pipe_ = CreateNamedPipeW(path.c_str(),
                           PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                           65536, 65536, 0, nullptr);
  return pipe_ != INVALID_HANDLE_VALUE;
}

bool Channel::wait_client(uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE) {
    return false;
  }
  OVERLAPPED ov = {};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ov.hEvent) {
    return false;
  }
  const BOOL ok = ConnectNamedPipe(pipe_, &ov);
  if (ok) {
    CloseHandle(ov.hEvent);
    return true;
  }
  const DWORD err = GetLastError();
  if (err == ERROR_PIPE_CONNECTED) {
    CloseHandle(ov.hEvent);
    return true;
  }
  if (err != ERROR_IO_PENDING) {
    CloseHandle(ov.hEvent);
    return false;
  }
  const bool ready = wait_overlapped(pipe_, &ov, timeout_ms);
  CloseHandle(ov.hEvent);
  return ready;
}

bool Channel::connect_client(const std::wstring& path, uint32_t timeout_ms) {
  close();
  const DWORD start = GetTickCount();
  for (;;) {
    pipe_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                        OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe_ != INVALID_HANDLE_VALUE) {
      DWORD mode = PIPE_READMODE_BYTE;
      SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);
      return true;
    }
    if (GetTickCount() - start >= timeout_ms) {
      return false;
    }
    Sleep(20);
  }
}

void Channel::close() {
  if (pipe_ != INVALID_HANDLE_VALUE) {
    CancelIo(pipe_);
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }
}

bool Channel::is_open() const {
  return pipe_ != INVALID_HANDLE_VALUE;
}

bool Channel::write_all(const void* data, uint32_t bytes, uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE || (bytes > 0 && !data)) {
    return false;
  }
  if (bytes == 0) {
    return true;
  }
  OVERLAPPED ov = {};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ov.hEvent) {
    return false;
  }
  DWORD wrote = 0;
  const BOOL ok = WriteFile(pipe_, data, bytes, &wrote, &ov);
  if (ok && wrote == bytes) {
    CloseHandle(ov.hEvent);
    return true;
  }
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    CloseHandle(ov.hEvent);
    return false;
  }
  if (!wait_overlapped(pipe_, &ov, timeout_ms)) {
    CloseHandle(ov.hEvent);
    return false;
  }
  GetOverlappedResult(pipe_, &ov, &wrote, FALSE);
  CloseHandle(ov.hEvent);
  return wrote == bytes;
}

bool Channel::read_all(void* data, uint32_t bytes, uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE || !data) {
    return false;
  }
  OVERLAPPED ov = {};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ov.hEvent) {
    return false;
  }
  DWORD got = 0;
  const BOOL ok = ReadFile(pipe_, data, bytes, &got, &ov);
  if (ok && got == bytes) {
    CloseHandle(ov.hEvent);
    return true;
  }
  if (!ok && GetLastError() != ERROR_IO_PENDING) {
    CloseHandle(ov.hEvent);
    return false;
  }
  if (!wait_overlapped(pipe_, &ov, timeout_ms)) {
    CloseHandle(ov.hEvent);
    return false;
  }
  GetOverlappedResult(pipe_, &ov, &got, FALSE);
  CloseHandle(ov.hEvent);
  return got == bytes;
}

bool Channel::send(uint16_t type,
                  uint32_t view_id,
                  const void* payload,
                  uint32_t payload_bytes) {
  FrameWire h = {};
  h.magic = k_frame_magic;
  h.version = k_frame_version;
  h.type = type;
  h.flags = k_flag_binary;
  h.view_id = view_id;
  h.payload_bytes = payload_bytes;
  std::lock_guard<std::mutex> lock(write_mu_);
  const bool ok_h = write_all(&h, sizeof(h), 10000);
  const bool ok_p =
      payload_bytes == 0 || write_all(payload, payload_bytes, 10000);
  return ok_h && ok_p;
}

bool Channel::send_empty(uint16_t type, uint32_t view_id) {
  return send(type, view_id, nullptr, 0);
}

bool Channel::recv(Frame* header, std::vector<uint8_t>* payload,
                   uint32_t timeout_ms) {
  if (!header || !payload) {
    return false;
  }
  FrameWire h = {};
  if (!read_all(&h, sizeof(h), timeout_ms)) {
    return false;
  }
  if (h.magic != k_frame_magic || h.version != k_frame_version) {
    close();
    return false;
  }
  header->magic = h.magic;
  header->version = h.version;
  header->type = h.type;
  header->flags = h.flags;
  header->view_id = h.view_id;
  header->payload_bytes = h.payload_bytes;
  payload->assign(h.payload_bytes, 0);
  if (h.payload_bytes == 0) {
    return true;
  }
  return read_all(payload->data(), h.payload_bytes, timeout_ms);
}

}  // namespace ipc
}  // namespace base
