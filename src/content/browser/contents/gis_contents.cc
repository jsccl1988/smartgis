// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/gis_contents.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#include "content/browser/contents/contents_host_pipe.h"
#include "content/public/event_bus.h"
#include "content/public/gis_document.h"
#include "content/public/plugin_host.h"
#include "tool/command/command.h"

#include "base/ipc/handle/handle.h"
#include "base/ipc/invitation/invitation.h"
#include "base/ipc/receiver/receiver.h"
#include "base/trace/event/process_trace.h"
#include "content/app/process_type.h"
#include "content/browser/child/child_process_host.h"
#include "content/common/ipc.h"

#include <dxgi.h>

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

using CreateDxgiFactory1Fn = HRESULT(WINAPI*)(REFIID, void**);

// Load CreateDXGIFactory1 without linking dxgi.lib (content.dll uses
// /NODEFAULTLIB; pragma comment(lib) is ignored).
CreateDxgiFactory1Fn load_create_dxgi_factory1() {
  HMODULE dxgi = LoadLibraryW(L"dxgi.dll");
  if (!dxgi) {
    return nullptr;
  }
  return reinterpret_cast<CreateDxgiFactory1Fn>(
      GetProcAddress(dxgi, "CreateDXGIFactory1"));
}

// Fill DXGI AdapterLuid for the monitor nearest to |hwnd|.
// Leaves *low/*high unchanged when hwnd is null or DXGI lookup fails.
void fill_monitor_luid(HWND hwnd, uint32_t* low, uint32_t* high) {
  if (!hwnd || !low || !high) {
    return;
  }
  const HMONITOR monitor =
      MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
  if (!monitor) {
    return;
  }
  const CreateDxgiFactory1Fn create_factory = load_create_dxgi_factory1();
  if (!create_factory) {
    return;
  }
  IDXGIFactory1* factory = nullptr;
  if (FAILED(create_factory(__uuidof(IDXGIFactory1),
                            reinterpret_cast<void**>(&factory))) ||
      !factory) {
    return;
  }
  bool matched = false;
  for (UINT i = 0; !matched; ++i) {
    IDXGIAdapter1* adapter = nullptr;
    if (factory->EnumAdapters1(i, &adapter) == DXGI_ERROR_NOT_FOUND) {
      break;
    }
    if (!adapter) {
      continue;
    }
    DXGI_ADAPTER_DESC1 desc = {};
    if (FAILED(adapter->GetDesc1(&desc))) {
      adapter->Release();
      continue;
    }
    for (UINT oi = 0; !matched; ++oi) {
      IDXGIOutput* output = nullptr;
      if (adapter->EnumOutputs(oi, &output) == DXGI_ERROR_NOT_FOUND) {
        break;
      }
      if (!output) {
        continue;
      }
      DXGI_OUTPUT_DESC od = {};
      if (SUCCEEDED(output->GetDesc(&od)) && od.Monitor == monitor) {
        *low = static_cast<uint32_t>(desc.AdapterLuid.LowPart);
        *high = static_cast<uint32_t>(desc.AdapterLuid.HighPart);
        matched = true;
      }
      output->Release();
    }
    adapter->Release();
  }
  factory->Release();
}

}  // namespace

class WidgetHostViewImpl final : public WidgetHostView {
 public:
  WidgetHostViewImpl(class GisContentsImpl* session, uint32_t view_id)
      : session_(session), view_id_(view_id) {}
  ~WidgetHostViewImpl() override {
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
  class GisContentsImpl* session_;
  uint32_t view_id_;
  void* parent_hwnd_ = nullptr;
  mutable std::mutex latest_mu_;
  SharedSurface latest_{};
  PresentMode present_mode_ = PresentMode::kSharedTexture;
};

// Forwards GisDocument mutators and also drives the OOP host pipe + observer.
class PipeDocument final : public GisDocument {
 public:
  PipeDocument(std::unique_ptr<GisDocument> owned,
               GisDocument* borrowed,
               ContentsHostPipe* pipe)
      : owned_(std::move(owned)),
        inner_(owned_ ? owned_.get() : borrowed),
        pipe_(pipe) {}

