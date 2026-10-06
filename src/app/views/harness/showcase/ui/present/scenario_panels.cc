// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/harness/showcase/ui/present/scenario_panels.h"

#include "app/views/browser/browser.h"
#include "app/views/browser/ui_delegate.h"
#include "app/views/browser/china_product_defaults.h"
#include "app/views/harness/common/mark/mark.h"
#include "app/views/harness/self_test/self_test.h"
#include "app/views/harness/showcase/ui/interact/interact_script.h"
#include "ui/gis/catalog/catalog_view.h"
#include "ui/views/map/viewport/draw_host.h"
#include "ui/views/primitives/collection/tab_strip.h"

namespace app {
namespace detail {
namespace {

void showcase_mark(const char* token) {
  write_mark(kUiShowcaseMarkLeaf, token, /*truncate=*/false);
}

}  // namespace

void apply_ui_scenario_panels(Browser& browser, UiShowcaseMode mode) {
  switch (mode) {
    case UiShowcaseMode::kData:
      browser.select_map_tab(0);
      pump_views_messages(350);
      break;
    case UiShowcaseMode::kScene: {
      browser.select_map_tab(1);
      pump_views_messages(800);
      // Lazy FlyCube attach needs several present ticks before HUD leaves
      // views-scene3d.gdi / Fps0 and the DEM fills the tab (not a sticker).
      if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
        for (int i = 0; i < 160; ++i) {
          if (scene->attach_mode() ==
                  ui::views::DrawHost::AttachMode::kGpuPresent &&
              scene->last_gpu_present_ok()) {
            break;
          }
          scene->sync_native_bounds();
          if (scene->attach_mode() ==
              ui::views::DrawHost::AttachMode::kNone) {
            scene->attach();
          }
          scene->invalidate_native();
          pump_views_messages(50);
        }
        if (scene->attach_mode() ==
                ui::views::DrawHost::AttachMode::kGpuPresent &&
            scene->last_gpu_present_ok()) {
          showcase_mark("scene-flycube-ok");
          apply_china_scene3d_product_defaults(browser);
          for (int j = 0; j < 16; ++j) {
            scene->request_frame();
            pump_views_messages(50);
          }
        } else {
          showcase_mark("scene-flycube-wait");
        }
      }
      apply_china_scene3d_product_defaults(browser);
      if (ui::views::DrawHost* scene = browser.scene_draw_host()) {
        scene->invalidate_native();
      }
      pump_views_messages(400);
      break;
    }
    case UiShowcaseMode::kCatalog:
      browser.select_map_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          tabs->set_active(2);  // Maps
        }
      }
      pump_views_messages(350);
      break;
    case UiShowcaseMode::kInteract:
      // Prefer Interact DSL (suite-colocated *.il under testing/tools/harness/);
      // OS driver waits for outer SendInput/PostMessage injector.
      if (try_apply_interact_script(browser)) {
        // Re-assert Map tab horizon after scripted clicks / OS inject.
        browser.select_map_tab(0);
        pump_views_messages(200);
        break;
      }
      browser.select_map_tab(0);
      pump_views_messages(200);
      browser.select_map_tab(1);
      pump_views_messages(250);
      browser.select_map_tab(0);
      if (ui::views::CatalogView* cat = browser.catalog_view()) {
        if (ui::views::TabStrip* tabs = cat->source_tabs()) {
          tabs->set_active(0);
          pump_views_messages(120);
          tabs->set_active(1);
          pump_views_messages(120);
          tabs->set_active(0);
        }
      }
      if (browser.ui()) {
        if (ui::views::TabStrip* insp = browser.ui()->inspector_tabs()) {
          if (insp->tab_count() > 1) {
            insp->set_active(1);
            pump_views_messages(150);
            insp->set_active(0);
          }
        }
      }
      pump_views_messages(250);
      break;
    case UiShowcaseMode::kShell:
    case UiShowcaseMode::kNone:
    default:
      // Short settle only — long pumps after China seed can AV when a present
      // timer races shell Yoga remeasure (visual_review #1 residual).
      browser.select_map_tab(0);
      pump_views_messages(80);
      break;
  }
}

}  // namespace detail
}  // namespace app
