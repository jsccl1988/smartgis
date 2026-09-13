// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/map_contents.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "content/common/ipc.h"
#include "content/public/process_type.h"

namespace content {
namespace detail {
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

  void SetLatest(const SharedSurface& s) { latest_ = s; }

 private:
  class MapContentsImpl* session_;
  uint32_t view_id_;
  void* parent_hwnd_ = nullptr;
  SharedSurface latest_{};
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

  
  void SetObserver(MapContentsObserver* observer) override { observer_ = observer; }
  bool WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) override;

  void ActivateTool(uint32_t view_id, const char* tool_id) override;
  void Activate(uint32_t view_id, const char* tool_id);
  void Dispatch(uint32_t view_id, const InputEvent& e) override;

  bool send_json(HostMsg type, uint32_t view_id, const std::string& json);
  void store_surface(uint32_t view_id, const SharedSurface& s);

 private:
  void recv_loop();
  void handle_frame(const FrameHeader& h, const std::vector<uint8_t>& payload);
  HANDLE job_ = nullptr;
  HANDLE process_ = nullptr;
  Pipe pipe_;
  std::thread recv_thread_;
  std::atomic<bool> running_{false};
  std::atomic<uint32_t> next_view_id_{1};
  MapContentsObserver* observer_ = nullptr;
  mutable std::mutex mu_;
  std::map<uint32_t, MapWidgetHostViewImpl*> views_;
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
  char json[128];
  sprintf_s(json, "{\"w\":%d,\"h\":%d,\"dpi\":%.2f}", width_px, height_px,
            static_cast<double>(dpi));
  session_->send_json(HostMsg::kResizeSurface, view_id_, json);
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
  char json[80];
  sprintf_s(json, "{\"present_mode\":\"%s\"}", present_mode_json(mode));
  session_->send_json(HostMsg::kAttachSurface, view_id_, json);
}

void MapWidgetHostViewImpl::SetVisible(bool visible) {
  char json[40];
  sprintf_s(json, "{\"visible\":%s}", visible ? "true" : "false");
  session_->send_json(HostMsg::kAttachSurface, view_id_, json);
}

SharedSurface MapWidgetHostViewImpl::Latest() const {
  return latest_;
}

bool MapContentsImpl::send_json(HostMsg type,
                               uint32_t view_id,
                               const std::string& json) {
  return pipe_.send_json(type, view_id, json);
}

void MapContentsImpl::store_surface(uint32_t view_id, const SharedSurface& s) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  if (it != views_.end() && it->second) {
    it->second->SetLatest(s);
  }
}

bool MapContentsImpl::StartRenderProcess() {
  if (running_) {
    return true;
  }
  if (!frame_event_) {
    frame_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }
  const uint32_t pid = GetCurrentProcessId();
  const std::wstring pipe_path = pipe_path_for_pid(pid);
  if (!pipe_.create_server(pipe_path)) {
    return false;
  }

  job_ = CreateJobObjectW(nullptr, nullptr);
  if (job_) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &info,
                            sizeof(info));
  }

  const std::wstring exe = this_exe_path();
  const std::wstring session = make_session_id();
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
  process_ = pi.hProcess;
  CloseHandle(pi.hThread);
  if (job_) {
    AssignProcessToJobObject(job_, process_);
  }

  if (!pipe_.wait_client(15000)) {
    Shutdown();
    return false;
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
  return pipe_.send_json(HostMsg::kHelloAck, 0,
                         "{\"protocol\":1,\"role\":\"chrome\",\"ok\":true}");
}

