// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/map_contents.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "base/ipc/handle.h"
#include "base/ipc/invitation.h"
#include "base/ipc/receiver.h"
#include "content/common/ipc.h"
#include "content/public/process_type.h"

namespace content {
namespace detail {

std::wstring g_gpu_exe_override;

namespace {

std::wstring module_dir() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  wchar_t* slash = wcsrchr(path, L'\\');
  if (slash) {
    *slash = 0;
  }
  return path;
}

std::wstring this_exe_path() {
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  return path;
}

bool file_exists_w(const std::wstring& path) {
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES &&
         (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring sibling_render_exe(const std::wstring& dir) {
  const wchar_t* names[] = {L"SmartGisRender.exe", L"SmartGisRenderD.exe"};
  for (const wchar_t* name : names) {
    const std::wstring cand = dir + L"\\" + name;
    if (file_exists_w(cand)) {
      return cand;
    }
  }
  return std::wstring();
}

// C# / CEF PEs cannot ContentMain --type=gpu. Prefer sibling render exe.
// Views / WinUI C++ keep relaunching this PE.
std::wstring resolve_render_exe() {
  if (!g_gpu_exe_override.empty() && file_exists_w(g_gpu_exe_override)) {
    return g_gpu_exe_override;
  }
  const std::wstring self = this_exe_path();
  const wchar_t* base = wcsrchr(self.c_str(), L'\\');
  base = base ? base + 1 : self.c_str();
  const bool foreign_host = _wcsnicmp(base, L"SmartGisCef", 11) == 0 ||
                            _wcsnicmp(base, L"SmartGisCs", 10) == 0;
  if (!foreign_host) {
    return self;
  }
  const std::wstring sibling = sibling_render_exe(module_dir());
  return sibling.empty() ? self : sibling;
}

std::wstring make_session_id() {
  wchar_t buf[80];
  swprintf_s(buf, L"%u-%lu", GetCurrentProcessId(), GetTickCount());
  return buf;
}

}  // namespace

class MapWidgetHostViewImpl final : public MapWidgetHostView {
 public:
  MapWidgetHostViewImpl(class MapContentsImpl* session, uint32_t view_id)
      : session_(session), view_id_(view_id) {}
  ~MapWidgetHostViewImpl() override {
    std::lock_guard<std::mutex> lock(latest_mu_);
    if (latest_.nt_handle) {
      CloseHandle(static_cast<HANDLE>(latest_.nt_handle));
      latest_.nt_handle = nullptr;
    }
  }

  void Create(const CreateParams& params,
              const Preferences& preferences) override;
  void Destroy() override { parent_hwnd_ = nullptr; }

  uint32_t ViewId() const override { return view_id_; }
  void* NativeHwnd() const override { return parent_hwnd_; }
  void Resize(int width_px, int height_px, float dpi) override;
  void Resize(int x, int y, int width_px, int height_px, float dpi) override;
  void SetPresentMode(PresentMode mode) override;
  void SetVisible(bool visible) override;
  SharedSurface Latest() const override;

  void SetLatest(const SharedSurface& s) {
    std::lock_guard<std::mutex> lock(latest_mu_);
    // Do not CloseHandle(old) here: the UI thread may still be mapping
    // latest_.nt_handle inside present_latest_frame. Leak until Destroy /
    // destructor; frames are infrequent relative to process lifetime.
    latest_ = s;
  }
  void set_present_mode(PresentMode mode) { present_mode_ = mode; }

 private:
  class MapContentsImpl* session_;
  uint32_t view_id_;
  void* parent_hwnd_ = nullptr;
  mutable std::mutex latest_mu_;
  SharedSurface latest_{};
  PresentMode present_mode_ = PresentMode::kSharedTexture;
};

class MapContentsImpl final : public MapContents {
 public:
  MapContentsImpl() = default;
  ~MapContentsImpl() override { Shutdown(); }

  bool StartRenderProcess() override;
  void Shutdown() override;
  bool IsOopRender() const override { return oop_; }
  const wchar_t* PresentStatus() const override { return status_.c_str(); }

  uint32_t OpenView(ViewKind kind) override;
  void CloseView(uint32_t view_id) override;
  MapWidgetHostView* AttachSurface(uint32_t view_id, PresentMode mode) override;
  MapWidgetHostView* HostView(uint32_t view_id) override;

  void SetExtent(uint32_t view_id, const Extent2& e) override;
  Extent2 Extent(uint32_t view_id) const override;

  void SetSelection(uint32_t view_id, const FeatureId* ids, size_t n) override;
  void LegendSnapshot(uint32_t view_id) override;
  void CatalogCall(const char* json_op) override;
  void DispatchPlugin(uint32_t view_id,
                      const char* plugin_id,
                      const char* method,
                      const void* bytes,
                      size_t n) override;

  void SetObserver(MapContentsObserver* observer) override { observer_ = observer; }
  bool WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) override;

  void ActivateTool(uint32_t view_id, const char* tool_id) override;
  void Activate(uint32_t view_id, const char* tool_id);
  void Dispatch(uint32_t view_id, const InputEvent& e) override;

  bool send_msg_empty(HostMsg type, uint32_t view_id);
  template <typename T>
  bool send_msg(HostMsg type, uint32_t view_id, const T& body) {
    return pipe_.send_msg(type, view_id, body);
  }
  template <typename T>
  bool send_tool_msg(HostMsg type, uint32_t view_id, const T& body) {
    if (renderer_pipe_.is_open()) {
      return renderer_pipe_.send_msg(type, view_id, body);
    }
    return pipe_.send_msg(type, view_id, body);
  }
  bool send_tool_empty(HostMsg type, uint32_t view_id) {
    if (renderer_pipe_.is_open()) {
      return renderer_pipe_.send_empty(type, view_id);
    }
    return pipe_.send_empty(type, view_id);
  }
  void store_surface(uint32_t view_id, const SharedSurface& s);

 private:
  bool launch_invited_child(const std::wstring& exe,
                            const wchar_t* extra_args,
                            const char* attach_name,
                            Pipe* pipe,
                            HANDLE* process);
  void recv_loop();
  void renderer_recv_loop();
  void handle_frame(const FrameHeader& h,
                    const std::vector<uint8_t>& payload,
                    std::vector<base::ipc::PlatformHandle> handles);

  struct PipeListener final : base::ipc::MessageListener {
    MapContentsImpl* self = nullptr;
    void on_message(const base::ipc::Frame& frame,
                    std::vector<uint8_t> payload,
                    std::vector<base::ipc::PlatformHandle> handles) override {
      FrameHeader h = {};
      h.magic = frame.magic;
      h.version = frame.version;
      h.type = frame.type;
      h.flags = frame.flags;
      h.handle_count = frame.handle_count;
      h.view_id = frame.view_id;
      h.seq = frame.seq;
      h.payload_bytes = frame.payload_bytes;
      self->handle_frame(h, payload, std::move(handles));
    }
    void on_disconnect() override {
      if (self->observer_) {
        self->observer_->OnRenderDied();
      }
    }
  };

  HANDLE job_ = nullptr;
  HANDLE process_ = nullptr;
  HANDLE renderer_process_ = nullptr;
  Pipe pipe_;
  Pipe renderer_pipe_;
  PipeListener listener_{};
  std::unique_ptr<base::ipc::Receiver> receiver_;
  std::thread recv_thread_;
  std::thread renderer_thread_;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> next_view_id_{1};
  MapContentsObserver* observer_ = nullptr;
  mutable std::mutex mu_;
  std::map<uint32_t, MapWidgetHostViewImpl*> views_;
  // Frames can arrive before AttachSurface inserts the HostView.
  std::map<uint32_t, SharedSurface> pending_surfaces_;
  std::map<uint32_t, Extent2> extents_;
  std::map<uint32_t, uint32_t> frame_gen_;
  HANDLE frame_event_ = nullptr;
  uint32_t hello_ok_ = 0;
  bool oop_ = false;
  std::wstring status_ = L"down";
};

void MapWidgetHostViewImpl::Create(const CreateParams& params,
                         const Preferences&) {
  parent_hwnd_ = params.parent_hwnd;
}

void MapWidgetHostViewImpl::Resize(int width_px, int height_px, float dpi) {
  ResizeSurfaceBody body;
  body.w = static_cast<uint32_t>(width_px);
  body.h = static_cast<uint32_t>(height_px);
  body.dpi = dpi;
  session_->send_msg(HostMsg::kResizeSurface, view_id_, body);
}

void MapWidgetHostViewImpl::Resize(int x,
                         int y,
                         int width_px,
                         int height_px,
                         float dpi) {
  (void)x;
  (void)y;
  Resize(width_px, height_px, dpi);
}

void MapWidgetHostViewImpl::SetPresentMode(PresentMode mode) {
  present_mode_ = mode;
  AttachSurfaceBody body;
  body.present_mode = static_cast<uint32_t>(mode);
  body.visible = 1;
  session_->send_msg(HostMsg::kAttachSurface, view_id_, body);
}

void MapWidgetHostViewImpl::SetVisible(bool visible) {
  AttachSurfaceBody body;
  body.present_mode = static_cast<uint32_t>(present_mode_);
  body.visible = visible ? 1u : 0u;
  session_->send_msg(HostMsg::kAttachSurface, view_id_, body);
}

SharedSurface MapWidgetHostViewImpl::Latest() const {
  std::lock_guard<std::mutex> lock(latest_mu_);
  return latest_;
}

bool MapContentsImpl::send_msg_empty(HostMsg type, uint32_t view_id) {
  return pipe_.send_empty(type, view_id);
}

void MapContentsImpl::store_surface(uint32_t view_id, const SharedSurface& s) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  if (it != views_.end() && it->second) {
    it->second->SetLatest(s);
    pending_surfaces_.erase(view_id);
    return;
  }
  pending_surfaces_[view_id] = s;
}

bool MapContentsImpl::launch_invited_child(const std::wstring& exe,
                                           const wchar_t* extra_args,
                                           const char* attach_name,
                                           Pipe* pipe,
                                           HANDLE* process) {
  if (!pipe || !process || !extra_args || !attach_name) {
    return false;
  }
  base::ipc::OutgoingInvitation invitation;
  base::ipc::Channel local = invitation.attach(attach_name);
  if (!local.is_open()) {
    return false;
  }
  base::ipc::ChildLaunch launch;
  launch.exe = exe;
  launch.extra_args = extra_args;
  launch.cwd = module_dir();
  launch.job = job_;
  PROCESS_INFORMATION pi = {};
  if (!base::ipc::launch_with_invitation(&invitation, launch, &pi)) {
    return false;
  }
  if (!pipe->adopt(std::move(local))) {
    if (pi.hThread) {
      CloseHandle(pi.hThread);
    }
    if (pi.hProcess) {
      TerminateProcess(pi.hProcess, 1);
      CloseHandle(pi.hProcess);
    }
    return false;
  }
  *process = pi.hProcess;
  if (pi.hThread) {
    CloseHandle(pi.hThread);
  }
  return true;
}

bool MapContentsImpl::StartRenderProcess() {
  if (running_) {
    return true;
  }
  if (!frame_event_) {
    frame_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }

  job_ = CreateJobObjectW(nullptr, nullptr);
  if (job_) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &info,
                            sizeof(info));
  }

  const uint32_t pid = GetCurrentProcessId();
  const std::wstring exe = resolve_render_exe();
  const std::wstring session = make_session_id();
  // Classic named-pipe GPU child. Invitation dual-launch is WIP and stalls
  // FrameReady / crashes WinUI --self-test; keep pipe path until proven.
  const std::wstring pipe_path = pipe_path_for_pid(pid);
  if (!pipe_.create_server(pipe_path)) {
    return false;
  }
  wchar_t cmd[1024];
  swprintf_s(cmd,
             L"\"%s\" --type=%s --parent-pid=%u --pipe=%s --session=%s",
             exe.c_str(), ProcessTypeSwitchValue(ProcessType::kGpu), pid,
             pipe_name_for_pid(pid).c_str(), session.c_str());
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  std::vector<wchar_t> cmd_buf(cmd, cmd + wcslen(cmd) + 1);
  if (!CreateProcessW(exe.c_str(), cmd_buf.data(), nullptr, nullptr, FALSE,
                      CREATE_NO_WINDOW, nullptr, module_dir().c_str(), &si,
                      &pi)) {
    pipe_.close();
    return false;
  }
  if (job_) {
    AssignProcessToJobObject(job_, pi.hProcess);
  }
  if (!pipe_.wait_client(15000)) {
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    Shutdown();
    return false;
  }
  process_ = pi.hProcess;
  if (pi.hThread) {
    CloseHandle(pi.hThread);
  }

  running_ = true;
  recv_thread_ = std::thread(&MapContentsImpl::recv_loop, this);

  const DWORD start = GetTickCount();
  while (hello_ok_ == 0 && GetTickCount() - start < 15000) {
    Sleep(20);
  }
  if (hello_ok_ == 0) {
    Shutdown();
    return false;
  }
  oop_ = true;
  status_ = L"oop";
  HelloBody ack;
  ack.role = "chrome";
  return pipe_.send_msg(HostMsg::kHelloAck, 0, ack);
}