  bool create_layer(std::string_view name,
                    std::string_view geometry_type) override {
    return inner_ ? inner_->create_layer(name, geometry_type) : false;
  }
  bool remove_layer(std::string_view id) override {
    return inner_ ? inner_->remove_layer(id) : false;
  }
  bool set_layer_visible(std::string_view id, bool visible) override {
    return inner_ ? inner_->set_layer_visible(id, visible) : false;
  }
  size_t layer_count() const override {
    return inner_ ? inner_->layer_count() : 0;
  }
  size_t feature_count() const override {
    return inner_ ? inner_->feature_count() : 0;
  }
  FeatureId append_from_draft(
      const tool::Draft& draft, const char* tool_id,
      const std::function<void(int view_x, int view_y, double* map_x,
                               double* map_y)>& to_map) override {
    return inner_ ? inner_->append_from_draft(draft, tool_id, to_map)
                  : FeatureId{};
  }
  bool update_feature_field(std::string_view token, std::string_view field,
                            std::string_view value) override {
    return inner_ ? inner_->update_feature_field(token, field, value) : false;
  }
  bool apply_style_json(std::string_view json) override {
    if (!inner_ || !inner_->apply_style_json(json)) {
      return false;
    }
    notify_style_changed(0);
    return true;
  }
  bool add_triangle_mesh(std::string_view name, const double* xyz,
                         int point_count, const int* triangles,
                         int triangle_count) override {
    return inner_ ? inner_->add_triangle_mesh(name, xyz, point_count, triangles,
                                              triangle_count)
                  : false;
  }
  bool add_point_cloud(std::string_view name, const float* xyz, int point_count,
                       const uint8_t* rgba) override {
    return inner_ ? inner_->add_point_cloud(name, xyz, point_count, rgba)
                  : false;
  }
  bool compute_extent(Extent2* out) const override {
    return inner_ ? inner_->compute_extent(out) : false;
  }
  void notify_layers_changed() override {
    if (inner_) {
      inner_->notify_layers_changed();
    }
  }
  void set_selection(uint32_t view_id,
                     const FeatureId* ids,
                     size_t n) override {
    if (inner_) {
      inner_->set_selection(view_id, ids, n);
    }
    if (pipe_) {
      pipe_->pipe_set_selection(view_id, ids, n);
    }
  }
  void catalog_call(const char* json_op) override {
    if (inner_) {
      inner_->catalog_call(json_op);
    }
    if (pipe_) {
      pipe_->pipe_catalog_call(json_op);
    }
  }
  void legend_snapshot(uint32_t view_id) override {
    if (inner_) {
      inner_->legend_snapshot(view_id);
    }
    if (pipe_) {
      pipe_->pipe_legend_snapshot(view_id);
    }
  }
  void notify_style_changed(uint32_t view_id) override {
    if (inner_) {
      inner_->notify_style_changed(view_id);
    }
    if (pipe_) {
      if (GisContentsObserver* obs = pipe_->contents_observer()) {
        obs->OnStyleChanged(view_id);
      }
    }
  }

 private:
  std::unique_ptr<GisDocument> owned_;
  GisDocument* inner_ = nullptr;
  ContentsHostPipe* pipe_ = nullptr;
};

class GisContentsImpl final : public GisContents, public ContentsHostPipe {
 public:
  GisContentsImpl() = default;
  ~GisContentsImpl() override { Shutdown(); }

  bool StartRenderProcess() override;
  void Shutdown() override;
  bool IsOopRender() const override { return oop_; }
  const wchar_t* PresentStatus() const override { return status_.c_str(); }

  uint32_t OpenView(ViewKind kind) override;
  void CloseView(uint32_t view_id) override;
  WidgetHostView* AttachSurface(uint32_t view_id, PresentMode mode) override;
  WidgetHostView* HostView(uint32_t view_id) override;

  void SetExtent(uint32_t view_id, const Extent2& e) override;
  Extent2 Extent(uint32_t view_id) const override;

  void SetObserver(GisContentsObserver* observer) override {
    observer_ = observer;
  }
  bool WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) override;

  void Dispatch(uint32_t view_id, const InputEvent& e) override;
  void SetRenderBackend(uint32_t kind) override;
  uint32_t RenderBackend() const override { return backend_kind_; }

