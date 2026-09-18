// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/chrome_bridge.h"

#include "app/cef/cef_map_slot.h"
#include "app/views/map_scene.h"

#include "content/public/map_contents.h"
#include "content/public/view_host.h"
#include "tool/interaction.h"
#include "tool/workspace.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>

#include <cstring>
#include <string>
#include <vector>

namespace app {
namespace cef {
namespace {

std::string wide_to_utf8(const wchar_t* wide) {
  if (!wide || !wide[0]) {
    return {};
  }
  const int n =
      WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (n <= 1) {
    return {};
  }
  std::string out(static_cast<size_t>(n - 1), '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, out.data(), n, nullptr, nullptr);
  return out;
}

std::string json_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '\\':
        out += "\\\\";
        break;
      case '"':
        out += "\\\"";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string catalog_json_from_document(const MapScene* document) {
  if (!document) {
    return "[]";
  }
  std::string out = "[";
  bool first = true;
  for (const MapScene::LayerDesc& layer : document->layer_descs()) {
    if (!first) {
      out += ',';
    }
    first = false;
    out += "{\"id\":\"";
    out += json_escape(layer.id);
    out += "\",\"name\":\"";
    out += json_escape(layer.name);
    out += "\",\"visible\":";
    out += layer.visible ? "true" : "false";
    out += '}';
  }
  out += ']';
  return out;
}

}  // namespace

ChromeBridge::ChromeBridge() = default;
ChromeBridge::~ChromeBridge() = default;

void ChromeBridge::set_handlers(LayoutHost* layout,
                                CefMapSlot* slots,
                                int slot_count,
                                content::MapContents* session) {
  layout_ = layout;
  slots_ = slots;
  slot_count_ = slot_count;
  session_ = session;
  wire_draft_observers();
}

void ChromeBridge::set_document(MapScene* document) {
  document_ = document;
  if (slots_) {
    for (int i = 0; i < slot_count_; ++i) {
      slots_[i].set_document(document_);
      slots_[i].set_view_menu_requested(
          [this](POINT screen) { show_view_context_menu(screen); });
    }
  }
}

void ChromeBridge::set_message_box_suppressed(bool suppressed) {
  suppress_message_box_ = suppressed;
}

void ChromeBridge::set_post_json(void (*fn)(void*, const std::string&),
                                 void* user) {
  post_json_ = fn;
  post_user_ = user;
}

void ChromeBridge::notify_ready() {
  ready_ = true;
  BridgeMessage msg;
  msg.api_version = 1;
  msg.type = BridgeType::kReady;
  msg.text =
      "selection.point,selection.clear,edit.append.point,view.pan,"
      "view.zoom_in,view.zoom_out,view.full,view.refresh,view3d.trackball";
  push_event(msg);
  push_catalog_snapshot();
}

bool ChromeBridge::wait_ready(uint32_t timeout_ms) {
  const DWORD end = GetTickCount() + timeout_ms;
  while (!ready_ && GetTickCount() < end) {
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return ready_;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }
    Sleep(10);
  }
  return ready_;
}

bool ChromeBridge::query_has_catalog_and_ambox() const {
  // Structure is provided by web/; host treats Ready + builtins as proof.
  return ready_;
}

void ChromeBridge::select_tab_for_test(int index) {
  BridgeMessage msg;
  msg.api_version = 1;
  msg.type = BridgeType::kSelectMapTab;
  msg.tab_index = index;
  msg.request_id = "self-test-tab";
  const_cast<ChromeBridge*>(this)->handle(msg);
}

bool ChromeBridge::seed_map_document() {
  if (!document_) {
    return false;
  }
  document_->seed_default();
  push_catalog_snapshot();
  invalidate_map_overlays();
  BridgeMessage status;
  status.api_version = 1;
  status.type = BridgeType::kStatus;
  if (document_->last_open_was_ogr() && document_->has_china_extent()) {
    status.text = "Seeded china_plp.geojson (OGR)";
  } else if (document_->last_open_was_ogr()) {
    status.text = "Seeded OGR sample";
  } else {
    status.text = "Seeded demo layer (china_plp missing)";
  }
  push_event(status);
  return document_->last_open_was_ogr() && document_->has_china_extent();
}

void ChromeBridge::push_catalog_snapshot() {
  BridgeMessage legend;
  legend.api_version = 1;
  legend.type = BridgeType::kLegendSnapshot;
  legend.text = catalog_json_from_document(document_);
  push_event(legend);
  BridgeMessage delta;
  delta.api_version = 1;
  delta.type = BridgeType::kCatalogDelta;
  delta.text = legend.text;
  push_event(delta);
}

bool ChromeBridge::open_document_path(const std::string& path) {
  if (!document_ || path.empty()) {
    return false;
  }
  if (session_) {
    const std::string op =
        std::string("{\"op\":\"open\",\"path\":\"") + json_escape(path) + "\"}";
    session_->CatalogCall(op.c_str());
  }
  const bool ogr_ok = document_->open_path(path);
  push_catalog_snapshot();
  invalidate_map_overlays();
  BridgeMessage status;
  status.api_version = 1;
  status.type = BridgeType::kStatus;
  status.text = ogr_ok ? ("OGR opened " + path) : ("Opened (sample) " + path);
  push_event(status);
  return ogr_ok;
}

void ChromeBridge::show_view_context_menu(POINT screen) {
  HWND owner = layout_ ? layout_->hwnd() : nullptr;
  if (!owner) {
    return;
  }
  HMENU menu = CreatePopupMenu();
  if (!menu) {
    return;
  }
  // Labels match leftover 2D view context menu (FIM_2DVIEW).
  AppendMenuW(menu, MF_STRING, 1, L"Zoom In");
  AppendMenuW(menu, MF_STRING, 2, L"Zoom Out");
  AppendMenuW(menu, MF_STRING, 3, L"Pan");
  AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
  AppendMenuW(menu, MF_STRING, 4, L"Full Extent");
  AppendMenuW(menu, MF_STRING, 5, L"Refresh");
  const UINT cmd =
      TrackPopupMenu(menu, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_LEFTALIGN,
                     screen.x, screen.y, 0, owner, nullptr);
  DestroyMenu(menu);
  const char* id = nullptr;
  switch (cmd) {
    case 1:
      id = "view.zoom_in";
      break;
    case 2:
      id = "view.zoom_out";
      break;
    case 3:
      id = "view.pan";
      break;
    case 4:
      id = "view.full";
      break;
    case 5:
      id = "view.refresh";
      break;
    default:
      return;
  }
  activate_tool(id);
}

bool ChromeBridge::handle_json(std::string_view json) {
  BridgeMessage msg;
  if (!detail::parse_bridge_message(json, &msg)) {
    send_error({}, 1, "parse failed");
    return false;
  }
  handle(msg);
  return true;
}

void ChromeBridge::push_event(const BridgeMessage& msg) {
  if (msg.type == BridgeType::kStatus) {
    last_status_ = msg.text;
  }
  if (!post_json_) {
    return;
  }
  post_json_(post_user_, detail::serialize_bridge_message(msg));
}

void ChromeBridge::send_error(const std::string& request_id,
                              int code,
                              const char* text) {
  BridgeMessage msg;
  msg.api_version = 1;
  msg.type = BridgeType::kError;
  msg.request_id = request_id;
  msg.error_code = code;
  msg.text = text ? text : "";
  push_event(msg);
}

void ChromeBridge::send_ack(const std::string& request_id) {
  BridgeMessage msg;
  msg.api_version = 1;
  msg.type = BridgeType::kAck;
  msg.request_id = request_id;
  push_event(msg);
}

std::string ChromeBridge::map_alias(std::string_view id) const {
  if (id == "select" || id == "identify") {
    return "selection.point";
  }
  if (id == "pan") {
    return "view.pan";
  }
  return std::string(id);
}

content::ViewHost* ChromeBridge::active_view_host() const {
  CefMapSlot* slot = active_slot();
  return slot ? slot->view_host() : nullptr;
}

CefMapSlot* ChromeBridge::active_slot() const {
  if (!layout_ || !slots_ || slot_count_ <= 0) {
    return nullptr;
  }
  const int idx = layout_->active_tab();
  if (idx < 0 || idx >= slot_count_) {
    return nullptr;
  }
  return &slots_[idx];
}

void ChromeBridge::invalidate_map_overlays() {
  if (!slots_) {
    return;
  }
  for (int i = 0; i < slot_count_; ++i) {
    slots_[i].invalidate();
  }
}

void ChromeBridge::wire_draft_observers() {
  if (!slots_) {
    return;
  }
  auto on_draft = [this](const tool::Draft& draft) { handle_draft(draft); };
  for (int i = 0; i < slot_count_; ++i) {
    content::ViewHost* host = slots_[i].view_host();
    if (host && host->workspace()) {
      host->workspace()->set_draft_observer(on_draft);
    }
  }
}

void ChromeBridge::handle_draft(const tool::Draft& draft) {
  if (!document_) {
    return;
  }
  content::ViewHost* host = active_view_host();
  tool::Interaction* cur =
      host && host->workspace() ? host->workspace()->stack().current()
                                : nullptr;
  const char* tool_id = cur ? cur->id() : nullptr;

  if (tool_id && std::strcmp(tool_id, "view.pan") == 0 &&
      draft.kind == tool::DraftKind::kRect && draft.points.size() >= 2) {
    const int dx = draft.points.back().x_px - draft.points.front().x_px;
    const int dy = draft.points.back().y_px - draft.points.front().y_px;
    document_->apply_pan(dx, dy);
    invalidate_map_overlays();
    return;
  }
  if (tool_id && std::strcmp(tool_id, "view.zoom_in") == 0 &&
      !draft.points.empty()) {
    document_->apply_zoom_at(draft.points.front().x_px,
                             draft.points.front().y_px, 1.25);
    invalidate_map_overlays();
    return;
  }
  if (tool_id && std::strcmp(tool_id, "view.zoom_out") == 0 &&
      !draft.points.empty()) {
    document_->apply_zoom_at(draft.points.front().x_px,
                             draft.points.front().y_px, 0.8);
    invalidate_map_overlays();
  }
}

bool ChromeBridge::activate_tool(const std::string& id) {
  content::ViewHost* host = active_view_host();
  if (!host) {
    return false;
  }
  const uint32_t view_id = active_slot() ? active_slot()->view_id() : 0;
  bool ok = host->execute(id, view_id);
  if (!ok) {
    ok = host->activate(id);
  }
  if (!ok) {
    return false;
  }
  if (document_ && (id == "view.full" || id == "view.refresh")) {
    if (CefMapSlot* slot = active_slot()) {
      RECT rc = {};
      if (slot->native_hwnd()) {
        GetClientRect(slot->native_hwnd(), &rc);
      }
      const int w = rc.right > 0 ? rc.right : 800;
      const int h = rc.bottom > 0 ? rc.bottom : 600;
      document_->fit_extent(w, h);
      invalidate_map_overlays();
    }
  }
  BridgeMessage status;
  status.api_version = 1;
  status.type = BridgeType::kStatus;
  status.text = "Activated " + id;
  if (id == "selection.clear") {
    status.text = "Selection cleared";
  } else if (id == "edit.append.point") {
    status.text = "Committed";
  } else if (id == "view.full") {
    status.text = "Full extent";
  } else if (id == "view.refresh") {
    status.text = "Refreshed";
  }
  push_event(status);
  return true;
}

void ChromeBridge::handle(const BridgeMessage& msg) {
  if (!detail::is_supported_api_version(msg.api_version)) {
    send_error(msg.request_id, 40, "unsupported api_version");
    return;
  }

  switch (msg.type) {
    case BridgeType::kActivateTool: {
      const std::string id = map_alias(msg.command_id);
      if (!activate_tool(id)) {
        send_error(msg.request_id, 3, "unknown command_id");
        BridgeMessage status;
        status.api_version = 1;
        status.type = BridgeType::kStatus;
        status.text = "Unknown tool " + id;
        push_event(status);
        return;
      }
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kCatalogOp: {
      if (session_) {
        session_->CatalogCall(msg.op_json.c_str());
      }
      if (msg.op_json.find("\"op\":\"refresh\"") != std::string::npos) {
        push_catalog_snapshot();
      }
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kSelectMapTab: {
      if (layout_) {
        layout_->set_active_tab(msg.tab_index);
      }
      if (slots_ && layout_) {
        const int active = layout_->active_tab();
        for (int i = 0; i < slot_count_; ++i) {
          slots_[i].set_visible(i == active);
          if (i == active) {
            slots_[i].sync_layout(layout_->map_slot_rect(), layout_->dpi_scale());
          }
        }
      }
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kOpenFile: {
      std::string path = msg.path;
      if (path.empty() && !suppress_message_box_) {
        IFileOpenDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                       CLSCTX_INPROC_SERVER,
                                       IID_PPV_ARGS(&dialog)))) {
          COMDLG_FILTERSPEC filters[] = {
              {L"GIS vectors", L"*.shp;*.gpkg;*.geojson;*.json"},
              {L"All files", L"*.*"},
          };
          dialog->SetFileTypes(2, filters);
          if (SUCCEEDED(dialog->Show(layout_ ? layout_->hwnd() : nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
              PWSTR wpath = nullptr;
              if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) &&
                  wpath) {
                path = wide_to_utf8(wpath);
                CoTaskMemFree(wpath);
              }
              item->Release();
            }
          }
          dialog->Release();
        }
      }
      if (!path.empty()) {
        open_document_path(path);
      }
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kLayoutSlot: {
      if (layout_) {
        layout_->set_map_slot_rect(msg.slot_rect);
        if (slots_) {
          const int active = layout_->active_tab();
          if (active >= 0 && active < slot_count_) {
            slots_[active].sync_layout(layout_->map_slot_rect(),
                                       msg.dpi > 0.f ? msg.dpi
                                                     : layout_->dpi_scale());
          }
        }
      }
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kQueryState: {
      BridgeMessage status;
      status.api_version = 1;
      status.type = BridgeType::kStatus;
      status.text = last_status_.empty() ? "ok" : last_status_;
      push_event(status);
      push_catalog_snapshot();
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kExit:
      PostQuitMessage(0);
      send_ack(msg.request_id);
      return;
    default:
      send_error(msg.request_id, 4, "unknown type");
      return;
  }
}

}  // namespace cef
}  // namespace app