void MapContentsImpl::Shutdown() {
  oop_ = false;
  status_ = L"down";
  running_ = false;
  pipe_.send_empty(HostMsg::kShutdown, 0);
  renderer_pipe_.send_empty(HostMsg::kShutdown, 0);
  pipe_.close();
  renderer_pipe_.close();
  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }
  if (renderer_thread_.joinable()) {
    renderer_thread_.join();
  }
  receiver_.reset();
  if (process_) {
    WaitForSingleObject(process_, 3000);
    CloseHandle(process_);
    process_ = nullptr;
  }
  if (renderer_process_) {
    WaitForSingleObject(renderer_process_, 3000);
    CloseHandle(renderer_process_);
    renderer_process_ = nullptr;
  }
  if (job_) {
    CloseHandle(job_);
    job_ = nullptr;
  }
  std::lock_guard<std::mutex> lock(mu_);
  for (auto& kv : views_) {
    delete kv.second;
  }
  views_.clear();
  if (frame_event_) {
    CloseHandle(frame_event_);
    frame_event_ = nullptr;
  }
}

uint32_t MapContentsImpl::OpenView(ViewKind kind) {
  const uint32_t id = next_view_id_++;
  OpenViewBody body;
  body.kind = static_cast<uint32_t>(kind);
  pipe_.send_msg(HostMsg::kOpenView, id, body);
  if (renderer_pipe_.is_open()) {
    renderer_pipe_.send_msg(HostMsg::kOpenView, id, body);
  }
  return id;
}

