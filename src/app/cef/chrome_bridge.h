// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#ifndef APP_CEF_CHROME_BRIDGE_H_
#define APP_CEF_CHROME_BRIDGE_H_

#include <cstdint>
#include <string>
#include <string_view>

#include "app/cef/layout_host.h"
#include "tool/gestures.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace content {
class MapContents;
class ViewHost;
}  // namespace content

namespace app {
class MapScene;

namespace cef {

class CefMapSlot;

enum class BridgeType {
  kActivateTool,
  kCatalogOp,
  kSelectMapTab,
  kOpenFile,
  kLayoutSlot,
  kQueryState,
  kExit,
  kAck,
  kError,
  kStatus,
  kSelectionChanged,
  kExtentChanged,
  kLegendSnapshot,
  kCatalogDelta,
  kViewCursor,
  kReady,
  kPointerEvent,
  kUnknown,
};

struct BridgeMessage {
  int api_version = 0;
  BridgeType type = BridgeType::kUnknown;
  std::string request_id;
  uint32_t view_id = 0;
  std::string command_id;
  std::string op_json;
  int tab_index = 0;
  std::string path;
  std::string slot_id;
  RectPx slot_rect;
  float dpi = 1.f;
  std::string query_what;
  std::string text;
  int error_code = 0;
  // PointerEvent: wheel | drag | pinch | ldown | lup | move | …
  std::string pointer_kind;
  int wheel = 0;
  uint32_t flags = 0;
  float scale = 1.f;
  // 0/1 = single; >=2 = multitouch midpoint (see content::InputEvent).
  uint32_t pointer_count = 0;
};

namespace detail {

// Typed BridgeType JSON, or SG20-style topic envelopes
// ({"topic":"tool.command"|"panel.action","payload":{"command":"view.pan"}}).
bool parse_bridge_message(std::string_view json, BridgeMessage* out);
bool is_supported_api_version(int version);
BridgeType bridge_type_from_string(std::string_view type);
const char* bridge_type_to_string(BridgeType type);
std::string serialize_bridge_message(const BridgeMessage& msg);

}  // namespace detail

// Host-side ChromeBridge. ProcessMessage JSON only — no GIS pointers in V8.
// CEF-dependent methods are implemented in chrome_bridge.cc (smt_has_cef).
class ChromeBridge {
 public:
  ChromeBridge();
  ~ChromeBridge();

  void set_handlers(LayoutHost* layout,
                    CefMapSlot* slots,
                    int slot_count,
                    content::MapContents* session);
  void set_document(MapScene* document);
  void set_message_box_suppressed(bool suppressed);

  void notify_ready();
  bool wait_ready(uint32_t timeout_ms);
  bool ready() const { return ready_; }

  bool query_has_catalog_and_ambox() const;
  void select_tab_for_test(int index);
  // Seed china_city (fallback china_plp) and push CatalogDelta. Returns true on
  // OGR China.
  bool seed_map_document();
  void push_catalog_snapshot();
  void show_view_context_menu(POINT screen);
  bool open_document_path(const std::string& path);

  // Called after JSON is extracted from a CefProcessMessage (browser process).
  bool handle_json(std::string_view json);

  // Forward wheel / drag / pinch to ViewHost + MapContents::Dispatch
  // (host protocol kPointerEvent). Returns false if kind is unknown.
  bool dispatch_pointer(const BridgeMessage& msg);

  void push_event(const BridgeMessage& msg);
  void set_post_json(void (*fn)(void* user, const std::string& json), void* user);

  const std::string& last_status() const { return last_status_; }
  MapScene* document() { return document_; }
  const MapScene* document() const { return document_; }

 private:
  void handle(const BridgeMessage& msg);
  void send_error(const std::string& request_id, int code, const char* text);
  void send_ack(const std::string& request_id);
  std::string map_alias(std::string_view id) const;
  content::ViewHost* active_view_host() const;
  CefMapSlot* active_slot() const;
  void wire_draft_observers();
  void handle_draft(const tool::Draft& draft);
  void invalidate_map_overlays();
  bool activate_tool(const std::string& id);
  void catalog_open_sample();
  void apply_default_tools();

  LayoutHost* layout_ = nullptr;
  CefMapSlot* slots_ = nullptr;
  int slot_count_ = 0;
  content::MapContents* session_ = nullptr;
  MapScene* document_ = nullptr;
  bool ready_ = false;
  bool suppress_message_box_ = false;
  std::string last_status_;
  void (*post_json_)(void*, const std::string&) = nullptr;
  void* post_user_ = nullptr;
};

}  // namespace cef
}  // namespace app

#endif  // APP_CEF_CHROME_BRIDGE_H_