void MapContentsImpl::Shutdown() {
  oop_ = false;
  status_ = L"down";
  running_ = false;
  pipe_.send_json(HostMsg::kShutdown, 0, "{}");
  pipe_.close();
  if (recv_thread_.joinable()) {
    recv_thread_.join();
  }
  if (process_) {
    WaitForSingleObject(process_, 3000);
    CloseHandle(process_);
    process_ = nullptr;
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
  char json[64];
  sprintf_s(json, "{\"kind\":\"%s\"}", view_kind_json(kind));
  pipe_.send_json(HostMsg::kOpenView, id, json);
  return id;
}

void MapContentsImpl::CloseView(uint32_t view_id) {
  pipe_.send_json(HostMsg::kCloseView, view_id, "{}");
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  if (it != views_.end()) {
    delete it->second;
    views_.erase(it);
  }
}

MapWidgetHostView* MapContentsImpl::AttachSurface(uint32_t view_id, PresentMode mode) {
  char json[80];
  sprintf_s(json, "{\"present_mode\":\"%s\"}", present_mode_json(mode));
  pipe_.send_json(HostMsg::kAttachSurface, view_id, json);
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  if (it != views_.end()) {
    return it->second;
  }
  auto* v = new MapWidgetHostViewImpl(this, view_id);
  views_[view_id] = v;
  return v;
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
  char json[160];
  sprintf_s(json, "{\"xmin\":%.17g,\"ymin\":%.17g,\"xmax\":%.17g,\"ymax\":%.17g}",
            e.xmin, e.ymin, e.xmax, e.ymax);
  pipe_.send_json(HostMsg::kSetExtent, view_id, json);
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
  char json[48];
  sprintf_s(json, "{\"count\":%u}", static_cast<unsigned>(n));
  pipe_.send_json(HostMsg::kSetSelection, view_id, json);
}

void MapContentsImpl::LegendSnapshot(uint32_t view_id) {
  pipe_.send_json(HostMsg::kLegendQuery, view_id, "{}");
}

void MapContentsImpl::CatalogCall(const char* json_op) {
  pipe_.send_json(HostMsg::kCatalogOp, 0, json_op ? json_op : "{}");
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
  char json[160];
  sprintf_s(json, "{\"tool_id\":\"%s\"}", tool_id ? tool_id : "");
  pipe_.send_json(HostMsg::kActivateTool, view_id, json);
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
  pipe_.send_binary(HostMsg::kPointerEvent, view_id, &w, sizeof(w));
}

void MapContentsImpl::recv_loop() {
  while (running_) {
    FrameHeader h = {};
    std::vector<uint8_t> payload;
    if (!pipe_.recv(&h, &payload, 500)) {
      if (!running_ || !pipe_.is_open()) {
        if (observer_) {
          observer_->OnRenderDied();
        }
        break;
      }
      continue;
    }
    handle_frame(h, payload);
  }
}

void MapContentsImpl::handle_frame(const FrameHeader& h,
                                  const std::vector<uint8_t>& payload) {
  const auto type = static_cast<HostMsg>(h.type);
  if (type == HostMsg::kHello) {
    hello_ok_ = 1;
    return;
  }
  if (type == HostMsg::kSharedHandle &&
      payload.size() >= sizeof(SharedHandleWire)) {
    SharedHandleWire w = {};
    std::memcpy(&w, payload.data(), sizeof(w));
    SharedSurface s = {};
    s.generation = w.generation;
    s.nt_handle = reinterpret_cast<void*>(static_cast<uintptr_t>(w.nt_handle));
    s.width_px = w.width_px;
    s.height_px = w.height_px;
    s.format = w.format;
    store_surface(h.view_id, s);
    return;
  }
  if (type == HostMsg::kFrameReady &&
      payload.size() >= sizeof(FrameReadyWire)) {
    FrameReadyWire w = {};
    std::memcpy(&w, payload.data(), sizeof(w));
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
  if (type == HostMsg::kExtentChanged && !payload.empty()) {
    const std::string json(payload.begin(), payload.end());
    Extent2 e = {};
    e.xmin = json_get_double(json, "xmin", 0);
    e.ymin = json_get_double(json, "ymin", 0);
    e.xmax = json_get_double(json, "xmax", 0);
    e.ymax = json_get_double(json, "ymax", 0);
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

}  // namespace content
