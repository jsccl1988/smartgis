// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/common/ipc.h"

#include <cstdlib>
#include <cstring>

namespace content {
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

std::wstring pipe_name_for_pid(uint32_t pid) {
  wchar_t buf[64];
  swprintf_s(buf, L"smartgis-host-%u", pid);
  return buf;
}

std::wstring pipe_path_for_pid(uint32_t pid) {
  return pipe_path_from_name(pipe_name_for_pid(pid));
}

std::wstring pipe_path_from_name(const std::wstring& name) {
  if (name.rfind(L"\\\\.\\pipe\\", 0) == 0) {
    return name;
  }
  return L"\\\\.\\pipe\\" + name;
}

const char* view_kind_json(ViewKind kind) {
  switch (kind) {
    case ViewKind::kMapData:
      return "map_data";
    case ViewKind::kScene3d:
      return "scene_3d";
    case ViewKind::kMapEdit:
    default:
      return "map_edit";
  }
}

ViewKind view_kind_from_json(const char* json) {
  if (!json) {
    return ViewKind::kMapEdit;
  }
  if (std::strstr(json, "map_data")) {
    return ViewKind::kMapData;
  }
  if (std::strstr(json, "scene_3d")) {
    return ViewKind::kScene3d;
  }
  return ViewKind::kMapEdit;
}

const char* present_mode_json(PresentMode mode) {
  switch (mode) {
    case PresentMode::kChildHwnd:
      return "child_hwnd";
    case PresentMode::kSoftwareDib:
      return "software_dib";
    case PresentMode::kSharedTexture:
    default:
      return "shared_texture";
  }
}

PresentMode present_mode_from_json(const char* json) {
  if (!json) {
    return PresentMode::kSharedTexture;
  }
  if (std::strstr(json, "child_hwnd")) {
    return PresentMode::kChildHwnd;
  }
  if (std::strstr(json, "software_dib")) {
    return PresentMode::kSoftwareDib;
  }
  return PresentMode::kSharedTexture;
}

namespace detail {

Pipe::Pipe() : pipe_(INVALID_HANDLE_VALUE) {
  InitializeCriticalSection(&write_lock_);
}

Pipe::~Pipe() {
  close();
  DeleteCriticalSection(&write_lock_);
}

bool Pipe::create_server(const std::wstring& path) {
  close();
  pipe_ = CreateNamedPipeW(path.c_str(),
                           PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                           PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT,
                           1, 65536, 65536, 0, nullptr);
  return pipe_ != INVALID_HANDLE_VALUE;
}

bool Pipe::wait_client(uint32_t timeout_ms) {
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

bool Pipe::connect_client(const std::wstring& path, uint32_t timeout_ms) {
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

void Pipe::close() {
  if (pipe_ != INVALID_HANDLE_VALUE) {
    CancelIo(pipe_);
    CloseHandle(pipe_);
    pipe_ = INVALID_HANDLE_VALUE;
  }
}

bool Pipe::is_open() const {
  return pipe_ != INVALID_HANDLE_VALUE;
}

bool Pipe::write_all(const void* data, uint32_t bytes, uint32_t timeout_ms) {
  if (pipe_ == INVALID_HANDLE_VALUE || !data) {
    return false;
  }
  OVERLAPPED ov = {};
  ov.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  if (!ov.hEvent) {
    return false;
  }
  DWORD wrote = 0;
  const BOOL ok =
      WriteFile(pipe_, data, bytes, &wrote, &ov);
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

bool Pipe::read_all(void* data, uint32_t bytes, uint32_t timeout_ms) {
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

bool Pipe::send(HostMsg type,
                uint16_t flags,
                uint32_t view_id,
                const void* payload,
                uint32_t payload_bytes) {
  FrameHeader h = {};
  h.magic = kHostMagic;
  h.version = kHostProtocolVersion;
  h.type = static_cast<uint16_t>(type);
  h.flags = flags;
  h.view_id = view_id;
  h.payload_bytes = payload_bytes;
  EnterCriticalSection(&write_lock_);
  const bool ok_h = write_all(&h, sizeof(h), 10000);
  const bool ok_p =
      payload_bytes == 0 || write_all(payload, payload_bytes, 10000);
  LeaveCriticalSection(&write_lock_);
  return ok_h && ok_p;
}

bool Pipe::send_json(HostMsg type, uint32_t view_id, const std::string& json) {
  return send(type, static_cast<uint16_t>(HostFlag::kJson), view_id,
              json.data(), static_cast<uint32_t>(json.size()));
}

bool Pipe::send_binary(HostMsg type,
                       uint32_t view_id,
                       const void* payload,
                       uint32_t payload_bytes) {
  return send(type, static_cast<uint16_t>(HostFlag::kBinary), view_id, payload,
              payload_bytes);
}

bool Pipe::recv(FrameHeader* header,
                std::vector<uint8_t>* payload,
                uint32_t timeout_ms) {
  if (!header || !payload) {
    return false;
  }
  if (!read_all(header, sizeof(*header), timeout_ms)) {
    return false;
  }
  if (header->magic != kHostMagic ||
      header->version != kHostProtocolVersion) {
    close();
    return false;
  }
  payload->assign(header->payload_bytes, 0);
  if (header->payload_bytes == 0) {
    return true;
  }
  if (!read_all(payload->data(), header->payload_bytes, timeout_ms)) {
    return false;
  }
  return true;
}

std::string json_get_string(const std::string& json, const char* key) {
  const std::string pat = std::string("\"") + key + "\"";
  const size_t k = json.find(pat);
  if (k == std::string::npos) {
    return {};
  }
  const size_t colon = json.find(':', k + pat.size());
  if (colon == std::string::npos) {
    return {};
  }
  size_t q1 = json.find('"', colon);
  if (q1 == std::string::npos) {
    return {};
  }
  const size_t q2 = json.find('"', q1 + 1);
  if (q2 == std::string::npos) {
    return {};
  }
  return json.substr(q1 + 1, q2 - q1 - 1);
}

int json_get_int(const std::string& json, const char* key, int fallback) {
  const std::string pat = std::string("\"") + key + "\"";
  const size_t k = json.find(pat);
  if (k == std::string::npos) {
    return fallback;
  }
  const size_t colon = json.find(':', k + pat.size());
  if (colon == std::string::npos) {
    return fallback;
  }
  return std::atoi(json.c_str() + colon + 1);
}

double json_get_double(const std::string& json, const char* key, double fallback) {
  const std::string pat = std::string("\"") + key + "\"";
  const size_t k = json.find(pat);
  if (k == std::string::npos) {
    return fallback;
  }
  const size_t colon = json.find(':', k + pat.size());
  if (colon == std::string::npos) {
    return fallback;
  }
  return std::atof(json.c_str() + colon + 1);
}

bool json_has_key(const std::string& json, const char* key) {
  const std::string pat = std::string("\"") + key + "\"";
  return json.find(pat) != std::string::npos;
}

}  // namespace detail
}  // namespace content
