// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/plugin/capture/map2d_export.h"

#include <windows.h>

#include <algorithm>
#include <string>

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/common/pump/pump.h"
#include "app/views/shell/util/exe_sidecar_path.h"
#include "content/browser/camera/view_frame.h"
#include "content/browser/document/map_scene.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "ui/views/map/map_viewport.h"

namespace app {
namespace detail {
namespace {

constexpr int kOrthogridExportW = 640;
constexpr int kOrthogridExportH = 480;

}  // namespace

bool try_export_map2d_bmp(Browser& browser, const char* leaf_utf8,
                          const content::Extent2* extent_or_null) {
  content::Map2dPresenter* map2d = browser.map2d();
  content::ViewFrame* vf = browser.view_frame();
  if (!map2d || !vf || !leaf_utf8 || !leaf_utf8[0]) {
    return false;
  }
  if (extent_or_null) {
    vf->apply_world_extent(*extent_or_null, kOrthogridExportW,
                           kOrthogridExportH);
  } else {
    double minx = 0.0;
    double miny = 0.0;
    double maxx = 0.0;
    double maxy = 0.0;
    if (browser.document() &&
        browser.document()->compute_extent(&minx, &miny, &maxx, &maxy) &&
        maxx > minx && maxy > miny) {
      // MapScene stores map_y = -geo_y. apply_world_extent expects geo-space
      // Extent2 and flips Y once; un-negate so framing matches stored verts.
      const double geo_miny = -maxy;
      const double geo_maxy = -miny;
      const double pad_x = std::max(0.05, (maxx - minx) * 0.15);
      const double pad_y = std::max(0.05, (geo_maxy - geo_miny) * 0.15);
      const content::Extent2 live{minx - pad_x, geo_miny - pad_y, maxx + pad_x,
                                  geo_maxy + pad_y};
      vf->apply_world_extent(live, kOrthogridExportW, kOrthogridExportH);
    } else {
      constexpr content::Extent2 kUnit{0.0, 0.0, 1.0, 1.0};
      vf->apply_world_extent(kUnit, kOrthogridExportW, kOrthogridExportH);
    }
  }
  const int wn = MultiByteToWideChar(CP_UTF8, 0, leaf_utf8, -1, nullptr, 0);
  if (wn <= 1) {
    return false;
  }
  std::wstring leaf_w(static_cast<size_t>(wn), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, leaf_utf8, -1, leaf_w.data(), wn);
  leaf_w.resize(static_cast<size_t>(wn - 1));
  wchar_t bmp_w[MAX_PATH] = {};
  if (!exe_capture_path(bmp_w, MAX_PATH, leaf_w.c_str())) {
    return false;
  }
  char bmp_a[MAX_PATH] = {};
  if (WideCharToMultiByte(CP_ACP, 0, bmp_w, -1, bmp_a, MAX_PATH, nullptr,
                          nullptr) <= 0) {
    return false;
  }
  DeleteFileW(bmp_w);
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
      UpdateWindow(pane->native_view());
    }
    pane->invalidate_native();
    pane->sync_identity_frame();
  }
  map2d->invalidate_frame_cache();
  pump_messages(200);
  return map2d->export_bmp(bmp_a, kOrthogridExportW, kOrthogridExportH);
}

}  // namespace detail
}  // namespace app