void MapContentsImpl::CloseView(uint32_t view_id) {
  pipe_.send_empty(HostMsg::kCloseView, view_id);
  if (renderer_pipe_.is_open()) {
    renderer_pipe_.send_empty(HostMsg::kCloseView, view_id);
  }
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  if (it != views_.end()) {
    delete it->second;
    views_.erase(it);
  }
  pending_surfaces_.erase(view_id);
  frame_gen_.erase(view_id);
}

MapWidgetHostView* MapContentsImpl::AttachSurface(uint32_t view_id, PresentMode mode) {
  // Register HostView before the GPU round-trip so SharedHandle / FrameReady
  // cannot race past an empty views_ map and be dropped.
  MapWidgetHostViewImpl* view = nullptr;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = views_.find(view_id);
    if (it != views_.end()) {
      view = it->second;
      view->set_present_mode(mode);
    } else {
      view = new MapWidgetHostViewImpl(this, view_id);
      view->set_present_mode(mode);
      views_[view_id] = view;
    }
    auto pending = pending_surfaces_.find(view_id);
    if (pending != pending_surfaces_.end()) {
      view->SetLatest(pending->second);
      pending_surfaces_.erase(pending);
    }
  }
  AttachSurfaceBody body;
  body.present_mode = static_cast<uint32_t>(mode);
  pipe_.send_msg(HostMsg::kAttachSurface, view_id, body);
  return view;
}

