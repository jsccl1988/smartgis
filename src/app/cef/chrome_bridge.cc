// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/cef/chrome_bridge.h"

#include "app/cef/cef_map_slot.h"

#include "content/public/map_contents.h"
#include "content/public/view_host.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <shobjidl.h>

#include <cstring>

namespace app {
namespace cef {

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
      "view.zoom_in,view.zoom_out,view3d.trackball";
  push_event(msg);
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
  if (!layout_ || !slots_ || slot_count_ <= 0) {
    return nullptr;
  }
  const int idx = layout_->active_tab();
  if (idx < 0 || idx >= slot_count_) {
    return nullptr;
  }
  return slots_[idx].view_host();
}

void ChromeBridge::handle(const BridgeMessage& msg) {
  if (!detail::is_supported_api_version(msg.api_version)) {
    send_error(msg.request_id, 40, "unsupported api_version");
    return;
  }

  switch (msg.type) {
    case BridgeType::kActivateTool: {
      content::ViewHost* host = active_view_host();
      if (!host) {
        send_error(msg.request_id, 2, "no view host");
        return;
      }
      const std::string id = map_alias(msg.command_id);
      const uint32_t view_id =
          (layout_ && slots_ && layout_->active_tab() < slot_count_)
              ? slots_[layout_->active_tab()].view_id()
              : msg.view_id;
      bool ok = host->execute(id, view_id);
      if (!ok) {
        ok = host->activate(id);
      }
      if (!ok) {
        send_error(msg.request_id, 3, "unknown command_id");
        BridgeMessage status;
        status.api_version = 1;
        status.type = BridgeType::kStatus;
        status.text = "Unknown tool " + id;
        push_event(status);
        return;
      }
      BridgeMessage status;
      status.api_version = 1;
      status.type = BridgeType::kStatus;
      status.text = "Activated " + id;
      if (id == "selection.clear") {
        status.text = "Selection cleared";
      } else if (id == "edit.append.point") {
        status.text = "Committed";
      }
      push_event(status);
      send_ack(msg.request_id);
      return;
    }
    case BridgeType::kCatalogOp: {
      if (session_) {
        session_->CatalogCall(msg.op_json.c_str());
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
      if (msg.path.empty() && !suppress_message_box_) {
        IFileOpenDialog* dialog = nullptr;
        if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr,
                                       CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
          if (SUCCEEDED(dialog->Show(layout_ ? layout_->hwnd() : nullptr))) {
            IShellItem* item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item) {
              PWSTR path = nullptr;
              if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) &&
                  path) {
                CoTaskMemFree(path);
              }
              item->Release();
            }
          }
          dialog->Release();
        }
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
      BridgeMessage legend;
      legend.api_version = 1;
      legend.type = BridgeType::kLegendSnapshot;
      legend.text = "[]";
      push_event(legend);
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
