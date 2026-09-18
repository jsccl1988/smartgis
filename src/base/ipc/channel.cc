// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "base/ipc/channel.h"

#include <cstring>

namespace base {
namespace ipc {

Channel::Channel()
    : pipe_(INVALID_HANDLE_VALUE),
      io_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      write_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      read_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      abort_event_(CreateEventW(nullptr, TRUE, FALSE, nullptr)),
      peer_(nullptr),
      owns_pipe_(true),
      connected_(false),
      header_pending_(false),
      header_ready_(false),
      next_seq_(1) {}

Channel::~Channel() {
  close();
  if (io_event_) {
    CloseHandle(io_event_);
    io_event_ = nullptr;
  }
  if (write_event_) {
    CloseHandle(write_event_);
    write_event_ = nullptr;
  }
  if (read_event_) {
    CloseHandle(read_event_);
    read_event_ = nullptr;
  }
  if (abort_event_) {
    CloseHandle(abort_event_);
    abort_event_ = nullptr;
  }
}

Channel::Channel(Channel&& other) noexcept
    : pipe_(other.pipe_),
      io_event_(other.io_event_),
      write_event_(other.write_event_),
      read_event_(other.read_event_),
      abort_event_(other.abort_event_),
      peer_(other.peer_),
      owns_pipe_(other.owns_pipe_),
      connected_(other.connected_),
      header_pending_(other.header_pending_),
      header_ready_(other.header_ready_),
      pending_header_(other.pending_header_),
      read_ov_(other.read_ov_),
      next_seq_(other.next_seq_.load()) {
  other.pipe_ = INVALID_HANDLE_VALUE;
  other.io_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.write_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.read_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.abort_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.peer_ = nullptr;
  other.owns_pipe_ = true;
  other.connected_ = false;
  other.header_pending_ = false;
  other.header_ready_ = false;
  other.pending_header_ = {};
  other.read_ov_ = {};
  other.next_seq_.store(1);
}

Channel& Channel::operator=(Channel&& other) noexcept {
  if (this == &other) {
    return *this;
  }
  close();
  pipe_ = other.pipe_;
  peer_ = other.peer_;
  owns_pipe_ = other.owns_pipe_;
  connected_ = other.connected_;
  header_pending_ = other.header_pending_;
  header_ready_ = other.header_ready_;
  pending_header_ = other.pending_header_;
  read_ov_ = other.read_ov_;
  next_seq_.store(other.next_seq_.load());
  if (io_event_) {
    CloseHandle(io_event_);
  }
  if (write_event_) {
    CloseHandle(write_event_);
  }
  if (read_event_) {
    CloseHandle(read_event_);
  }
  if (abort_event_) {
    CloseHandle(abort_event_);
  }
  io_event_ = other.io_event_;
  write_event_ = other.write_event_;
  read_event_ = other.read_event_;
  abort_event_ = other.abort_event_;
  other.pipe_ = INVALID_HANDLE_VALUE;
  other.io_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.write_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.read_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.abort_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  other.peer_ = nullptr;
  other.owns_pipe_ = true;
  other.connected_ = false;
  other.header_pending_ = false;
  other.header_ready_ = false;
  other.pending_header_ = {};
  other.read_ov_ = {};
  other.next_seq_.store(1);
  return *this;
}

void Channel::reset_io_event() {
  if (io_event_) {
    ResetEvent(io_event_);
  }
}

void Channel::reset_write_event() {
  if (write_event_) {
    ResetEvent(write_event_);
  }
}

void Channel::reset_abort() {
  if (abort_event_) {
    ResetEvent(abort_event_);
  }
  connected_ = false;
  header_pending_ = false;
  header_ready_ = false;
  pending_header_ = {};
  read_ov_ = {};
}

bool Channel::create_server(const std::wstring& path) {
  close();
  reset_abort();
  pipe_ = CreateNamedPipeW(path.c_str(),
                           PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT, 1,
                           65536, 65536, 0, nullptr);
  owns_pipe_ = true;
  return pipe_ != INVALID_HANDLE_VALUE;
}

bool Channel::wait_client(uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE || !io_event_) {
    return false;
  }
  reset_io_event();
  OVERLAPPED ov = {};
  ov.hEvent = io_event_;
  const BOOL ok = ConnectNamedPipe(pipe_, &ov);
  if (ok || GetLastError() == ERROR_PIPE_CONNECTED) {
    connected_ = true;
    return true;
  }
  if (GetLastError() != ERROR_IO_PENDING) {
    return false;
  }
  DWORD got = 0;
  if (!wait_io(&ov, timeout_ms, &got)) {
    return false;
  }
  connected_ = true;
  return true;
}

bool Channel::connect_client(const std::wstring& path, uint32_t timeout_ms) {
  close();
  reset_abort();
  const DWORD start = GetTickCount();
  for (;;) {
    const DWORD elapsed = GetTickCount() - start;
    if (timeout_ms != INFINITE && elapsed >= timeout_ms) {
      return false;
    }
    const DWORD left =
        timeout_ms == INFINITE ? 200 : (timeout_ms - elapsed);
    const DWORD slice = left > 200 ? 200 : left;
    WaitNamedPipeW(path.c_str(), slice);
    pipe_ = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr,
                        OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe_ != INVALID_HANDLE_VALUE) {
      DWORD mode = PIPE_READMODE_BYTE;
      SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);
      owns_pipe_ = true;
      connected_ = true;
      return true;
    }
    if (GetLastError() != ERROR_FILE_NOT_FOUND &&
        GetLastError() != ERROR_PIPE_BUSY) {
      if (GetTickCount() - start >= timeout_ms) {
        return false;
      }
    }
  }
}