MapWidgetHostView* MapContentsImpl::HostView(uint32_t view_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  return it == views_.end() ? nullptr : it->second;
}

void MapContentsImpl::SetExtent(uint32_t view_id, const Extent2& e) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    extents_[view_id] = e;
  }
  ExtentWire body;
  body.xmin = e.xmin;
  body.ymin = e.ymin;
  body.xmax = e.xmax;
  body.ymax = e.ymax;
  send_tool_msg(HostMsg::kSetExtent, view_id, body);
}

Extent2 MapContentsImpl::Extent(uint32_t view_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = extents_.find(view_id);
  if (it == extents_.end()) {
    return Extent2{};
  }
  return it->second;
}

void MapContentsImpl::SetSelection(uint32_t view_id,
                                   const FeatureId* ids,
                                   size_t n) {
  (void)ids;
  SelectionBody body;
  body.count = static_cast<uint32_t>(n);
  send_tool_msg(HostMsg::kSetSelection, view_id, body);
}

void MapContentsImpl::LegendSnapshot(uint32_t view_id) {
  send_tool_empty(HostMsg::kLegendQuery, view_id);
}

void MapContentsImpl::CatalogCall(const char* json_op) {
  JsonBody body;
  body.json = json_op ? json_op : "{}";
  send_tool_msg(HostMsg::kCatalogOp, 0, body);
}

