// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef CONTENT_COMMON_HOST_MSG_REGISTRY_H_
#define CONTENT_COMMON_HOST_MSG_REGISTRY_H_

#include <cstdint>
#include <string_view>

#include "content/common/host_protocol.h"

namespace content {

// Name table for HostMsg (mojom-shaped registry without an IDL toolchain).
inline constexpr std::string_view host_msg_name(HostMsg type) {
  switch (type) {
    case HostMsg::kHello:
      return "Hello";
    case HostMsg::kHelloAck:
      return "HelloAck";
    case HostMsg::kOpenView:
      return "OpenView";
    case HostMsg::kViewReady:
      return "ViewReady";
    case HostMsg::kCloseView:
      return "CloseView";
    case HostMsg::kAttachSurface:
      return "AttachSurface";
    case HostMsg::kResizeSurface:
      return "ResizeSurface";
    case HostMsg::kSetExtent:
      return "SetExtent";
    case HostMsg::kExtentChanged:
      return "ExtentChanged";
    case HostMsg::kSetSelection:
      return "SetSelection";
    case HostMsg::kSelectionChanged:
      return "SelectionChanged";
    case HostMsg::kLegendQuery:
      return "LegendQuery";
    case HostMsg::kLegendSnapshot:
      return "LegendSnapshot";
    case HostMsg::kCatalogOp:
      return "CatalogOp";
    case HostMsg::kCatalogDelta:
      return "CatalogDelta";
    case HostMsg::kActivateTool:
      return "ActivateTool";
    case HostMsg::kPluginCall:
      return "PluginCall";
    case HostMsg::kPluginEvent:
      return "PluginEvent";
    case HostMsg::kPrintRequest:
      return "PrintRequest";
    case HostMsg::kPrintPage:
      return "PrintPage";
    case HostMsg::kRenderDied:
      return "RenderDied";
    case HostMsg::kResetGpu:
      return "ResetGpu";
    case HostMsg::kIoCall:
      return "IoCall";
    case HostMsg::kPointerEvent:
      return "PointerEvent";
    case HostMsg::kFrameReady:
      return "FrameReady";
    case HostMsg::kViewCursor:
      return "ViewCursor";
    case HostMsg::kContextMenu:
      return "ContextMenu";
    case HostMsg::kSharedHandle:
      return "SharedHandle";
    case HostMsg::kTextCommit:
      return "TextCommit";
    case HostMsg::kShutdown:
      return "Shutdown";
    case HostMsg::kSetRenderBackend:
      return "SetRenderBackend";
    default:
      return {};
  }
}

inline bool is_known_host_msg(uint16_t type) {
  if (type < static_cast<uint16_t>(HostMsg::kHello) ||
      type > static_cast<uint16_t>(HostMsg::kSetRenderBackend)) {
    return false;
  }
  return !host_msg_name(static_cast<HostMsg>(type)).empty();
}

// Capability bits advertised in HelloBody.caps (protocol >= 4).
enum class HostCap : uint64_t {
  kNone = 0,
  kSharedHandle = 1ull << 0,
  kDataPipe = 1ull << 1,
  kInvitation = 1ull << 2,
  kChannelProxy = 1ull << 3,
  kBroker = 1ull << 4,
};

inline constexpr uint64_t k_host_caps_shell =
    static_cast<uint64_t>(HostCap::kSharedHandle) |
    static_cast<uint64_t>(HostCap::kDataPipe) |
    static_cast<uint64_t>(HostCap::kInvitation) |
    static_cast<uint64_t>(HostCap::kChannelProxy) |
    static_cast<uint64_t>(HostCap::kBroker);

inline constexpr uint64_t k_host_caps_gpu =
    static_cast<uint64_t>(HostCap::kSharedHandle) |
    static_cast<uint64_t>(HostCap::kDataPipe) |
    static_cast<uint64_t>(HostCap::kInvitation);

}  // namespace content

#endif  // CONTENT_COMMON_HOST_MSG_REGISTRY_H_
