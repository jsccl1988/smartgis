// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/seed/china_seed.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/browser/china_product_defaults.h"
#include "app/views/shell/harness/common/mark/mark.h"
#include "app/views/shell/harness/common/io/sample.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/ui/session/shell_prep.h"
#include "content/browser/camera/map_host_extent.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/map/map_viewport.h"
#include "ui/views/primitives/collection/tab_strip.h"

#include <cstdlib>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include "base/process/switches.h"

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

void client_size(Browser* browser, int* w, int* h) {
  if (!browser || !w || !h) {
    return;
  }
  *w = 1280;
  *h = 720;
  if (HWND hwnd = browser->hwnd()) {
    RECT rc = {};
    if (GetClientRect(hwnd, &rc)) {
      if (rc.right > 64) {
        *w = rc.right;
      }
      if (rc.bottom > 64) {
        *h = rc.bottom;
      }
    }
  }
}

// Prefer POD China framing over fit_map_extent (AV → china-fit-seh → hollow map).
bool seh_frame_china_map2d(Browser* browser, int view_w, int view_h) {
  if (!browser) {
    return false;
  }
  __try {
    frame_china_map2d(*browser, view_w, view_h);
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

bool seh_apply_china_extent(Browser* browser, int view_w, int view_h) {
  if (!browser || !browser->view_frame()) {
    return false;
  }
  __try {
    browser->view_frame()->apply_world_extent(content::kChinaMap2dFrameExtent,
                                              view_w, view_h);
    if (content::Map2dPresenter* map2d = browser->map2d()) {
      map2d->invalidate_frame_cache();
    }
    browser->push_shared_extent();
    return true;
  } __except (EXCEPTION_EXECUTE_HANDLER) {
    return false;
  }
}

}  // namespace

void ensure_ui_showcase_china_map(Browser& browser, UiShowcaseMode mode) {
  // Scene capture prefers GDI placeholder for PrintWindow; do not force a
  // heavy China OGR replace onto the FlyCube path here.
  if (mode == UiShowcaseMode::kScene) {
    return;
  }
  if (!browser.document()) {
    showcase_mark("china-seed-nodoc");
    return;
  }
  // ContentMapView SharedSurface can present an empty ocean DIB while software
  // still draws land — force full Map2dPresenter paint on the overlay HWND so
  // PrintWindow / review-prep sees carto (same as map2d/plugin showcase).
  base::set_switch("force-gdi-map-overlay", "1");
  // Keep skip flags for the whole showcase path (clearing before pump let
  // land-clip / hillshade hang after china-catalog-ok).
  base::set_switch("skip-china-land-clip", "1");
  base::set_switch("map2d-no-hillshade", "1");
  // Pause present timers before LayerStore replace (no FlyCube hide — that
  // path AVd under parallel out/ churn when viewport native was mid-teardown).
  stop_ui_map_present(browser);

  bool ok = browser.document()->has_china_extent();
  if (!ok) {
    browser.document()->seed_default(/*allow_china_bootstrap=*/true);
    ok = browser.document()->has_china_extent();
  }
  if (!ok) {
    ok = try_open_china_sample(browser, /*write_stub_if_missing=*/false);
    ok = ok && browser.document()->has_china_extent();
  }

  if (!ok) {
    showcase_mark("china-seed-miss");
    return;
  }
  showcase_mark("china-seed-ok");

  // Skip product-defaults fit (AV under carto churn). Frame via POD China
  // extent so the Map HWND is not a hollow dark ocean for PrintWindow.
  base::set_switch("skip-china-map2d-defaults", "1");
  ensure_china_maplibre_carto(browser);
  int view_w = 1280;
  int view_h = 720;
  client_size(&browser, &view_w, &view_h);
  if (seh_frame_china_map2d(&browser, view_w, view_h)) {
    showcase_mark("china-fit-ok");
  } else if (seh_apply_china_extent(&browser, view_w, view_h)) {
    showcase_mark("china-fit-extent");
  } else {
    showcase_mark("china-fit-seh");
  }
  showcase_mark("china-catalog-pre");
  browser.sync_catalog_from_scene();
  showcase_mark("china-catalog-synced");
  if (ui::views::CatalogView* cat = browser.catalog_view()) {
    showcase_mark("china-catalog-view");
    cat->set_source_names({"china_city"});
    showcase_mark("china-catalog-src");
    cat->set_map_docs({{"china_city", "", "China", false}});
    showcase_mark("china-catalog-docs");
    // Layers TOC is the review gate; keep it active after Maps seed.
    if (ui::views::TabStrip* tabs = cat->source_tabs()) {
      tabs->set_active(0);
    }
    // Second sync after Maps/Sources seed + forced TabStrip/LayerTree layout
    // so rows are not stuck invisible from a zero-height first pass (#2).
    browser.sync_catalog_from_scene();
    if (ui::views::TabStrip* tabs = cat->source_tabs()) {
      tabs->layout();
    }
    if (ui::views::LayerTree* tree = cat->layer_tree()) {
      if (tree->layer_count() == 0) {
        showcase_mark("china-catalog-empty");
      } else {
        showcase_mark("china-catalog-rows");
      }
      tree->layout();
      tree->schedule_paint();
    }
    cat->schedule_paint();
  }
  showcase_mark("china-catalog-ok");
  // Prefer HWND invalidate over ui()->invalidate_map_overlays(): the latter
  // AVd after china-catalog-ok under DLL churn (no china-ready mark).
  if (HWND hwnd = browser.hwnd()) {
    InvalidateRect(hwnd, nullptr, TRUE);
  }
  if (ui::views::MapViewport* map = browser.map_viewport()) {
    map->invalidate_native();
  }
  // FORCE_GDI overlay needs a few paint ticks before PrintWindow.
  pump_views_messages(250);
  showcase_mark("china-ready");
}

}  // namespace detail
}  // namespace app
