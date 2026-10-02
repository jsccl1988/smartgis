// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/ui/china_seed.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/mark.h"
#include "app/views/shell/harness/common/sample.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "ui/gis/catalog/catalog_view.h"

#include <cstdlib>

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
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
  _putenv_s("SMT_FORCE_GDI_MAP_OVERLAY", "1");
  // Skip O(n×m) land-clip on the UI thread (product deferred-seed path).
  _putenv_s("SMT_SKIP_CHINA_LAND_CLIP", "1");

  bool ok = browser.document()->has_china_extent();
  if (!ok) {
    browser.document()->seed_default(/*allow_china_bootstrap=*/true);
    ok = browser.document()->has_china_extent();
  }
  if (!ok) {
    ok = try_open_china_sample(browser, /*write_stub_if_missing=*/false);
    ok = ok && browser.document()->has_china_extent();
  }
  _putenv_s("SMT_SKIP_CHINA_LAND_CLIP", "");

  if (!ok) {
    showcase_mark("china-seed-miss");
    return;
  }
  showcase_mark("china-seed-ok");

  // init/show skipped fit_map_extent under SMT_SKIP_AMBOX_CATALOG.
  // fit_map_extent already applies China carto framing + overlay invalidate.
  browser.fit_map_extent();
  showcase_mark("china-fit-ok");
  // Refresh Layers from MapScene after OGR replace. Prior permanent skip left
  // Demo layer in Catalog while Map showed China (bug #10 residual).
  // Root causes addressed together:
  //  1) LayerTree::set_layers used to fire selection_changed → CatalogCall +
  //     fill_attribute_rows over every MapLayer feature (AV / hang).
  //  2) Inline browser.ui() from this TU can read freefill when Browser layout
  //     skews under parallel ninja — use non-inline Browser::sync_catalog_* /
  //     catalog_view() defined in browser.cc (same pattern as fit_map_extent).
  showcase_mark("china-catalog-pre");
  browser.sync_catalog_from_scene();
  showcase_mark("china-catalog-synced");
  if (ui::views::CatalogView* cat = browser.catalog_view()) {
    showcase_mark("china-catalog-view");
    cat->set_source_names({"china_city"});
    showcase_mark("china-catalog-src");
    cat->set_map_docs({{"china_city", "", "China", false}});
    showcase_mark("china-catalog-docs");
  }
  showcase_mark("china-catalog-ok");
  pump_views_messages(800);
  showcase_mark("china-ready");
}

}  // namespace detail
}  // namespace app