void MapContentsImpl::DispatchPlugin(uint32_t view_id,
                                    const char* plugin_id,
                                    const char* method,
                                    const void* bytes,
                                    size_t n) {
  PluginCallBody body;
  body.plugin_id = plugin_id ? plugin_id : "";
  body.method = method ? method : "";
  if (bytes && n) {
    body.bytes.assign(static_cast<const char*>(bytes), n);
  }
  send_tool_msg(HostMsg::kPluginCall, view_id, body);
}

bool MapContentsImpl::WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) {
  const DWORD start = GetTickCount();
  for (;;) {
    {
      std::lock_guard<std::mutex> lock(mu_);
      auto it = frame_gen_.find(view_id);
      if (it != frame_gen_.end() && it->second > 0) {
        return true;
      }
    }
    if (GetTickCount() - start >= timeout_ms) {
      return false;
    }
    if (frame_event_) {
      WaitForSingleObject(frame_event_, 50);
    } else {
      Sleep(20);
    }
  }
}

void MapContentsImpl::ActivateTool(uint32_t view_id, const char* tool_id) {
  Activate(view_id, tool_id);
}

void MapContentsImpl::Activate(uint32_t view_id, const char* tool_id) {
  ToolBody body;
  body.tool_id = tool_id ? tool_id : "";
  send_tool_msg(HostMsg::kActivateTool, view_id, body);
}

void MapContentsImpl::Dispatch(uint32_t view_id, const InputEvent& e) {
  PointerEventWire w = {};
  w.t_qpc = e.t_qpc;
  w.kind = static_cast<uint32_t>(e.kind);
  w.flags = e.flags;
  w.x_px = e.x_px;
  w.y_px = e.y_px;
  w.wheel = e.wheel;
  w.key = e.key;
  w.dpi = 96.f;
  if (e.kind == InputEvent::Kind::kTextCommit) {
    send_tool_msg(HostMsg::kTextCommit, view_id, w);
    return;
  }
  send_tool_msg(HostMsg::kPointerEvent, view_id, w);
}

void MapContentsImpl::recv_loop() {
  while (running_) {
    FrameHeader h = {};
    std::vector<uint8_t> payload;
    std::vector<base::ipc::PlatformHandle> handles;
    if (!pipe_.recv(&h, &payload, &handles, 500)) {
      if (!running_ || !pipe_.is_open()) {
        if (observer_) {
          observer_->OnRenderDied();
        }
        break;
      }
      continue;
    }
    handle_frame(h, payload, std::move(handles));
  }
}

