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

#include "base/ipc/handle.h"
#include "base/ipc/invitation.h"
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

bool send_shared_surface(cd::Pipe* pipe,
                         uint32_t view_id,
                         detail::PresentTarget* present) {
  if (!pipe || !present) {
    return false;
  }
  content::SharedHandleWire w = present->wire();
  HANDLE local = present->share_handle();
  if (w.nt_handle == 0 && !local) {
    return false;
  }
  // Prefer pickle nt_handle (already duplicated into the UI process). Fall
  // back to Channel attachment of the local share / DIB mapping.
  if (w.nt_handle != 0) {
    if (!pipe->send_msg(content::HostMsg::kSharedHandle, view_id, w)) {
      return false;
    }
  } else {
    base::ipc::PlatformHandle attached =
        base::ipc::PlatformHandle::borrow(local);
    if (!pipe->send_msg(content::HostMsg::kSharedHandle, view_id, w, &attached,
                        1)) {
      return false;
    }
  }
  content::FrameReadyWire fr = {};
  fr.generation = w.generation;
  fr.fence = 0;
  fr.cursor_hint = 0;
  return pipe->send_msg(content::HostMsg::kFrameReady, view_id, fr);
}

bool announce_and_paint(cd::Pipe* pipe,
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
  // Scene3d: land-masked DEM wireframe (not flat MapLibre). Chrome HUD /
  // FlyCube present_gpu sit on top — SmartGis.exe 3D parity.
  if (slot->kind == content::ViewKind::kScene3d) {
    slot->present.paint_clear(b, g, r, 0xFF);
    slot->present.paint_demo_frame(content::ViewKind::kScene3d);
    return send_shared_surface(pipe, view_id, &slot->present);
  }
  // Map / Data: Track A MapLibre (or software basemap). Chrome overlays
  // MapScene vectors on this DIB.
  MapPaintRequest req;
  req.kind = slot->kind;
  req.extent = slot->extent;
  req.style_json =
      "{\"version\":8,\"name\":\"gpu-map\",\"layers\":["
      "{\"id\":\"bg\",\"type\":\"background\","
      "\"paint\":{\"background-color\":\"#4080C0\"}}]}";
  char xyz[512] = {};
  if (GetEnvironmentVariableA("SMT_XYZ_URL", xyz, sizeof(xyz)) > 0) {
    req.tile_url_template = xyz;
  }
  const bool has_template =
      (req.tile_url_template && req.tile_url_template[0] != '\0') ||
      !req.tile_url_templates.empty();
  const bool has_sources =
      req.style_json && std::strstr(req.style_json, "\"sources\"");
  if (has_template || has_sources) {
    req.fetch = make_net_tile_fetch();
  }
  if (paint_map_frame(&slot->present, req)) {
    return send_shared_surface(pipe, view_id, &slot->present);
  }
  slot->present.paint_clear(b, g, r, 0xFF);
  return send_shared_surface(pipe, view_id, &slot->present);
}

int run_server(int argc, wchar_t** argv, const Args& args) {
  base::ipc::PlatformChannel invited =
      base::ipc::PlatformChannel::from_command_line(argc, argv);
  if (!invited.is_valid() &&
      (args.parent_pid == 0 || args.pipe_name.empty())) {
    std::fprintf(stderr,
                 "gpu: --parent-pid and --pipe, or --ipc-channel-handle, "
                 "are required\n");
    return 2;
  }

  HANDLE parent = nullptr;
  if (args.parent_pid != 0) {
    // PROCESS_DUP_HANDLE is required so PresentTarget can DuplicateHandle the
    // DIB/DXGI share into the UI process (pickle nt_handle). SYNCHRONIZE-only
    // fallback made SharedHandle empty → FrameReady without Latest() → white.
    parent = OpenProcess(SYNCHRONIZE | PROCESS_DUP_HANDLE, FALSE,
                         args.parent_pid);
    if (!parent) {
      parent = OpenProcess(PROCESS_DUP_HANDLE, FALSE, args.parent_pid);
    }
    if (!parent) {
      parent = OpenProcess(SYNCHRONIZE, FALSE, args.parent_pid);
    }
  }

  std::unique_ptr<Adapter> adapter(create_adapter());
  adapter->load_legacy_dlls();
  adapter->init_hidden_hwnd(64, 64);

  cd::Pipe pipe;
  if (invited.is_valid()) {
    base::ipc::IncomingInvitation incoming =
        base::ipc::IncomingInvitation::accept(std::move(invited));
    base::ipc::Channel gpu = incoming.extract("gpu");
    if (!pipe.adopt(std::move(gpu))) {
      std::fprintf(stderr, "gpu: invitation extract failed\n");
      return 3;
    }
    if (parent) {
      pipe.set_peer_process(parent);
    }
  } else {
    const std::wstring path = content::pipe_path_from_name(args.pipe_name);
    if (!pipe.connect_client(path, 15000)) {
      std::fprintf(stderr, "gpu: failed to connect %ls\n", path.c_str());
      return 3;
    }
    if (parent) {
      pipe.set_peer_process(parent);
    }
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
        // Visibility / duplicate Attach must not recreate the DIB, but still
        // republish SharedHandle + FrameReady so chrome that attached late
        // (WinUI sync attach after Hello) receives Latest().
        (void)send_shared_surface(&pipe, h.view_id, &slot.present);
        continue;
      }
      if (!slot.present.resize(slot.width_px, slot.height_px, slot.mode,
                               parent)) {
        continue;
      }
      (void)announce_and_paint(&pipe, h.view_id, &slot,
                               static_cast<HWND>(adapter->hwnd()));
      continue;
    }
    if (type == content::HostMsg::kResizeSurface) {
      SurfaceSlot& slot = views[h.view_id];
      content::ResizeSurfaceBody body;
      if (!cd::decode_payload(payload, &body)) {
        continue;
      }
      const uint32_t new_w = body.w < 1 ? 1 : body.w;
      const uint32_t new_h = body.h < 1 ? 1 : body.h;
      // Duplicate LayoutSlot / tab sync must not release the live DIB — chrome
      // may still be mapping it for StretchDIBits (CEF flicker + AV).
      if (slot.present.generation() != 0 && slot.width_px == new_w &&
          slot.height_px == new_h) {
        slot.dpi = body.dpi;
        continue;
      }
      slot.width_px = new_w;
      slot.height_px = new_h;
      slot.dpi = body.dpi;
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
      // Scene3d keeps a throttled redraw so the map/terrain frame still updates.
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
    if (type == content::HostMsg::kActivateTool) {
      content::ToolBody body;
      if (cd::decode_payload(payload, &body) &&
          apply_render_backend_command(body.tool_id.c_str())) {
        for (auto& kv : views) {
          announce_and_paint(&pipe, kv.first, &kv.second,
                             static_cast<HWND>(adapter->hwnd()));
        }
      }
      continue;
    }
    if (type == content::HostMsg::kSetRenderBackend) {
      content::RenderBackendWire body;
      if (cd::decode_payload(payload, &body)) {
        set_render_backend(body.kind == 1
                               ? RenderBackendKind::kTrackAMapLibre
                               : RenderBackendKind::kTrackBRhi);
      }
      for (auto& kv : views) {
        announce_and_paint(&pipe, kv.first, &kv.second,
                           static_cast<HWND>(adapter->hwnd()));
      }
      continue;
    }
    if (type == content::HostMsg::kSetSelection ||
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
  return run_server(argc, argv, args);
}

int render_main(int argc, wchar_t** argv) {
  return GpuMain(argc, argv);
}

}  // namespace gpu
