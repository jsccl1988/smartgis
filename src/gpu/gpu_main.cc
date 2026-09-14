// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "gpu/gpu.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "content/common/ipc.h"
#include "gpu/present.h"
#include "gpu/render_backend.h"

namespace gpu {
namespace {
namespace cd = content::detail;

struct SurfaceSlot {
  content::ViewKind kind = content::ViewKind::kMapEdit;
  content::PresentMode mode = content::PresentMode::kSharedTexture;
  uint32_t width_px = 64;
  uint32_t height_px = 64;
  float dpi = 96.f;
  content::Extent2 extent{};
  detail::PresentTarget present;
  bool attached = false;
  DWORD last_pointer_paint_ms = 0;
};

struct Args {
  uint32_t parent_pid = 0;
  std::wstring pipe_name;
  std::wstring session;
  bool self_test = false;
};

Args parse_args(int argc, wchar_t** argv) {
  Args a;
  for (int i = 1; i < argc; ++i) {
    const wchar_t* s = argv[i];
    if (wcsncmp(s, L"--parent-pid=", 13) == 0) {
      a.parent_pid = static_cast<uint32_t>(_wtoi(s + 13));
    } else if (wcsncmp(s, L"--pipe=", 7) == 0) {
      a.pipe_name = s + 7;
    } else if (wcsncmp(s, L"--session=", 10) == 0) {
      a.session = s + 10;
    } else if (wcscmp(s, L"--self-test") == 0) {
      a.self_test = true;
    }
  }
  return a;
}

void announce_and_paint(cd::Pipe* pipe,
                        uint32_t view_id,
                        SurfaceSlot* slot,
                        HWND hwnd) {
  // Distinct clears so Map / Data / 3D panes are visibly different once chrome
  // presents Latest() into the HWND.
  uint8_t b = 0x40;
  uint8_t g = 0x80;
  uint8_t r = 0xC0;
  if (slot->kind == content::ViewKind::kMapData) {
    b = 0x30;
    g = 0x70;
    r = 0x50;
  } else if (slot->kind == content::ViewKind::kScene3d) {
    b = 0x90;
    g = 0x40;
    r = 0x28;
  }
  (void)hwnd;
  if (select_render_backend() == RenderBackendKind::kTrackAMapLibre &&
      slot->kind != content::ViewKind::kScene3d) {
    MapPaintRequest req;
    req.kind = slot->kind;
    req.extent = slot->extent;
    req.style_json =
        "{\"version\":8,\"name\":\"gpu-default\",\"layers\":["
        "{\"id\":\"bg\",\"type\":\"background\","
        "\"paint\":{\"background-color\":\"#4080C0\"}}]}";
    char xyz[512] = {};
    if (GetEnvironmentVariableA("SMT_XYZ_URL", xyz, sizeof(xyz)) > 0) {
      req.tile_url_template = xyz;
    }
    if (paint_map_frame(&slot->present, req)) {
      const content::SharedHandleWire w = slot->present.wire();
      pipe->send_msg(content::HostMsg::kSharedHandle, view_id, w);
      content::FrameReadyWire fr = {};
      fr.generation = w.generation;
      fr.fence = 0;
      fr.cursor_hint = 0;
      pipe->send_msg(content::HostMsg::kFrameReady, view_id, fr);
      return;
    }
  }
  // Track B / 3D: clear once, then demo geometry until GpuScene submit lands.
  slot->present.paint_clear(b, g, r, 0xFF);
  slot->present.paint_demo_frame(slot->kind);
  const content::SharedHandleWire w = slot->present.wire();
  pipe->send_msg(content::HostMsg::kSharedHandle, view_id, w);
  content::FrameReadyWire fr = {};
  fr.generation = w.generation;
  fr.fence = 0;
  fr.cursor_hint = 0;
  pipe->send_msg(content::HostMsg::kFrameReady, view_id, fr);
}

int run_server(const Args& args) {
  if (args.parent_pid == 0 || args.pipe_name.empty()) {
    std::fprintf(stderr,
                 "gpu: --parent-pid and --pipe are required\n");
    return 2;
  }

  HANDLE parent = OpenProcess(SYNCHRONIZE | PROCESS_DUP_HANDLE, FALSE,
                              args.parent_pid);
  if (!parent) {
    parent = OpenProcess(SYNCHRONIZE, FALSE, args.parent_pid);
  }

  std::unique_ptr<Adapter> adapter(create_adapter());
  adapter->load_legacy_dlls();
  adapter->init_hidden_hwnd(64, 64);

  cd::Pipe pipe;
  const std::wstring path = content::pipe_path_from_name(args.pipe_name);
  if (!pipe.connect_client(path, 15000)) {
    std::fprintf(stderr, "gpu: failed to connect %ls\n",
                 path.c_str());
    return 3;
  }

  content::HelloBody hello;
  hello.role = "gpu";
  hello.gpu = "d3d11";
  pipe.send_msg(content::HostMsg::kHello, 0, hello);

  std::map<uint32_t, SurfaceSlot> views;

  for (;;) {
    if (parent) {
      if (WaitForSingleObject(parent, 0) == WAIT_OBJECT_0) {
        break;
      }
    }
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        return 0;
      }
      TranslateMessage(&msg);
      DispatchMessageW(&msg);
    }

