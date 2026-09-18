// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/common/ipc.h"

#include <cstring>


namespace content {

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

bool Pipe::recv(FrameHeader* header,
               std::vector<uint8_t>* payload,
               uint32_t timeout_ms) {
  return recv(header, payload, nullptr, timeout_ms);
}

bool Pipe::recv(FrameHeader* header,
               std::vector<uint8_t>* payload,
               std::vector<base::ipc::PlatformHandle>* handles,
               uint32_t timeout_ms) {
  if (!header || !payload) {
    return false;
  }
  base::ipc::Frame f;
  if (!ch_.recv(&f, payload, handles, timeout_ms)) {
    return false;
  }
  header->magic = f.magic;
  header->version = f.version;
  header->type = f.type;
  header->flags = f.flags;
  header->handle_count = f.handle_count;
  header->view_id = f.view_id;
  header->seq = f.seq;
  header->payload_bytes = f.payload_bytes;
  return true;
}

}  // namespace detail
}  // namespace content
