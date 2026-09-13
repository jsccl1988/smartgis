// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_PUBLIC_HOST_PROTOCOL_H
#define CONTENT_PUBLIC_HOST_PROTOCOL_H

#include <cstdint>
#include <string>

#include "content/public/map_types.h"

// Named-pipe Host ABI 0.7: length-prefixed frames, pickle (BinarySink) bodies.
// Pipe: \\.\pipe\smartgis-host-<ui-pid>
// Child argv: --parent-pid=<pid> --pipe=smartgis-host-<pid> --session=<guid>
namespace content {

inline constexpr uint32_t kHostMagic = 0x31544D53u;  // 'SMT1' LE
inline constexpr uint16_t kHostProtocolVersion = 2;

enum class HostMsg : uint16_t {
  kHello = 1,
  kHelloAck = 2,
  kOpenView = 3,
  kViewReady = 4,
  kCloseView = 5,
  kAttachSurface = 6,
  kResizeSurface = 7,
  kSetExtent = 8,
  kExtentChanged = 9,
  kSetSelection = 10,
  kSelectionChanged = 11,
  kLegendQuery = 12,
  kLegendSnapshot = 13,
  kCatalogOp = 14,
  kCatalogDelta = 15,
  kActivateTool = 16,
  kPluginCall = 17,
  kPluginEvent = 18,
  kPrintRequest = 19,
  kPrintPage = 20,
  kRenderDied = 21,
  kResetGpu = 22,
  kIoCall = 23,
  kPointerEvent = 24,
  kFrameReady = 25,
  kViewCursor = 26,
  kContextMenu = 27,
  kSharedHandle = 28,
  kTextCommit = 29,
  kShutdown = 30,
};

enum class HostFlag : uint16_t {
  kJson = 1,
  kBinary = 2,
  kNeedAck = 4,
};

#pragma pack(push, 1)
// Length-prefixed named-pipe header. Payload bytes are pickle, not JSON.
struct FrameHeader {
  uint32_t magic;
  uint16_t version;
  uint16_t type;
  uint16_t flags;
  uint32_t view_id;
  uint32_t payload_bytes;
};

// Pointer / wheel / key event on the pickle wire.
struct PointerEventWire {
  uint64_t t_qpc;
  uint32_t kind;
  uint32_t flags;
  int32_t x_px;
  int32_t y_px;
  int32_t wheel;
  uint32_t key;
  float dpi;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(t_qpc, kind, flags, x_px, y_px, wheel, key, dpi);
  }
};

// DXGI shared texture (or DIB) duplicated into the chrome process.
struct SharedHandleWire {
  uint32_t generation;
  uint32_t width_px;
  uint32_t height_px;
  uint32_t format;
  uint64_t nt_handle;
  uint32_t present_mode;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(generation, width_px, height_px, format, nt_handle, present_mode);
  }
};

// GPU finished a generation; chrome may present.
struct FrameReadyWire {
  uint32_t generation;
  uint64_t fence;
  uint32_t cursor_hint;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(generation, fence, cursor_hint);
  }
};
#pragma pack(pop)

// Hello / HelloAck. role is "gpu" or "chrome".
struct HelloBody {
  uint32_t protocol = kHostProtocolVersion;
  std::string role;
  std::string gpu;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(protocol, role, gpu);
  }
};

// OpenView: ViewKind stored as uint32.
struct OpenViewBody {
  uint32_t kind = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(kind);
  }
};

// AttachSurface / SetPresentMode / SetVisible.
struct AttachSurfaceBody {
  uint32_t present_mode = 0;
  uint32_t visible = 1;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(present_mode, visible);
  }
};

// ResizeSurface in CSS pixels plus DPI.
struct ResizeSurfaceBody {
  uint32_t w = 64;
  uint32_t h = 64;
  float dpi = 96.f;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(w, h, dpi);
  }
};

// Map extent in world coordinates.
struct ExtentWire {
  double xmin = 0;
  double ymin = 0;
  double xmax = 0;
  double ymax = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(xmin, ymin, xmax, ymax);
  }
};

// SetSelection count only; feature ids stay in-process in v1.
struct SelectionBody {
  uint32_t count = 0;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(count);
  }
};

// ActivateTool pickle body.
struct ToolBody {
  std::string tool_id;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(tool_id);
  }
};

// Catalog / leftover JSON ops as a single pickle string.
struct JsonBody {
  std::string json;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(json);
  }
};

// PluginCall envelope: opaque bytes for future utility-process plugins.
struct PluginCallBody {
  std::string plugin_id;
  std::string method;
  std::string bytes;

  template <typename Ar>
  void archive(Ar&& ar) {
    ar(plugin_id, method, bytes);
  }
};

static_assert(sizeof(FrameHeader) == 18, "Host ABI frame header");

inline constexpr uint32_t kDxgiBgraUnorm = 87;  // DXGI_FORMAT_B8G8R8A8_UNORM

std::wstring pipe_name_for_pid(uint32_t pid);
std::wstring pipe_path_for_pid(uint32_t pid);
std::wstring pipe_path_from_name(const std::wstring& name);

const char* view_kind_json(ViewKind kind);
ViewKind view_kind_from_json(const char* json);
const char* present_mode_json(PresentMode mode);
PresentMode present_mode_from_json(const char* json);

}  // namespace content

#endif  // CONTENT_PUBLIC_HOST_PROTOCOL_H