bool Channel::adopt(HANDLE pipe, bool take_ownership) {
  close();
  reset_abort();
  if (!pipe || pipe == INVALID_HANDLE_VALUE) {
    return false;
  }
  pipe_ = pipe;
  owns_pipe_ = take_ownership;
  connected_ = true;
  DWORD mode = PIPE_READMODE_BYTE;
  SetNamedPipeHandleState(pipe_, &mode, nullptr, nullptr);
  return true;
}

void Channel::interrupt() {
  if (abort_event_) {
    SetEvent(abort_event_);
  }
  if (header_pending_ && pipe_ != INVALID_HANDLE_VALUE) {
    CancelIoEx(pipe_, &read_ov_);
  }
}

void Channel::close() {
  interrupt();
  if (pipe_ != INVALID_HANDLE_VALUE) {
    if (header_pending_) {
      if (read_event_) {
        WaitForSingleObject(read_event_, 2000);
      }
      DWORD ignored = 0;
      GetOverlappedResult(pipe_, &read_ov_, &ignored, FALSE);
      header_pending_ = false;
    }
    CancelIo(pipe_);
    if (owns_pipe_) {
      const DWORD ev = read_event_ ? WaitForSingleObject(read_event_, 0)
                                   : WAIT_OBJECT_0;
      if (!header_pending_ || ev == WAIT_OBJECT_0) {
        CloseHandle(pipe_);
      }
    }
    pipe_ = INVALID_HANDLE_VALUE;
  }
  connected_ = false;
  header_pending_ = false;
  header_ready_ = false;
}

bool Channel::is_open() const {
  return pipe_ != INVALID_HANDLE_VALUE;
}

void Channel::set_peer_process(HANDLE process) {
  peer_ = process;
}

uint32_t Channel::next_seq() {
  return next_seq_.fetch_add(1);
}

bool Channel::wait_io(OVERLAPPED* ov, uint32_t timeout_ms, DWORD* transferred) {
  if (!ov || !ov->hEvent) {
    return false;
  }
  HANDLE wait[2] = {ov->hEvent, abort_event_};
  const DWORD count = abort_event_ ? 2 : 1;
  const DWORD w = WaitForMultipleObjects(count, wait, FALSE, timeout_ms);
  if (w != WAIT_OBJECT_0) {
    CancelIoEx(pipe_, ov);
    if (ov->hEvent) {
      WaitForSingleObject(ov->hEvent, 2000);
    }
    DWORD ignored = 0;
    GetOverlappedResult(pipe_, ov, &ignored, FALSE);
    return false;
  }
  return GetOverlappedResult(pipe_, ov, transferred, FALSE) != FALSE;
}

bool Channel::write_all(const void* data, uint32_t bytes, uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE || (bytes > 0 && !data) || !write_event_) {
    return false;
  }
  if (bytes == 0) {
    return true;
  }
  const DWORD start = GetTickCount();
  const char* p = static_cast<const char*>(data);
  uint32_t done = 0;
  while (done < bytes) {
    const DWORD elapsed = GetTickCount() - start;
    if (elapsed >= timeout_ms) {
      return false;
    }
    const DWORD left = timeout_ms - elapsed;
    reset_write_event();
    OVERLAPPED ov = {};
    ov.hEvent = write_event_;
    DWORD wrote = 0;
    const BOOL ok =
        WriteFile(pipe_, p + done, bytes - done, &wrote, &ov);
    if (!ok) {
      if (GetLastError() != ERROR_IO_PENDING) {
        return false;
      }
      if (!wait_io(&ov, left, &wrote)) {
        return false;
      }
    }
    if (wrote == 0) {
      return false;
    }
    done += wrote;
  }
  return true;
}

