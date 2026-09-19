// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/app/renderer_main.h"

#include <cstdio>
#include <cwchar>
#include <map>
#include <memory>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "base/ipc/channel.h"
#include "base/ipc/codec.h"
#include "base/ipc/invitation.h"
#include "content/public/event_bus.h"
#include "content/public/host_protocol.h"
#include "gis/edit/edit_session.h"
#include "tool/command.h"
#include "tool/workspace.h"

namespace content {
namespace {

bool argv_has_flag(int argc, wchar_t** argv, const wchar_t* flag) {
  if (!argv) {
    return false;
  }
  for (int i = 0; i < argc; ++i) {
    if (argv[i] && wcscmp(argv[i], flag) == 0) {
      return true;
    }
  }
  return false;
}

uint32_t parent_pid_from_argv(int argc, wchar_t** argv) {
  if (!argv) {
    return 0;
  }
  for (int i = 1; i < argc; ++i) {
    if (argv[i] && wcsncmp(argv[i], L"--parent-pid=", 13) == 0) {
      return static_cast<uint32_t>(_wtoi(argv[i] + 13));
    }
  }
  return 0;
}

// In-process edit log so Workspace can commit without the sdb DLL.
class RendererEdits final : public gis::EditSession {
 public:
  bool commit(const gis::FeatureMutation&) override { return true; }
  bool undo() override { return false; }
  bool redo() override { return false; }
  bool can_undo() const override { return false; }
  bool can_redo() const override { return false; }
};

struct ViewSlot {
  EventBus events;
  RendererEdits edits;
  std::unique_ptr<tool::Workspace> workspace;
  ViewKind kind = ViewKind::kMapEdit;
  Extent2 extent{};
};

InputEvent input_from_wire(const PointerEventWire& w) {
  InputEvent e{};
  e.kind = static_cast<InputEvent::Kind>(w.kind);
  e.flags = w.flags;
  e.x_px = w.x_px;
  e.y_px = w.y_px;
  e.wheel = w.wheel;
  e.key = w.key;
  e.t_qpc = w.t_qpc;
  e.pointer_count = w.pointer_count;
  return e;
}

bool run_renderer_loop(base::ipc::Channel* ch, HANDLE parent) {
  std::map<uint32_t, std::unique_ptr<ViewSlot>> views;

  HelloBody hello;
  hello.role = "renderer";
  if (!ch->send_msg(static_cast<uint16_t>(HostMsg::kHello), 0, hello)) {
    return false;
  }

  for (;;) {
    if (parent && WaitForSingleObject(parent, 0) == WAIT_OBJECT_0) {
      break;
    }
    if (!ch->wait_readable(parent, INFINITE)) {
      if (!ch->is_open()) {
        break;
      }
      if (parent && WaitForSingleObject(parent, 0) == WAIT_OBJECT_0) {
        break;
      }
      continue;
    }
    base::ipc::Frame h;
    std::vector<uint8_t> payload;
    if (!ch->recv(&h, &payload, 10000)) {
      if (!ch->is_open()) {
        break;
      }
      continue;
    }
    const auto type = static_cast<HostMsg>(h.type);
    if (type == HostMsg::kShutdown) {
      break;
    }
    if (type == HostMsg::kHelloAck) {
      continue;
    }
    if (type == HostMsg::kOpenView) {
      OpenViewBody body;
      base::ipc::decode(payload.data(), payload.size(), &body);
      auto slot = std::make_unique<ViewSlot>();
      slot->kind = static_cast<ViewKind>(body.kind);
      slot->workspace =
          std::make_unique<tool::Workspace>(&slot->events, &slot->edits);
      views[h.view_id] = std::move(slot);
      ch->send_empty(static_cast<uint16_t>(HostMsg::kViewReady), h.view_id);
      continue;
    }
    if (type == HostMsg::kCloseView) {
      views.erase(h.view_id);
      continue;
    }
    ViewSlot* slot = nullptr;
    auto it = views.find(h.view_id);
    if (it != views.end()) {
      slot = it->second.get();
    }
    if (type == HostMsg::kActivateTool) {
      ToolBody body;
      if (base::ipc::decode(payload.data(), payload.size(), &body) && slot &&
          slot->workspace) {
        if (!slot->workspace->execute(body.tool_id,
                                      tool::CommandArgs{h.view_id, {}})) {
          slot->workspace->activate(body.tool_id);
        }
      }
      continue;
    }
    if (type == HostMsg::kPointerEvent || type == HostMsg::kTextCommit) {
      PointerEventWire w;
      if (base::ipc::decode(payload.data(), payload.size(), &w) && slot &&
          slot->workspace) {
        slot->workspace->dispatch_input(input_from_wire(w));
      }
      continue;
    }
    if (type == HostMsg::kSetExtent) {
      ExtentWire w;
      if (base::ipc::decode(payload.data(), payload.size(), &w)) {
        if (slot) {
          slot->extent.xmin = w.xmin;
          slot->extent.ymin = w.ymin;
          slot->extent.xmax = w.xmax;
          slot->extent.ymax = w.ymax;
        }
        ch->send_msg(static_cast<uint16_t>(HostMsg::kExtentChanged), h.view_id,
                     w);
      }
      continue;
    }
    if (type == HostMsg::kCatalogOp) {
      JsonBody in;
      JsonBody out;
      if (base::ipc::decode(payload.data(), payload.size(), &in) && slot &&
          slot->workspace && !in.json.empty()) {
        slot->workspace->execute(in.json, tool::CommandArgs{h.view_id, {}});
      }
      out.json = "{}";
      ch->send_msg(static_cast<uint16_t>(HostMsg::kCatalogDelta), h.view_id,
                   out);
      continue;
    }
    if (type == HostMsg::kLegendQuery) {
      ch->send_empty(static_cast<uint16_t>(HostMsg::kLegendSnapshot),
                     h.view_id);
      continue;
    }
    if (type == HostMsg::kPluginCall) {
      PluginCallBody body;
      if (base::ipc::decode(payload.data(), payload.size(), &body) && slot &&
          slot->workspace) {
        if (!slot->workspace->execute(body.method,
                                      tool::CommandArgs{h.view_id, {}})) {
          slot->workspace->execute(body.plugin_id,
                                   tool::CommandArgs{h.view_id, {}});
        }
      }
      ch->send_empty(static_cast<uint16_t>(HostMsg::kPluginEvent), h.view_id);
      continue;
    }
    if (type == HostMsg::kSetSelection) {
      ch->send_empty(static_cast<uint16_t>(HostMsg::kSelectionChanged),
                     h.view_id);
      continue;
    }
    if (type == HostMsg::kPrintRequest) {
      ch->send_empty(static_cast<uint16_t>(HostMsg::kPrintPage), h.view_id);
      continue;
    }
  }
  return true;
}

}  // namespace

int RendererMain(const ContentMainParams& params) {
  // This TU must stay free of d3d11.h / GL. Paint lives in gpu::GpuMain.
  if (argv_has_flag(params.argc, params.argv, L"--self-test")) {
    std::fprintf(stdout, "--type=renderer --self-test: no GPU device\n");
    return 0;
  }

  base::ipc::IncomingInvitation incoming =
      base::ipc::IncomingInvitation::accept(params.argc, params.argv);
  base::ipc::Channel ch = incoming.extract("renderer");
  if (!ch.is_open()) {
    std::fprintf(stderr, "renderer: invitation extract failed\n");
    return 3;
  }

  HANDLE parent = nullptr;
  const uint32_t parent_pid = parent_pid_from_argv(params.argc, params.argv);
  if (parent_pid != 0) {
    parent = OpenProcess(SYNCHRONIZE, FALSE, parent_pid);
  }

  const bool ok = run_renderer_loop(&ch, parent);
  if (parent) {
    CloseHandle(parent);
  }
  return ok ? 0 : 4;
}

}  // namespace content
