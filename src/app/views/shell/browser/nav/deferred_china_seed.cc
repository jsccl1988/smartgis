// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/browser/browser.h"

#include <cstring>
#include <exception>
#include <string>

#include "app/views/shell/harness/common/io/sample.h"
#include "base/core/log.h"
#include "base/process/switches.h"
#include "base/trace/event/process_trace.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace app {
namespace {

bool seh_fit_map_extent(Browser* browser) {
  if (!browser) {
    return false;
  }
  __try {
    browser->fit_map_extent();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}  // namespace

void Browser::schedule_deferred_china_seed() {
  const bool skip_deferred_china = []() {
    const char* skip = base::switch_cstr("skip-ambox-catalog");
    return skip && skip[0] != '\0' && skip[0] != '0';
  }();
  if (!defer_china_seed_ || skip_deferred_china || !document() ||
      document()->has_china_extent()) {
    return;
  }
  HWND shell = hwnd();
  if (!shell || !IsWindow(shell)) {
    return;
  }
  SetPropW(shell, L"DeferChinaBrowser", reinterpret_cast<HANDLE>(this));
  constexpr UINT_PTR kDeferChina = 0x43484E41u;  // 'CHNA'
  SetTimer(shell, kDeferChina, 1, [](HWND timer_hwnd, UINT, UINT_PTR id,
                                     DWORD) {
    KillTimer(timer_hwnd, id);
    auto* self = reinterpret_cast<Browser*>(
        GetPropW(timer_hwnd, L"DeferChinaBrowser"));
    RemovePropW(timer_hwnd, L"DeferChinaBrowser");
    if (!self || self->is_close_prepared() || !self->document() ||
        self->document()->has_china_extent()) {
      return;
    }
    BASE_TRACE_EVENT("try_open_china", "startup");
    LOGGING(LOG_INFO, "startup: deferred China seed begin");
#if defined(_MSC_VER)
    base::set_switch("skip-china-land-clip", "1");
#else
    setenv("skip-china-land-clip", "1", 1);
#endif
    LOGGING(LOG_INFO, "startup: SKIP_CHINA_LAND_CLIP=%s",
            base::switch_cstr("skip-china-land-clip")
                ? base::switch_cstr("skip-china-land-clip")
                : "(null)");
    if (BrowserUiDelegate* ui = self->ui()) {
      ui->pause_all_presents();
    }
    struct ResumePresents {
      Browser* browser = nullptr;
      ~ResumePresents() {
        if (!browser || browser->is_close_prepared() || !browser->ui()) {
          return;
        }
        browser->ui()->resume_all_presents();
      }
    } resume_presents{self};
    try {
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
      if (!seh_fit_map_extent(self)) {
        LOGGING(LOG_WARNING, "startup: deferred China fit_map_extent SEH");
      }
      self->push_shared_extent();
      if (content::Map2dPresenter* map2d = self->map2d()) {
        map2d->note_surface_reset();
        map2d->invalidate_frame_cache();
      }
      self->refresh_inspectors();
      self->sync_catalog_from_scene();
      if (self->ui()) {
        self->ui()->invalidate_map_overlays();
        self->ui()->invalidate_native_map();
      }
      if (const char* tab = base::switch_cstr("views-start-map-tab")) {
        int idx = -1;
        if (std::strcmp(tab, "scene3d") == 0 || std::strcmp(tab, "2") == 0 ||
            std::strcmp(tab, "1") == 0) {
          idx = 1;
        } else if (std::strcmp(tab, "data") == 0) {
          idx = 0;
        } else if (tab[0] == '0' && tab[1] == '\0') {
          idx = 0;
        }
        if (idx >= 0 && timer_hwnd && IsWindow(timer_hwnd)) {
          constexpr UINT kReselectTab = WM_APP + 0x5354;  // 'ST'
          PostMessageW(timer_hwnd, kReselectTab, static_cast<WPARAM>(idx), 0);
        }
      }
    } catch (const std::exception& ex) {
      LOGGING(LOG_ERROR, "startup: deferred China seed exception: %s",
              ex.what());
    } catch (...) {
      LOGGING(LOG_ERROR, "startup: deferred China seed unknown exception");
    }
#if defined(_MSC_VER)
    base::set_switch("skip-china-land-clip", "");
#else
    unsetenv("skip-china-land-clip");
#endif
    LOGGING(LOG_INFO, "startup: deferred China seed done china=%d",
            self->document() && self->document()->has_china_extent() ? 1 : 0);
  });
}

}  // namespace app