bool Channel::read_all(void* data, uint32_t bytes, uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE || !data || !io_event_) {
    return false;
  }
  if (bytes == 0) {
    return true;
  }
  const DWORD start = GetTickCount();
  char* p = static_cast<char*>(data);
  uint32_t done = 0;
  while (done < bytes) {
    const DWORD elapsed = GetTickCount() - start;
    DWORD left = INFINITE;
    if (timeout_ms != INFINITE) {
      if (elapsed >= timeout_ms) {
        if (done > 0) {
          return false;
        }
        left = 0;
      } else {
        left = timeout_ms - elapsed;
      }
    }
    reset_io_event();
    OVERLAPPED ov = {};
    ov.hEvent = io_event_;
    DWORD got = 0;
    const BOOL ok = ReadFile(pipe_, p + done, bytes - done, &got, &ov);
    if (!ok) {
      if (GetLastError() != ERROR_IO_PENDING) {
        return false;
      }
      if (!wait_io(&ov, left, &got)) {
        return false;
      }
    }
    if (got == 0) {
      return false;
    }
    done += got;
  }
  return true;
}

bool Channel::arm_header_read() {
  if (header_ready_) {
    return true;
  }
  if (header_pending_) {
    return true;
  }
  if (!connected_ || pipe_ == INVALID_HANDLE_VALUE || !read_event_) {
    return false;
  }
  ResetEvent(read_event_);
  read_ov_ = {};
  read_ov_.hEvent = read_event_;
  pending_header_ = {};
  DWORD got = 0;
  const BOOL ok = ReadFile(pipe_, &pending_header_, sizeof(FrameWire),
                           &got, &read_ov_);
  if (ok) {
    if (got != sizeof(FrameWire)) {
      close();
      return false;
    }
    header_ready_ = true;
    return true;
  }
  if (GetLastError() != ERROR_IO_PENDING) {
    return false;
  }
  header_pending_ = true;
  return true;
}

bool Channel::finish_header_read() {
  DWORD got = 0;
  if (!GetOverlappedResult(pipe_, &read_ov_, &got, FALSE) ||
      got != sizeof(FrameWire)) {
    header_pending_ = false;
    return false;
  }
  header_pending_ = false;
  header_ready_ = true;
  return true;
}

bool Channel::wait_readable(HANDLE extra_abort, uint32_t timeout_ms) {
  if (header_ready_) {
    return true;
  }
  if (header_pending_) {
    DWORD got = 0;
    if (GetOverlappedResult(pipe_, &read_ov_, &got, FALSE)) {
      if (got == sizeof(FrameWire)) {
        header_pending_ = false;
        header_ready_ = true;
        return true;
      }
      header_pending_ = false;
      return false;
    }
    if (GetLastError() != ERROR_IO_INCOMPLETE) {
      header_pending_ = false;
      return false;
    }
  }
  if (!connected_) {
    HANDLE wait[2] = {};
    DWORD count = 0;
    if (abort_event_) {
      wait[count++] = abort_event_;
    }
    if (extra_abort) {
      wait[count++] = extra_abort;
    }
    if (count > 0) {
      WaitForMultipleObjects(count, wait, FALSE, timeout_ms);
    }
    return false;
  }
  if (!arm_header_read()) {
    return false;
  }
  if (header_ready_) {
    return true;
  }
  HANDLE wait[3] = {read_event_, abort_event_, extra_abort};
  DWORD count = 1;
  if (abort_event_) {
    ++count;
  }
  if (extra_abort) {
    wait[count] = extra_abort;
    ++count;
  }
  const DWORD w = WaitForMultipleObjects(count, wait, FALSE, timeout_ms);
  if (w == WAIT_OBJECT_0) {
    return finish_header_read();
  }
  // timeout 0 is a poll used by ScopedIpcSupport; leave the armed ReadFile.
  if (timeout_ms == 0) {
    return false;
  }
  if (header_pending_) {
    CancelIoEx(pipe_, &read_ov_);
    if (read_event_) {
      WaitForSingleObject(read_event_, 2000);
    }
    DWORD ignored = 0;
    GetOverlappedResult(pipe_, &read_ov_, &ignored, FALSE);
    header_pending_ = false;
  }
  return false;
}

