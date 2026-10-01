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
#include "gpu/compositor/composer/composer.h"
#include "gpu/compositor/frame/frame.h"
#include "gpu/device/gpu_device_hub.h"
#include "gpu/display/output_surface.h"
#include "gpu/frame_sink.h"

namespace gpu {
namespace {
namespace cd = content::detail;

using SmtRhi2dBgraSubmitFn = bool (*)(const uint8_t* bgra, uint32_t width_px,
                                    uint32_t height_px, uint32_t stride_bytes,
                                    void* user);
using SmtRhi2dSetBgraSubmitFn = void (*)(SmtRhi2dBgraSubmitFn fn, void* user);
using SmtRhi2dClearBgraSubmitFn = void (*)(void);

detail::OutputSurface* g_leftover_gdi_present = nullptr;

void stretch_bgra_nn(const uint8_t* src, uint32_t src_stride, uint32_t src_w,
                     uint32_t src_h, uint32_t dest_w, uint32_t dest_h,
                     std::vector<uint8_t>* out) {
  out->assign(static_cast<size_t>(dest_w) * dest_h * 4u, 0);
  if (!src || src_w == 0 || src_h == 0 || dest_w == 0 || dest_h == 0) {
    return;
  }
  for (uint32_t y = 0; y < dest_h; ++y) {
    const uint32_t sy = y * src_h / dest_h;
    const uint8_t* srow = src + static_cast<size_t>(sy) * src_stride;
    uint8_t* drow =
        out->data() + static_cast<size_t>(y) * static_cast<size_t>(dest_w) * 4u;
    for (uint32_t x = 0; x < dest_w; ++x) {
      const uint32_t sx = x * src_w / dest_w;
      const uint8_t* sp = srow + static_cast<size_t>(sx) * 4u;
      uint8_t* dp = drow + static_cast<size_t>(x) * 4u;
      dp[0] = sp[0];
      dp[1] = sp[1];
      dp[2] = sp[2];
      dp[3] = sp[3];
    }
  }
}

// Phase 3: leftover GDI BGRA → CompositorFrame → FrameComposer (RHI/software)
// onto the pinned OutputSurface. Scales when sizes differ.
bool leftover_gdi_bgra_upload(const uint8_t* bgra, uint32_t width_px,
                              uint32_t height_px, uint32_t stride_bytes,
                              void* /*user*/) {
  detail::OutputSurface* surface = g_leftover_gdi_present;
  if (!surface || !bgra || width_px == 0 || height_px == 0) {
    return false;
  }
  if (stride_bytes < width_px * 4u) {
    return false;
  }
  const uint32_t sw = surface->wire().width_px;
  const uint32_t sh = surface->wire().height_px;
  if (sw == 0 || sh == 0) {
    return false;
  }

  std::vector<uint8_t> packed;
  if (width_px == sw && height_px == sh && stride_bytes == sw * 4u) {
    packed.assign(bgra, bgra + static_cast<size_t>(sw) * sh * 4u);
  } else if (width_px == sw && height_px == sh) {
    packed.resize(static_cast<size_t>(sw) * sh * 4u);
    for (uint32_t y = 0; y < sh; ++y) {
      std::memcpy(packed.data() + static_cast<size_t>(y) * sw * 4u,
                  bgra + static_cast<size_t>(y) * stride_bytes, sw * 4u);
    }
  } else {
    stretch_bgra_nn(bgra, stride_bytes, width_px, height_px, sw, sh, &packed);
  }

  detail::CompositorFrame frame;
  frame.width_px = sw;
  frame.height_px = sh;
  detail::RenderPass pass;
  pass.width_px = sw;
  pass.height_px = sh;
  detail::append_bgra_quad(&pass, std::move(packed), 1.f, /*replaces=*/true,
                           /*texture_cache_key=*/0);
  frame.render_pass_list.push_back(std::move(pass));

  detail::AdapterId adapter = surface->adapter_id();
  if (adapter == detail::kAdapterInvalid) {
    adapter = detail::device_hub().primary_adapter();
  }
  (void)detail::device_hub().bind_surface(surface, adapter);
  auto composer =
      detail::make_frame_composer(detail::select_compose_backend(), adapter);
  return composer && composer->draw_frame(surface, frame);
}

void bind_leftover_gdi_bgra_submit() {
#ifdef _DEBUG
  const wchar_t* stem = L"legacy_render_d.dll";
#else
  const wchar_t* stem = L"legacy_render.dll";
#endif
  HMODULE gdi = GetModuleHandleW(stem);
  if (!gdi) {
    gdi = GetModuleHandleW(L"legacy_render.dll");
  }
  if (!gdi) {
    return;
  }
  auto set_fn = reinterpret_cast<SmtRhi2dSetBgraSubmitFn>(
      GetProcAddress(gdi, "SmtRhi2dSetBgraSubmit"));
  if (set_fn) {
    set_fn(&leftover_gdi_bgra_upload, nullptr);
  }
}

void clear_leftover_gdi_bgra_submit() {
#ifdef _DEBUG
  const wchar_t* stem = L"legacy_render_d.dll";
#else
  const wchar_t* stem = L"legacy_render.dll";
#endif
  HMODULE gdi = GetModuleHandleW(stem);
  if (!gdi) {
    gdi = GetModuleHandleW(L"legacy_render.dll");
  }
  if (!gdi) {
    return;
  }
  auto clear_fn = reinterpret_cast<SmtRhi2dClearBgraSubmitFn>(
      GetProcAddress(gdi, "SmtRhi2dClearBgraSubmit"));
  if (clear_fn) {
    clear_fn();
  }
  g_leftover_gdi_present = nullptr;
}

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

// Resolve GPU adapter from Attach/Resize body: explicit hint, else LUID, else
// primary. bind_surface is enough when DXGI recreate happens on resize.
detail::AdapterId resolve_adapter_from_affinity(uint32_t adapter_hint,
                                                uint32_t monitor_luid_low,
                                                uint32_t monitor_luid_high) {
  detail::GpuDeviceHub& hub = detail::device_hub();
  if (adapter_hint != detail::kAdapterInvalid) {
    return static_cast<detail::AdapterId>(adapter_hint);
  }
  const uint64_t luid =
      (static_cast<uint64_t>(monitor_luid_high) << 32) |
      static_cast<uint64_t>(monitor_luid_low);
  if (luid != 0) {
    return hub.adapter_for_luid(luid);
  }
  return hub.primary_adapter();
}

void pin_surface_adapter(detail::OutputSurface* surface,
                         uint32_t adapter_hint,
                         uint32_t monitor_luid_low,
                         uint32_t monitor_luid_high) {
  if (!surface) {
    return;
  }
  const detail::AdapterId id = resolve_adapter_from_affinity(
      adapter_hint, monitor_luid_low, monitor_luid_high);
  (void)detail::device_hub().bind_surface(surface, id);
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
  // Pin the active OutputSurface so leftover GDI publish can upload_bgra
  // (async worker may submit after this returns).
  g_leftover_gdi_present = &slot->present;
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
  bind_leftover_gdi_bgra_submit();
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
      pin_surface_adapter(&slot.present, body.adapter_hint,
                          body.monitor_luid_low, body.monitor_luid_high);
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
      // Defer first paint until ResizeSurface supplies the real client size.
      // Painting the default 64² slot here races the shell's follow-up Resize
      // and can leave WaitFrameReady starved under load (Data tab exit 8).
      const bool defer_first_paint =
          slot.present.generation() == 0 && slot.width_px <= 64 &&
          slot.height_px <= 64;
      if (!slot.present.resize(slot.width_px, slot.height_px, slot.mode,
                               parent)) {
        continue;
      }
      if (!defer_first_paint) {
        (void)announce_and_paint(&pipe, h.view_id, &slot);
      }
      continue;
    }
    if (type == content::HostMsg::kResizeSurface) {
      SurfaceSlot& slot = views[h.view_id];
      content::ResizeSurfaceBody body;
      if (!cd::decode_payload(payload, &body)) {
        continue;
      }
      pin_surface_adapter(&slot.present, body.adapter_hint,
                          body.monitor_luid_low, body.monitor_luid_high);
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
  clear_leftover_gdi_bgra_submit();
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