void MapContentsImpl::renderer_recv_loop() {
  while (running_) {
    FrameHeader h = {};
    std::vector<uint8_t> payload;
    std::vector<base::ipc::PlatformHandle> handles;
    if (!renderer_pipe_.recv(&h, &payload, &handles, 500)) {
      if (!running_ || !renderer_pipe_.is_open()) {
        break;
      }
      continue;
    }
    const auto type = static_cast<HostMsg>(h.type);
    if (type == HostMsg::kHello) {
      HelloBody ack;
      ack.role = "chrome";
      renderer_pipe_.send_msg(HostMsg::kHelloAck, 0, ack);
      continue;
    }
    if (type == HostMsg::kShutdown) {
      break;
    }
    if (type == HostMsg::kExtentChanged) {
      ExtentWire w = {};
      if (decode_payload(payload, &w)) {
        {
          std::lock_guard<std::mutex> lock(mu_);
          extents_[h.view_id] = Extent2{w.xmin, w.ymin, w.xmax, w.ymax};
        }
        pipe_.send_msg(HostMsg::kSetExtent, h.view_id, w);
        if (observer_) {
          observer_->OnExtentChanged(
              h.view_id, Extent2{w.xmin, w.ymin, w.xmax, w.ymax});
        }
      }
      continue;
    }
    if (type == HostMsg::kViewReady || type == HostMsg::kCatalogDelta ||
        type == HostMsg::kLegendSnapshot || type == HostMsg::kPluginEvent) {
      continue;
    }
  }
}

void MapContentsImpl::handle_frame(
    const FrameHeader& h,
    const std::vector<uint8_t>& payload,
    std::vector<base::ipc::PlatformHandle> handles) {
  const auto type = static_cast<HostMsg>(h.type);
  if (type == HostMsg::kHello) {
    hello_ok_ = 1;
    return;
  }
  if (type == HostMsg::kSharedHandle) {
    SharedHandleWire w = {};
    if (!decode_payload(payload, &w)) {
      return;
    }
    SharedSurface s = {};
    s.generation = w.generation;
    if (!handles.empty() && handles[0].is_valid()) {
      s.nt_handle = handles[0].release();
    } else if (w.nt_handle != 0) {
      s.nt_handle =
          reinterpret_cast<void*>(static_cast<uintptr_t>(w.nt_handle));
    } else {
      return;
    }
    s.width_px = w.width_px;
    s.height_px = w.height_px;
    s.format = w.format;
    store_surface(h.view_id, s);
    return;
  }
  if (type == HostMsg::kFrameReady) {
    FrameReadyWire w = {};
    if (!decode_payload(payload, &w)) {
      return;
    }
    {
      std::lock_guard<std::mutex> lock(mu_);
      frame_gen_[h.view_id] = w.generation;
    }
    if (frame_event_) {
      SetEvent(frame_event_);
    }
    if (observer_) {
      observer_->OnFrameReady(h.view_id, w.generation);
    }
    return;
  }
  if (type == HostMsg::kExtentChanged) {
    ExtentWire w = {};
    if (!decode_payload(payload, &w)) {
      return;
    }
    Extent2 e = {};
    e.xmin = w.xmin;
    e.ymin = w.ymin;
    e.xmax = w.xmax;
    e.ymax = w.ymax;
    {
      std::lock_guard<std::mutex> lock(mu_);
      extents_[h.view_id] = e;
    }
    if (observer_) {
      observer_->OnExtentChanged(h.view_id, e);
    }
  }
  if (type == HostMsg::kRenderDied && observer_) {
    observer_->OnRenderDied();
  }
}

}  // namespace detail

MapContents* MapContents::Create() {
  return new detail::MapContentsImpl();
}

void MapContents::SetGpuExeOverride(const wchar_t* utf16_path) {
  if (!utf16_path || !utf16_path[0]) {
    detail::g_gpu_exe_override.clear();
    return;
  }
  detail::g_gpu_exe_override = utf16_path;
}

}  // namespace content