  GisDocument* gis_document() override { return gis_doc_; }
  void set_gis_document(GisDocument* doc) override {
    if (!doc) {
      pipe_doc_.reset();
      gis_doc_ = nullptr;
      return;
    }
    pipe_doc_ = std::make_unique<PipeDocument>(nullptr, doc, this);
    gis_doc_ = pipe_doc_.get();
  }
  void take_gis_document(std::unique_ptr<GisDocument> doc) override {
    if (!doc) {
      pipe_doc_.reset();
      gis_doc_ = nullptr;
      return;
    }
    pipe_doc_ = std::make_unique<PipeDocument>(std::move(doc), nullptr, this);
    gis_doc_ = pipe_doc_.get();
  }

  PluginHost* plugin_host() override { return plugin_host_.get(); }
  PluginHost* ensure_plugin_host(tool::CommandCatalog* catalog,
                                 EventBus* events) override {
    if (plugin_host_) {
      return plugin_host_.get();
    }
    plugin_host_.reset(create_plugin_host(catalog, events, this));
    bind_event_bus(events);
    return plugin_host_.get();
  }

  void pipe_set_selection(uint32_t view_id,
                          const FeatureId* ids,
                          size_t n) override;
  void pipe_legend_snapshot(uint32_t view_id) override;
  void pipe_catalog_call(const char* json_op) override;
  void pipe_dispatch_plugin(uint32_t view_id,
                            const char* plugin_id,
                            const char* method,
                            const void* bytes,
                            size_t n) override;
  void pipe_activate_tool(uint32_t view_id, const char* tool_id) override;
  GisContentsObserver* contents_observer() const override { return observer_; }

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
  void bind_event_bus(EventBus* events);
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
    GisContentsImpl* self = nullptr;
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
  GisContentsObserver* observer_ = nullptr;
  mutable std::mutex mu_;
  std::map<uint32_t, WidgetHostViewImpl*> views_;
  // Frames can arrive before AttachSurface inserts the HostView.
  std::map<uint32_t, SharedSurface> pending_surfaces_;
  std::map<uint32_t, Extent2> extents_;
  std::map<uint32_t, uint32_t> frame_gen_;
  HANDLE frame_event_ = nullptr;
  uint32_t hello_ok_ = 0;
  bool oop_ = false;
  std::wstring status_ = L"down";
  uint32_t backend_kind_ = 0;
  std::unique_ptr<PipeDocument> pipe_doc_;
  GisDocument* gis_doc_ = nullptr;
  // Owned here; PluginShell holds a non-owning PluginHost*.
  std::unique_ptr<PluginHost> plugin_host_;
  EventBus::Connection selection_sub_;
  EventBus::Connection layers_sub_;
  EventBus::Connection style_sub_;
  EventBus::Connection edit_sub_;
  EventBus::Connection backend_sub_;
};

ContentsHostPipe* as_contents_host_pipe(GisContents* contents) {
  return dynamic_cast<ContentsHostPipe*>(contents);
}

void WidgetHostViewImpl::Create(const CreateParams& params,
                         const Preferences&) {
  parent_hwnd_ = params.parent_hwnd;
}

void WidgetHostViewImpl::Resize(int width_px, int height_px, float dpi) {
  ResizeSurfaceBody body;
  body.w = static_cast<uint32_t>(width_px);
  body.h = static_cast<uint32_t>(height_px);
  body.dpi = dpi;
  if (parent_hwnd_) {
    fill_monitor_luid(static_cast<HWND>(parent_hwnd_), &body.monitor_luid_low,
                      &body.monitor_luid_high);
  }
  session_->send_msg(HostMsg::kResizeSurface, view_id_, body);
}

void WidgetHostViewImpl::Resize(int x,
                         int y,
                         int width_px,
                         int height_px,
                         float dpi) {
  (void)x;
  (void)y;
  Resize(width_px, height_px, dpi);
}

void WidgetHostViewImpl::SetPresentMode(PresentMode mode) {
  present_mode_ = mode;
  AttachSurfaceBody body;
  body.present_mode = static_cast<uint32_t>(mode);
  body.visible = 1;
  if (parent_hwnd_) {
    fill_monitor_luid(static_cast<HWND>(parent_hwnd_), &body.monitor_luid_low,
                      &body.monitor_luid_high);
  }
  session_->send_msg(HostMsg::kAttachSurface, view_id_, body);
}

