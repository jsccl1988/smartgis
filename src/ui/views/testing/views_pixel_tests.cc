// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

// L2 shell pixel regression for ui::views chrome (GDI canvas backend).
// Run: out\views_pixel_tests.exe
// Update goldens: out\views_pixel_tests.exe --update-goldens

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#include "ui/views/gis/ambox_view.h"
#include "ui/views/primitives/button.h"
#include "ui/views/primitives/label.h"
#include "ui/views/testing/pixel_harness.h"
#include "ui/views/gis/status_bar.h"
#include "ui/views/primitives/tab_strip.h"

namespace {

int g_fails = 0;
bool g_update_goldens = false;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

bool compare_or_update_golden(const char* name, const ui::views::PixelBuffer& actual) {
  const std::filesystem::path dir = ui::views::pixel_testdata_directory();
  const std::filesystem::path golden = dir / (std::string(name) + ".png");
  if (g_update_goldens) {
    std::filesystem::create_directories(dir);
    std::string err;
    const bool wrote = ui::views::write_png_bgra(golden, actual, &err);
    if (!wrote) {
      std::fprintf(stderr, "FAIL: write golden %s (%s)\n", name, err.c_str());
      return false;
    }
    std::printf("updated golden %s\n", golden.string().c_str());
    return true;
  }

  ui::views::PixelBuffer expected;
  std::string err;
  if (!ui::views::load_png_bgra(golden, &expected, &err)) {
    std::fprintf(stderr, "FAIL: load golden %s (%s)\n", golden.string().c_str(),
                 err.c_str());
    return false;
  }
  const ui::views::PixelCompareResult cmp =
      ui::views::compare_pixel_buffers(actual, expected);
  if (!cmp.match) {
    std::fprintf(stderr, "FAIL: pixel %s (%s, bad=%zu)\n", name,
                 cmp.detail.c_str(), cmp.bad_pixels);
    return false;
  }
  return true;
}

void test_pixel_button_and_label() {
  auto host = std::make_unique<ui::views::View>();
  auto* label = new ui::views::Label("Scale");
  auto* button = new ui::views::Button("Apply");
  label->set_bounds({8, 8, 72, 24});
  button->set_bounds({88, 6, 96, 28});
  host->add_child(std::unique_ptr<ui::views::View>(label));
  host->add_child(std::unique_ptr<ui::views::View>(button));

  const ui::views::PixelBuffer shot =
      ui::views::capture_view_tree(std::move(host), 200, 40);
  expect(shot.width == 200 && !shot.bgra.empty(), "button_label capture");
  expect(compare_or_update_golden("button_label_row", shot),
         "button_label golden");
}

void test_pixel_tab_strip_chrome() {
  auto tabs = std::make_unique<ui::views::TabStrip>();
  tabs->set_bounds({0, 0, 240, 120});
  tabs->add_tab("Map", std::make_unique<ui::views::Label>("map body"));
  tabs->add_tab("Data", std::make_unique<ui::views::Label>("data body"));
  tabs->set_active(1);
  tabs->layout();

  const ui::views::PixelBuffer shot =
      ui::views::capture_view_tree(std::move(tabs), 240, 120);
  expect(shot.width == 240 && !shot.bgra.empty(), "tab_strip capture");
  expect(compare_or_update_golden("tab_strip_two_tabs", shot),
         "tab_strip golden");
}

void test_pixel_status_bar() {
  auto bar = std::make_unique<ui::views::StatusBar>();
  bar->set_bounds({0, 0, 520, 24});
  bar->set_scale_text("1:5000");
  bar->set_crs_text("EPSG:4326");
  bar->set_coord_text("X: 1.000  Y: 2.000");
  bar->set_status("Ready");
  bar->layout();

  const ui::views::PixelBuffer shot =
      ui::views::capture_view_tree(std::move(bar), 520, 24);
  expect(shot.width == 520 && !shot.bgra.empty(), "status_bar capture");
  expect(compare_or_update_golden("status_bar_fields", shot),
         "status_bar golden");
}

void test_pixel_ambox_strip() {
  auto box = std::make_unique<ui::views::AmboxView>();
  box->set_bounds({0, 0, 180, 120});
  box->layout();

  const ui::views::PixelBuffer shot =
      ui::views::capture_view_tree(std::move(box), 180, 120);
  expect(shot.width == 180 && !shot.bgra.empty(), "ambox capture");
  expect(compare_or_update_golden("ambox_default_strip", shot), "ambox golden");
}

}  // namespace

int main(int argc, char** argv) {
  for (int i = 1; i < argc; ++i) {
    if (std::strcmp(argv[i], "--update-goldens") == 0) {
      g_update_goldens = true;
    }
  }

  test_pixel_button_and_label();
  test_pixel_tab_strip_chrome();
  test_pixel_status_bar();
  test_pixel_ambox_strip();

  if (g_fails) {
    std::fprintf(stderr, "views_pixel_tests: %d failed\n", g_fails);
    return 1;
  }
  std::printf("views_pixel_tests: ok\n");
  return 0;
}
