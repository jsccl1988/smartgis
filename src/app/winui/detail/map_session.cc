// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/winui/detail/map_session.h"

#include "app/winui/detail/local_render.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

#ifndef SMT_WINUI_HAS_CONTENT_PUBLIC

namespace app {
namespace winui {
namespace detail {
namespace {

std::wstring exe_dir() {
  wchar_t path[MAX_PATH];
  const DWORD n = GetModuleFileNameW(nullptr, path, MAX_PATH);
  if (n == 0 || n >= MAX_PATH) {
    return L".";
  }
  std::wstring full(path, path + n);
  const size_t slash = full.find_last_of(L"\\/");
  if (slash == std::wstring::npos) {
    return L".";
  }
  return full.substr(0, slash);
}

bool file_exists(const std::wstring& path) {
  const DWORD attrs = GetFileAttributesW(path.c_str());
  return attrs != INVALID_FILE_ATTRIBUTES &&
         (attrs & FILE_ATTRIBUTE_DIRECTORY) == 0;
}

std::wstring find_render_exe() {
  const std::wstring dir = exe_dir();
  const wchar_t* names[] = {L"SmartGisRender.exe", L"SmartGisRenderD.exe"};
  for (size_t i = 0; i < 2; ++i) {
    const std::wstring cand = dir + L"\\" + names[i];
    if (file_exists(cand)) {
      return cand;
    }
  }
  return std::wstring();
}

class MapViewImpl : public content::MapView {
 public:
  explicit MapViewImpl(uint32_t id) : view_id_(id) {}

  void Create(const CreateParams& params,
              const Preferences& /*preferences*/) override {
    parent_hwnd_ = params.parent_hwnd;
  }

  void Destroy() override { parent_hwnd_ = nullptr; }

  uint32_t view_id() const override { return view_id_; }

  void resize(int width_px, int height_px, float dpi) override {
    width_px_ = width_px > 0 ? width_px : 0;
    height_px_ = height_px > 0 ? height_px : 0;
    dpi_ = dpi;
  }

  void set_present_mode(content::PresentMode mode) override { mode_ = mode; }

  void set_visible(bool visible) override { visible_ = visible; }

  content::SharedSurface latest() const override { return surface_; }

  void set_shared_handle(void* handle,
                         uint32_t w,
                         uint32_t h,
                         uint32_t format) {
    surface_.nt_handle = handle;
    surface_.width_px = w;
    surface_.height_px = h;
    surface_.format = format;
    ++surface_.generation;
  }

 private:
  uint32_t view_id_ = 0;
  HWND parent_hwnd_ = nullptr;
  content::PresentMode mode_ = content::PresentMode::kChildHwnd;
  bool visible_ = true;
  int width_px_ = 0;
  int height_px_ = 0;
  float dpi_ = 96.f;
  content::SharedSurface surface_{};
};

class MapSessionImpl : public content::MapSession {
 public:
  bool start_render_process() override {
    const std::wstring render = find_render_exe();
    if (render.empty()) {
      oop_ = false;
      fallback_probe_ = probe_legacy_render_dlls();
      return false;
    }

    const DWORD pid = GetCurrentProcessId();
    wchar_t cmd[1024];
    swprintf_s(cmd,
               L"\"%s\" --parent-pid=%lu --pipe=smartgis-host-%lu "
               L"--session=winui",
               render.c_str(), static_cast<unsigned long>(pid),
               static_cast<unsigned long>(pid));

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    std::wstring mutable_cmd(cmd);
    if (!CreateProcessW(render.c_str(), &mutable_cmd[0], nullptr, nullptr,
                        FALSE, 0, nullptr, nullptr, &si, &pi)) {
      oop_ = false;
      fallback_probe_ = probe_legacy_render_dlls();
      return false;
    }
    CloseHandle(pi.hThread);
    process_ = pi.hProcess;
    oop_ = true;
    return true;
  }

  void shutdown() override {
    views_.clear();
    if (process_) {
      TerminateProcess(process_, 0);
      CloseHandle(process_);
      process_ = nullptr;
    }
    oop_ = false;
  }

  bool is_oop_render() const override { return oop_; }

  uint32_t open_view(content::ViewKind /*kind*/) override {
    const uint32_t id = ++next_id_;
    views_[id].reset(new MapViewImpl(id));
    return id;
  }

  void close_view(uint32_t view_id) override { views_.erase(view_id); }

  content::MapView* attach_surface(uint32_t view_id,
                                   content::PresentMode mode) override {
    content::MapView* view = map_view(view_id);
    if (view) {
      view->set_present_mode(mode);
    }
    return view;
  }

  content::MapView* map_view(uint32_t view_id) override {
    auto it = views_.find(view_id);
    return it == views_.end() ? nullptr : it->second.get();
  }

  void set_extent(uint32_t view_id, const content::Extent2& e) override {
    extents_[view_id] = e;
  }

  content::Extent2 extent(uint32_t view_id) const override {
    auto it = extents_.find(view_id);
    if (it != extents_.end()) {
      return it->second;
    }
    content::Extent2 empty{};
    return empty;
  }

  void set_selection(uint32_t /*view_id*/,
                     const content::FeatureId* /*ids*/,
                     size_t /*n*/) override {}

  void legend_snapshot(uint32_t /*view_id*/) override {}

  void catalog_call(const char* /*json_op*/) override {}

  void activate_tool(uint32_t /*view_id*/, const char* /*tool_id*/) override {}

  void dispatch(uint32_t /*view_id*/,
                const content::InputEvent& /*e*/) override {}

  const LocalRenderProbe& fallback_probe() const { return fallback_probe_; }

 private:
  HANDLE process_ = nullptr;
  bool oop_ = false;
  uint32_t next_id_ = 0;
  std::map<uint32_t, std::unique_ptr<MapViewImpl>> views_;
  std::map<uint32_t, content::Extent2> extents_;
  LocalRenderProbe fallback_probe_;
};

MapSessionImpl* g_session = nullptr;

}  // namespace
}  // namespace detail
}  // namespace winui
}  // namespace app

namespace content {

MapSession* create_map_session() {
  if (!app::winui::detail::g_session) {
    app::winui::detail::g_session = new app::winui::detail::MapSessionImpl();
  }
  return app::winui::detail::g_session;
}

}  // namespace content

#endif  // SMT_WINUI_HAS_CONTENT_PUBLIC