bool Channel::send(uint16_t type,
                   uint32_t view_id,
                   const void* payload,
                   uint32_t payload_bytes) {
  return send(type, view_id, 0, payload, payload_bytes, nullptr, 0);
}

bool Channel::send(uint16_t type,
                   uint32_t view_id,
                   uint32_t seq,
                   const void* payload,
                   uint32_t payload_bytes,
                   const PlatformHandle* handles,
                   uint32_t handle_count) {
  if (payload_bytes > k_max_payload_bytes || handle_count > k_max_handles) {
    return false;
  }
  if (payload_bytes > 0 && !payload) {
    return false;
  }
  if (handle_count > 0 && !handles) {
    return false;
  }
  FrameWire h = {};
  h.magic = k_frame_magic;
  h.version = k_frame_version;
  h.type = type;
  h.flags = k_flag_binary;
  h.handle_count = static_cast<uint16_t>(handle_count);
  if (handle_count > 0) {
    h.flags = static_cast<uint16_t>(h.flags | k_flag_has_handles);
  }
  h.view_id = view_id;
  h.seq = seq ? seq : next_seq();
  h.payload_bytes = payload_bytes;

  uint64_t tokens[k_max_handles] = {};
  PlatformHandle owned[k_max_handles];
  for (uint32_t i = 0; i < handle_count; ++i) {
    if (peer_) {
      if (!wrap_into(handles[i].native, peer_, &owned[i])) {
        return false;
      }
      tokens[i] = handle_to_token(owned[i]);
      owned[i].release();
    } else {
      tokens[i] = handle_to_token(handles[i]);
    }
  }

  std::lock_guard<std::mutex> lock(write_mu_);
  if (!write_all(&h, sizeof(FrameWire), 10000)) {
    close();
    return false;
  }
  if (payload_bytes > 0 && !write_all(payload, payload_bytes, 10000)) {
    close();
    return false;
  }
  if (handle_count > 0 &&
      !write_all(tokens, handle_count * sizeof(uint64_t), 10000)) {
    close();
    return false;
  }
  return true;
}

bool Channel::send_empty(uint16_t type, uint32_t view_id) {
  return send(type, view_id, nullptr, 0);
}

bool Channel::recv(Frame* header, std::vector<uint8_t>* payload,
                   uint32_t timeout_ms) {
  return recv(header, payload, nullptr, timeout_ms);
}

bool Channel::recv(Frame* header,
                   std::vector<uint8_t>* payload,
                   std::vector<PlatformHandle>* handles,
                   uint32_t timeout_ms) {
  if (!header || !payload) {
    return false;
  }
  FrameWire h = {};
  if (!wait_readable(nullptr, timeout_ms) || !header_ready_) {
    return false;
  }
  h = pending_header_;
  header_ready_ = false;
  if (h.magic != k_frame_magic || h.version != k_frame_version) {
    close();
    return false;
  }
  if (h.payload_bytes > k_max_payload_bytes || h.handle_count > k_max_handles) {
    close();
    return false;
  }
  header->magic = h.magic;
  header->version = h.version;
  header->type = h.type;
  header->flags = h.flags;
  header->handle_count = h.handle_count;
  header->view_id = h.view_id;
  header->seq = h.seq;
  header->payload_bytes = h.payload_bytes;
  payload->assign(h.payload_bytes, 0);
  // timeout 0 only polls for a header. Once one is ready, finish the frame.
  const uint32_t body_timeout = timeout_ms == 0 ? 10000 : timeout_ms;
  if (h.payload_bytes > 0 &&
      !read_all(payload->data(), h.payload_bytes, body_timeout)) {
    if (!abort_event_ ||
        WaitForSingleObject(abort_event_, 0) != WAIT_OBJECT_0) {
      close();
    }
    return false;
  }
  if (h.handle_count == 0) {
    if (handles) {
      handles->clear();
    }
    return true;
  }
  uint64_t tokens[k_max_handles] = {};
  if (!read_all(tokens, h.handle_count * sizeof(uint64_t), body_timeout)) {
    if (!abort_event_ ||
        WaitForSingleObject(abort_event_, 0) != WAIT_OBJECT_0) {
      close();
    }
    return false;
  }
  if (handles) {
    handles->clear();
    handles->reserve(h.handle_count);
    for (uint16_t i = 0; i < h.handle_count; ++i) {
      handles->push_back(handle_from_token(tokens[i]));
    }
  }
  return true;
}

}  // namespace ipc
}  // namespace base
