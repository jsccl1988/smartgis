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

#include "base/ipc/handle/handle.h"
#include "base/ipc/invitation/invitation.h"
#include "content/common/ipc.h"
#include "gpu/device/gpu_device_hub.h"
#include "gpu/display/output_surface.h"
#include "gpu/frame_sink.h"

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
  detail::OutputSurface present;
  bool attached = false;
};

struct Args {
  uint32_t parent_pid = 0;
  std::wstring pipe_name;
  std::wstring session;
  bool self_test = false;
};

void pin_surface_adapter(detail::OutputSurface* surface) {
  if (!surface) {
    return;
  }
  detail::GpuDeviceHub& hub = detail::device_hub();
  detail::AdapterId id = hub.adapter_of(surface);
  if (id == detail::kAdapterInvalid) {
    id = hub.primary_adapter();
  }
  (void)hub.bind_surface(surface, id);
}

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
                         detail::OutputSurface* present) {
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

bool announce_and_paint(cd::Pipe* pipe, uint32_t view_id, SurfaceSlot* slot) {
  // Shell overlays MapScene vectors. Scene3d is direct content inside
  // draw_and_swap; this call does not branch on kind.
  DrawRequest req;
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
  (void)draw_and_swap(&slot->present, req);
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
    // PROCESS_DUP_HANDLE is required so OutputSurface can DuplicateHandle the
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

  std::unique_ptr<LegacyHost> host(create_legacy_host());
  host->load_legacy_dlls();
  // One hidden window for legacy device Init. Resize only updates the present
  // target; another CreateWindowExW would leak the previous HWND.
  host->init_hidden_hwnd(64, 64);

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
        // republish SharedHandle + FrameReady so shell that attached late
        // (WinUI sync attach after Hello) receives Latest().
        (void)send_shared_surface(&pipe, h.view_id, &slot.present);
        continue;
      }
      pin_surface_adapter(&slot.present);
      if (!slot.present.resize(slot.width_px, slot.height_px, slot.mode,
                               parent)) {
        continue;
      }
      (void)announce_and_paint(&pipe, h.view_id, &slot);
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
      // Duplicate LayoutSlot / tab sync must not release the live DIB — shell
      // may still be mapping it for StretchDIBits (CEF flicker + AV).
      if (slot.present.generation() != 0 && slot.width_px == new_w &&
          slot.height_px == new_h) {
        slot.dpi = body.dpi;
        continue;
      }
      slot.width_px = new_w;
      slot.height_px = new_h;
      slot.dpi = body.dpi;
      pin_surface_adapter(&slot.present);
      if (!slot.present.resize(slot.width_px, slot.height_px, slot.mode,
                               parent)) {
        continue;
      }
      announce_and_paint(&pipe, h.view_id, &slot);
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
      // Extent and camera stay as stored. A pointer tick must not republish
      // the same Scene3d preview (2D panes already skip this message).
      continue;
    }
    if (type == content::HostMsg::kActivateTool) {
      content::ToolBody body;
      if (cd::decode_payload(payload, &body) &&
          apply_content_source_command(body.tool_id.c_str())) {
        for (auto& kv : views) {
          announce_and_paint(&pipe, kv.first, &kv.second);
        }
      }
      continue;
    }
    if (type == content::HostMsg::kSetRenderBackend) {
      content::RenderBackendWire body;
      if (cd::decode_payload(payload, &body)) {
        set_content_source(body.kind == 1 ? ContentSource::kTile
                                         : ContentSource::kDirect);
      }
      for (auto& kv : views) {
        announce_and_paint(&pipe, kv.first, &kv.second);
      }
      continue;
    }
    if (type == content::HostMsg::kSetSelection ||
        type == content::HostMsg::kLegendQuery ||
        type == content::HostMsg::kCatalogOp ||
        type == content::HostMsg::kPluginCall ||
        type == content::HostMsg::kTextCommit) {
      // No present republish — shell already holds Latest().
      continue;
    }
    if (type == content::HostMsg::kResetGpu) {
      for (auto& kv : views) {
        kv.second.present.resize(kv.second.width_px, kv.second.height_px,
                                 kv.second.mode, parent);
        announce_and_paint(&pipe, kv.first, &kv.second);
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