    content::FrameHeader h = {};
    std::vector<uint8_t> payload;
    if (!pipe.recv(&h, &payload, 50)) {
      if (!pipe.is_open()) {
        break;
      }
      continue;
    }
    if (h.version != content::kHostProtocolVersion) {
      std::fprintf(stderr, "gpu: protocol mismatch\n");
      return 4;
    }
    const auto type = static_cast<content::HostMsg>(h.type);

    if (type == content::HostMsg::kShutdown) {
      break;
    }
    if (type == content::HostMsg::kHelloAck) {
      continue;
    }
    if (type == content::HostMsg::kOpenView) {
      SurfaceSlot& slot = views[h.view_id];
      content::OpenViewBody body;
      if (cd::decode_payload(payload, &body)) {
        slot.kind = static_cast<content::ViewKind>(body.kind);
      }
      pipe.send_empty(content::HostMsg::kViewReady, h.view_id);
      continue;
    }
    if (type == content::HostMsg::kCloseView) {
      views.erase(h.view_id);
      continue;
    }
    if (type == content::HostMsg::kAttachSurface) {
      SurfaceSlot& slot = views[h.view_id];
      content::AttachSurfaceBody body;
      if (cd::decode_payload(payload, &body)) {
        slot.mode = static_cast<content::PresentMode>(body.present_mode);
      }
      const bool need_new =
          !slot.attached || slot.present.generation() == 0 ||
          slot.present.mode() != slot.mode;
      slot.attached = true;
      if (!need_new) {
        // Visibility / duplicate Attach must not recreate the DIB — that was
        // another UI-thread stall path when Catalog toggled the island.
        continue;
      }
      if (!slot.present.resize(slot.width_px, slot.height_px, slot.mode,
                               parent)) {
        continue;
      }
      announce_and_paint(&pipe, h.view_id, &slot,
                         static_cast<HWND>(adapter->hwnd()));
      continue;
    }
    if (type == content::HostMsg::kResizeSurface) {
      SurfaceSlot& slot = views[h.view_id];
      content::ResizeSurfaceBody body;
      if (cd::decode_payload(payload, &body)) {
        slot.width_px = body.w;
        slot.height_px = body.h;
        slot.dpi = body.dpi;
      }
      if (!slot.present.resize(slot.width_px, slot.height_px, slot.mode,
                               parent)) {
        continue;
      }
      adapter->init_hidden_hwnd(static_cast<int>(slot.width_px),
                                static_cast<int>(slot.height_px));
      announce_and_paint(&pipe, h.view_id, &slot,
                         static_cast<HWND>(adapter->hwnd()));
      continue;
    }
    if (type == content::HostMsg::kSetExtent) {
      SurfaceSlot& slot = views[h.view_id];
      content::ExtentWire body;
      if (!cd::decode_payload(payload, &body)) {
        continue;
      }
      slot.extent.xmin = body.xmin;
      slot.extent.ymin = body.ymin;
      slot.extent.xmax = body.xmax;
      slot.extent.ymax = body.ymax;
      pipe.send_msg(content::HostMsg::kExtentChanged, h.view_id, body);
      continue;
    }
    if (type == content::HostMsg::kPointerEvent) {
      // Do not rebuild/publish a full DIB on every mouse move — that fills the
      // pipe and blocks chrome's UI thread inside Dispatch (white-screen hang).
      // Scene3d keeps a throttled redraw so orbit/demo still updates.
      auto it = views.find(h.view_id);
      if (it == views.end() || it->second.present.generation() == 0) {
        continue;
      }
      SurfaceSlot& slot = it->second;
      if (slot.kind != content::ViewKind::kScene3d) {
        continue;
      }
      const DWORD now = GetTickCount();
      if (now - slot.last_pointer_paint_ms < 33) {
        continue;
      }
      slot.last_pointer_paint_ms = now;
      announce_and_paint(&pipe, h.view_id, &slot,
                         static_cast<HWND>(adapter->hwnd()));
      continue;
    }
    if (type == content::HostMsg::kActivateTool ||
        type == content::HostMsg::kSetSelection ||
        type == content::HostMsg::kLegendQuery ||
        type == content::HostMsg::kCatalogOp ||
        type == content::HostMsg::kPluginCall ||
        type == content::HostMsg::kTextCommit) {
      // No present republish — chrome already holds Latest().
      continue;
    }
    if (type == content::HostMsg::kResetGpu) {
      for (auto& kv : views) {
        kv.second.present.resize(kv.second.width_px, kv.second.height_px,
                                 kv.second.mode, parent);
        announce_and_paint(&pipe, kv.first, &kv.second,
                           static_cast<HWND>(adapter->hwnd()));
      }
    }
  }

  if (parent) {
    CloseHandle(parent);
  }
  return 0;
}

}  // namespace

int GpuMain(int argc, wchar_t** argv) {
  const Args args = parse_args(argc, argv);
  if (args.self_test) {
    return run_self_test(argv && argv[0] ? argv[0] : L"");
  }
  return run_server(args);
}

int render_main(int argc, wchar_t** argv) {
  return GpuMain(argc, argv);
}

}  // namespace gpu
