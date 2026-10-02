// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <cstdlib>
#include <cstring>
#include <exception>

#include "app/views/shell/browser/browser_ui_delegate.h"
#include "app/views/shell/browser/plugin/analysis_writers.h"
#include "app/views/shell/browser/plugin/plugin_shell.h"
#include "app/views/shell/harness/common/sample.h"
#include "base/core/log.h"
#include "base/trace/event/process_trace.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/public/map_contents.h"
#include "content/public/view_host.h"
#include "ui/views/map/viewport/map_viewport.h"

namespace app {

Browser::Browser() = default;

Browser::~Browser() {
  prepare_close();
  session_.clear_map_contents_observer();
  if (plugins_) {
    plugins_->shutdown();
    plugins_.reset();
  }
  ui_.reset();
}

void Browser::set_plugins_dir(std::string path) {
  plugins_dir_ = std::move(path);
}

void Browser::set_defer_china_seed(bool defer) {
  defer_china_seed_ = defer;
}

bool Browser::defer_china_seed() const {
  return defer_china_seed_;
}

void Browser::set_enable_oop_render(bool enable) {
  enable_oop_render_ = enable;
}

bool Browser::enable_oop_render() const {
  return enable_oop_render_;
}

content::EventBus::Connection* Browser::selection_sub() {
  return &selection_sub_;
}

content::EventBus::Connection* Browser::edit_sub() {
  return &edit_sub_;
}

content::EventBus::Connection* Browser::extent_sub() {
  return &extent_sub_;
}

bool Browser::init() {
  BASE_TRACE_EVENT("Browser.init.body", "startup");
  {
    BASE_TRACE_EVENT("Session.init_hosts", "startup");
    LOGGING(LOG_INFO, "startup: session.init_hosts");
    session_.init_hosts();
    if (enable_oop_render_) {
      if (!session_.ensure_oop_render_process()) {
        LOGGING(LOG_WARNING,
                "startup: enable_oop_render requested but StartRenderProcess "
                "failed — continuing in-process");
      }
    }
  }

  {
    BASE_TRACE_EVENT("PluginShell.init", "startup");
    LOGGING(LOG_INFO, "startup: PluginShell.init");
    plugins_ = std::make_unique<PluginShell>();
    plugins_->set_plugins_dir(plugins_dir_);
    if (!session_.edit_host() ||
        !plugins_->init(session_.edit_host()->events())) {
      LOGGING(LOG_ERROR, "startup: PluginShell.init failed");
      if (plugins_) {
        plugins_->shutdown();
        plugins_.reset();
      }
      // Showcase / self-test can still paint map2d without plugins; product
      // interactive shell keeps hard-fail by returning false below when UI
      // creation also requires plugins. Soft-continue so chrome can load.
      LOGGING(LOG_WARNING, "startup: continuing without PluginShell");
    }
  }

  {
    BASE_TRACE_EVENT("CreateBrowserUi", "startup");
    LOGGING(LOG_INFO, "startup: create_browser_ui");
    ui_ = create_browser_ui(this);
    if (!ui_) {
      LOGGING(LOG_ERROR, "startup: create_browser_ui failed");
      return false;
    }
  }

  wire_plugin_analysis_writers(this);

  BASE_TRACE_EVENT("InitChrome", "startup");
  LOGGING(LOG_INFO, "startup: init_chrome");
  const bool ok = ui_->init_chrome();
  if (!ok) {
    LOGGING(LOG_ERROR, "startup: init_chrome failed");
  }
  return ok;
}

void Browser::show() {
  if (ui_) {
    // show_chrome shows the shell and schedules the first map invalidate.
    // Full first-map present wait is opt-in (SMT_SYNC_FIRST_MAP_PRESENT=1).
    ui_->show_chrome();
  }
  // Fit after chrome is visible (final client size). Extent-only nudge;
  // init_chrome already framed from SeedDocument (demo or sync China).
  // Showcase / harness (SMT_SKIP_AMBOX_CATALOG): demo-only seed AVs inside
  // fit_map_extent (cdb world3d-early2 Browser::show). Scene3D framing is
  // applied later by apply_china_scene3d_product_defaults.
  const bool skip_fit = []() {
    const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
    return skip && skip[0] != '\0' && skip[0] != '0';
  }();
  if (!skip_fit) {
    fit_map_extent();
  }
  navigation_baselined_ = true;
  refresh_scale();

  // P1-2: open China/DEM after first interactive show (product path only).
  // SeedDocument already skipped bootstrap when defer_china_seed_ is set.
  // SMT_SKIP_AMBOX_CATALOG also skips this timer: china city land-clip on the
  // UI thread can run tens of seconds and makes WM_CLOSE look hung.
  const bool skip_deferred_china = []() {
    const char* skip = std::getenv("SMT_SKIP_AMBOX_CATALOG");
    return skip && skip[0] != '\0' && skip[0] != '0';
  }();
  if (defer_china_seed_ && !skip_deferred_china && document() &&
      !document()->has_china_extent()) {
    HWND shell = hwnd();
    if (shell && IsWindow(shell)) {
      SetPropW(shell, L"SmtDeferChinaBrowser", reinterpret_cast<HANDLE>(this));
      constexpr UINT_PTR kDeferChina = 0x43484E41u;  // 'CHNA'
      SetTimer(shell, kDeferChina, 1, [](HWND timer_hwnd, UINT, UINT_PTR id,
                                         DWORD) {
        KillTimer(timer_hwnd, id);
        auto* self = reinterpret_cast<Browser*>(
            GetPropW(timer_hwnd, L"SmtDeferChinaBrowser"));
        RemovePropW(timer_hwnd, L"SmtDeferChinaBrowser");
        if (!self || self->is_close_prepared() || !self->document() ||
            self->document()->has_china_extent()) {
          return;
        }
        BASE_TRACE_EVENT("try_open_china", "startup");
        LOGGING(LOG_INFO, "startup: deferred China seed begin");
        // GDAL/OGR land-clip can throw; an uncaught exception on this timer
        // becomes std::terminate → ExitProcess(-1) with no second-chance AV.
        // Skip O(n×m) land-clip on this path so the UI thread returns quickly;
        // sync / showcase seeds keep the full clip for ocean cleanup.
        // Use CRT _putenv_s — MSVC getenv() does not see SetEnvironmentVariableA.
#if defined(_MSC_VER)
        _putenv_s("SMT_SKIP_CHINA_LAND_CLIP", "1");
#else
        setenv("SMT_SKIP_CHINA_LAND_CLIP", "1", 1);
#endif
        LOGGING(LOG_INFO, "startup: SMT_SKIP_CHINA_LAND_CLIP=%s",
                std::getenv("SMT_SKIP_CHINA_LAND_CLIP")
                    ? std::getenv("SMT_SKIP_CHINA_LAND_CLIP")
                    : "(null)");
        // Pause Present timers before LayerStore replace — concurrent FlyCube
        // present + GDAL open/replace_layers can hang the UI thread forever
        // (seed begin with no done). Same pattern as console_self_test.
        auto stop_present = [](ui::views::MapViewport* pane) {
          if (!pane) {
            return;
          }
          pane->set_flycube_present_visible(false);
          HWND nv = pane->native_view();
          if (nv && IsWindow(nv)) {
            KillTimer(nv, 1);
          }
        };
        if (self->ui()) {
          stop_present(self->ui()->map_viewport());
          stop_present(self->ui()->map_data_viewport());
          stop_present(self->ui()->map_scene_viewport());
        }
        try {
          // Replace demo layer with china_city / PLP (same paths as sync seed).
          self->document()->seed_default(/*allow_china_bootstrap=*/true);
          if (self->is_close_prepared()) {
            return;
          }
          if (!self->document()->has_china_extent()) {
            (void)detail::try_open_china_sample(
                *self, /*write_stub_if_missing=*/false);
          }
          if (self->is_close_prepared()) {
            return;
          }
          self->fit_map_extent();
          self->push_shared_extent();
          self->refresh_inspectors();
          self->sync_catalog_from_scene();
          if (self->ui()) {
            self->ui()->invalidate_map_overlays();
          }
          // Do not call select_map_tab here — SMT_VIEWS_START_MAP_TAB may
          // already be inside switch_map_tab's PeekMessage wait; nested select
          // deadlocks the China-seed timer. Post a one-shot re-select after.
          if (const char* tab = std::getenv("SMT_VIEWS_START_MAP_TAB")) {
            int idx = -1;
            if (std::strcmp(tab, "scene3d") == 0 ||
                std::strcmp(tab, "2") == 0) {
              idx = 2;
            } else if (std::strcmp(tab, "data") == 0 ||
                       std::strcmp(tab, "1") == 0) {
              idx = 1;
            } else if (tab[0] == '0' && tab[1] == '\0') {
              idx = 0;
            }
            if (idx >= 0 && timer_hwnd && IsWindow(timer_hwnd)) {
              constexpr UINT kReselectTab = WM_APP + 0x5354;  // 'ST'
              PostMessageW(timer_hwnd, kReselectTab, static_cast<WPARAM>(idx),
                           0);
            }
          }
        } catch (const std::exception& ex) {
          LOGGING(LOG_ERROR, "startup: deferred China seed exception: %s",
                  ex.what());
        } catch (...) {
          LOGGING(LOG_ERROR, "startup: deferred China seed unknown exception");
        }
#if defined(_MSC_VER)
        _putenv_s("SMT_SKIP_CHINA_LAND_CLIP", "");
#else
        unsetenv("SMT_SKIP_CHINA_LAND_CLIP");
#endif
        LOGGING(LOG_INFO, "startup: deferred China seed done china=%d",
                self->document() && self->document()->has_china_extent() ? 1
                                                                         : 0);
      });
    }
  }
}

int Browser::run_loop() {
  return ui_ ? ui_->run_chrome_loop() : 1;
}

void Browser::prepare_close() {
  if (prepare_close_done_) {
    return;
  }
  prepare_close_done_ = true;
  // Cancel deferred China seed before it blocks the UI thread on land-clip
  // (WM_CLOSE cannot run until that timer callback returns).
  if (HWND shell = hwnd()) {
    if (IsWindow(shell)) {
      constexpr UINT_PTR kDeferChina = 0x43484E41u;  // 'CHNA'
      KillTimer(shell, kDeferChina);
      RemovePropW(shell, L"SmtDeferChinaBrowser");
    }
  }
  // Detach MapViewport / join Display before abandon_mesh. Reversing that
  // order lets Scene3d present hold present_mu_ while the UI thread blocks in
  // abandon, and release_rhi_device waits forever for a destroy ack the Display
  // thread cannot process (close hang).
  if (ui_) {
    ui_->prepare_chrome_close();
  }
  session_.prepare_close();
}

HWND Browser::hwnd() const {
  return ui_ ? ui_->hwnd() : nullptr;
}

ui::views::View* Browser::contents_view() const {
  return ui_ ? ui_->contents_view() : nullptr;
}

ui::views::CatalogView* Browser::catalog_view() const {
  return ui_ ? ui_->catalog_view() : nullptr;
}

ui::views::AmboxView* Browser::ambox_view() const {
  return ui_ ? ui_->ambox_view() : nullptr;
}

ui::views::StatusBar* Browser::status_bar() const {
  return ui_ ? ui_->status_bar() : nullptr;
}

ui::views::FeatureInfo* Browser::feature_info() const {
  return ui_ ? ui_->feature_info() : nullptr;
}

ui::views::AttributeTable* Browser::attribute_table() const {
  return ui_ ? ui_->attribute_table() : nullptr;
}

ui::views::ProcessingPanel* Browser::processing_panel() const {
  return ui_ ? ui_->processing_panel() : nullptr;
}

ui::views::MapViewport* Browser::map_viewport() const {
  return ui_ ? ui_->map_viewport() : nullptr;
}

ui::views::MapViewport* Browser::map_data_viewport() const {
  return ui_ ? ui_->map_data_viewport() : nullptr;
}

ui::views::MapViewport* Browser::map_scene_viewport() const {
  return ui_ ? ui_->map_scene_viewport() : nullptr;
}

content::ViewHost* Browser::edit_view_host() const {
  return session_.edit_host();
}

void Browser::refit_active_view() {
  fit_map_extent();
}

void Browser::refresh_inspectors() {
  if (ui_) {
    ui_->sync_inspectors_from_scene();
  }
}

void Browser::sync_catalog_from_scene() {
  if (ui_) {
    ui_->sync_catalog_from_scene();
  }
}

void Browser::select_map_tab(int index) {
  if (ui_) {
    ui_->select_map_tab(index);
  }
}

void Browser::OnExtentChanged(uint32_t /*view_id*/, const content::Extent2& e) {
  if (syncing_extent_ || !extent_nonempty(e)) {
    return;
  }
  // 2D ViewFrame is owned by shell pan/wheel navigation. Applying remote
  // ExtentChanged here races in-flight push_shared_extent echoes and undoes
  // cursor zoom (self-test exit 47). Orbit tracks only a China lon/lat box;
  // pixel or world extents shrink the DEM into a sticker on the ocean.
  if (!extent_looks_like_china(e)) {
    return;
  }
  syncing_extent_ = true;
  session_.orbit_frame().apply_world_extent(e);
  syncing_extent_ = false;
  if (ui_) {
    refresh_scale();
    ui_->invalidate_map_overlays();
  }
}

}  // namespace app