void WidgetHostViewImpl::SetVisible(bool visible) {
  AttachSurfaceBody body;
  body.present_mode = static_cast<uint32_t>(present_mode_);
  body.visible = visible ? 1u : 0u;
  if (parent_hwnd_) {
    fill_monitor_luid(static_cast<HWND>(parent_hwnd_), &body.monitor_luid_low,
                      &body.monitor_luid_high);
  }
  session_->send_msg(HostMsg::kAttachSurface, view_id_, body);
}

SharedSurface WidgetHostViewImpl::Latest() const {
  std::lock_guard<std::mutex> lock(latest_mu_);
  return latest_;
}

bool GisContentsImpl::send_msg_empty(HostMsg type, uint32_t view_id) {
  return pipe_.send_empty(type, view_id);
}

void GisContentsImpl::store_surface(uint32_t view_id, const SharedSurface& s) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  if (it != views_.end() && it->second) {
    it->second->SetLatest(s);
    pending_surfaces_.erase(view_id);
    return;
  }
  pending_surfaces_[view_id] = s;
}

bool GisContentsImpl::launch_invited_child(const std::wstring& exe,
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

bool GisContentsImpl::StartRenderProcess() {
  if (running_) {
    return true;
  }
  if (!frame_event_) {
    frame_event_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
  }

  // Job is opened before the pipe so a failed CreateProcess still leaves
  // job_ for Shutdown, matching the previous inline launch.
  job_ = ChildProcessHost::open_job();

  const uint32_t pid = GetCurrentProcessId();
  // Classic named-pipe GPU child. Invitation dual-launch is WIP and stalls
  // FrameReady / crashes WinUI --self-test; keep pipe path until proven.
  const std::wstring pipe_path = pipe_path_for_pid(pid);
  if (!pipe_.create_server(pipe_path)) {
    return false;
  }
  HANDLE child = nullptr;
  const ChildProcessHost::Status status = ChildProcessHost::launch(
      ProcessType::kGpu, pipe_name_for_pid(pid), job_,
      [this] { return pipe_.wait_client(15000); }, &child);
  if (status == ChildProcessHost::Status::kCreateFailed) {
    pipe_.close();
    return false;
  }
  if (status != ChildProcessHost::Status::kOk) {
    Shutdown();
    return false;
  }
  process_ = child;

  running_ = true;
  recv_thread_ = std::thread(&GisContentsImpl::recv_loop, this);

  {
    BASE_TRACE_EVENT("HelloWait", "startup");
    const DWORD start = GetTickCount();
    while (hello_ok_ == 0 && GetTickCount() - start < 15000) {
      Sleep(20);
    }
  }
  if (hello_ok_ == 0) {
    Shutdown();
    return false;
  }
  oop_ = true;
  status_ = L"oop";
  HelloBody ack;
  ack.role = "shell";
  return pipe_.send_msg(HostMsg::kHelloAck, 0, ack);
}

void GisContentsImpl::Shutdown() {
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

uint32_t GisContentsImpl::OpenView(ViewKind kind) {
  const uint32_t id = next_view_id_++;
  OpenViewBody body;
  body.kind = static_cast<uint32_t>(kind);
  pipe_.send_msg(HostMsg::kOpenView, id, body);
  if (renderer_pipe_.is_open()) {
    renderer_pipe_.send_msg(HostMsg::kOpenView, id, body);
  }
  return id;
}

void GisContentsImpl::CloseView(uint32_t view_id) {
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

WidgetHostView* GisContentsImpl::AttachSurface(uint32_t view_id, PresentMode mode) {
  // Register HostView before the GPU round-trip so SharedHandle / FrameReady
  // cannot race past an empty views_ map and be dropped.
  WidgetHostViewImpl* view = nullptr;
  {
    std::lock_guard<std::mutex> lock(mu_);
    auto it = views_.find(view_id);
    if (it != views_.end()) {
      view = it->second;
      view->set_present_mode(mode);
    } else {
      view = new WidgetHostViewImpl(this, view_id);
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
  if (view && view->NativeHwnd()) {
    fill_monitor_luid(static_cast<HWND>(view->NativeHwnd()),
                      &body.monitor_luid_low, &body.monitor_luid_high);
  }
  pipe_.send_msg(HostMsg::kAttachSurface, view_id, body);
  return view;
}

WidgetHostView* GisContentsImpl::HostView(uint32_t view_id) {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = views_.find(view_id);
  return it == views_.end() ? nullptr : it->second;
}

void GisContentsImpl::SetExtent(uint32_t view_id, const Extent2& e) {
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

Extent2 GisContentsImpl::Extent(uint32_t view_id) const {
  std::lock_guard<std::mutex> lock(mu_);
  auto it = extents_.find(view_id);
  if (it == extents_.end()) {
    return Extent2{};
  }
  return it->second;
}

void GisContentsImpl::bind_event_bus(EventBus* events) {
  selection_sub_.disconnect();
  layers_sub_.disconnect();
  style_sub_.disconnect();
  edit_sub_.disconnect();
  backend_sub_.disconnect();
  if (!events) {
    return;
  }
  selection_sub_ = events->subscribe<SelectionChanged>(
      [this](const SelectionChanged& ev) {
        if (!observer_) {
          return;
        }
        const FeatureId* ids =
            ev.ids.empty() ? nullptr : ev.ids.data();
        observer_->OnSelectionChanged(ev.view_id, ids, ev.ids.size());
      });
  layers_sub_ = events->subscribe<LayersChanged>(
      [this](const LayersChanged& ev) {
        if (observer_) {
          observer_->OnLayersChanged(ev.view_id, ev.layer_count);
        }
      });
  style_sub_ = events->subscribe<StyleChanged>([this](const StyleChanged& ev) {
    if (observer_) {
      observer_->OnStyleChanged(ev.view_id);
    }
  });
  edit_sub_ = events->subscribe<EditCommitted>(
      [this](const EditCommitted& ev) {
        if (observer_) {
          observer_->OnEditCommitted(ev.view_id, ev.id,
                                     static_cast<int>(ev.op));
        }
      });
  backend_sub_ = events->subscribe<RenderBackendChanged>(
      [this](const RenderBackendChanged& ev) {
        if (observer_) {
          observer_->OnRenderBackendChanged(ev.view_id, ev.kind);
        }
      });
}

void GisContentsImpl::pipe_set_selection(uint32_t view_id,
                                         const FeatureId* ids,
                                         size_t n) {
  (void)ids;
  SelectionBody body;
  body.count = static_cast<uint32_t>(n);
  send_tool_msg(HostMsg::kSetSelection, view_id, body);
}

void GisContentsImpl::pipe_legend_snapshot(uint32_t view_id) {
  send_tool_empty(HostMsg::kLegendQuery, view_id);
}

void GisContentsImpl::pipe_catalog_call(const char* json_op) {
  JsonBody body;
  body.json = json_op ? json_op : "{}";
  send_tool_msg(HostMsg::kCatalogOp, 0, body);
}

void GisContentsImpl::pipe_dispatch_plugin(uint32_t view_id,
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

void GisContentsImpl::pipe_activate_tool(uint32_t view_id,
                                         const char* tool_id) {
  ToolBody body;
  body.tool_id = tool_id ? tool_id : "";
  send_tool_msg(HostMsg::kActivateTool, view_id, body);
}

bool GisContentsImpl::WaitFrameReady(uint32_t view_id, uint32_t timeout_ms) {
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

void GisContentsImpl::SetRenderBackend(uint32_t kind) {
  backend_kind_ = kind == 1u ? 1u : 0u;
  RenderBackendWire body;
  body.kind = backend_kind_;
  // GPU process only — do not route through the renderer pipe.
  pipe_.send_msg(HostMsg::kSetRenderBackend, 0, body);
}

void GisContentsImpl::Dispatch(uint32_t view_id, const InputEvent& e) {
  PointerEventWire w = {};
  w.t_qpc = e.t_qpc;
  w.kind = static_cast<uint32_t>(e.kind);
  w.flags = e.flags;
  w.x_px = e.x_px;
  w.y_px = e.y_px;
  w.wheel = e.wheel;
  w.key = e.key;
  w.dpi = 96.f;
  w.pointer_count = e.pointer_count;
  if (e.kind == InputEvent::Kind::kTextCommit) {
    send_tool_msg(HostMsg::kTextCommit, view_id, w);
    return;
  }
  send_tool_msg(HostMsg::kPointerEvent, view_id, w);
}

void GisContentsImpl::recv_loop() {
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

void GisContentsImpl::renderer_recv_loop() {
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
      ack.role = "shell";
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

void GisContentsImpl::handle_frame(
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

GisContents* create_gis_contents() {
  return new detail::GisContentsImpl();
}

}  // namespace content
