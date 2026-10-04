// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/shell/harness/showcase/map2d/present/present_run.h"

#include "app/views/shell/browser/browser.h"
#include "app/views/shell/harness/self_test/self_test.h"
#include "app/views/shell/harness/showcase/map2d/capture/capture.h"
#include "app/views/shell/harness/showcase/map2d/present/fps_bench.h"
#include "app/views/shell/harness/showcase/map2d/present/gpu_present.h"
#include "app/views/shell/harness/showcase/map2d/common/progress.h"
#include "content/browser/present/map2d/map2d_presenter.h"
#include "ui/views/map/map_viewport.h"

#include <cstdio>
#include <windows.h>

namespace app {
namespace detail {

int run_map2d_present(Browser& browser,
                      const char* mode_name,
                      int showcase_w,
                      int showcase_h) {
  Map2dCapturePaths paths;
  if (const int rc = prepare_map2d_capture_paths(mode_name, &paths)) {
    return rc;
  }

  content::Map2dPresenter* map2d = browser.map2d();
  if (!map2d) {
    std::fprintf(stderr, "map2d-showcase: Map2dPresenter missing\n");
    return 57;
  }
  // frame_china_map2d / orthogrid already invalidated when size/extent changed.
  // Do not invalidate again immediately before timed present â€?that forces a
  // cold layout+upload into the present_gpu wall clock.
  map2d_showcase_mark("cache-ready");

  // Capture uses software export_bmp â€?do NOT UpdateWindow here. Sync GDI
  // paint through the HWND has AVd in Map2dSoftwarePainter / ContentMapView
  // under parallel harness (mark stops at bmp-path). Async InvalidateRect is
  // enough so the live HWND may refresh; BMP does not depend on it.
  if (ui::views::MapViewport* pane = browser.map_viewport()) {
    if (pane->native_view() && IsWindow(pane->native_view())) {
      InvalidateRect(pane->native_view(), nullptr, FALSE);
    }
  }
  pump_views_messages(50);
  map2d_showcase_mark("overlay-paint");

  // Warm layout+hillshade AFTER HWND pump: a live paint at client size would
  // otherwise rebuild MapFrame at ~2k and clobber the showcase 1280x720 cache.
  // Scenic GDI SoT does not use MapFrame — skip ensure_full (china layout can
  // AV / hang on the leftover MapFrame path while scenic is hosted).
  if (!map2d->hosts_scenic_present()) {
    if (!map2d->frame_cache().ensure_full(static_cast<uint32_t>(showcase_w),
                                          static_cast<uint32_t>(showcase_h))) {
      std::fprintf(stderr, "map2d-showcase: ensure_full layout failed\n");
      return 57;
    }
    map2d_showcase_mark("layout-warm");
  } else {
    map2d_showcase_mark("layout-warm-scenic-skip");
  }

  // Software BMP first â€?carto gates / review-prep must not depend on optional
  // FlyCube smoke. Prior order (GPU then export) left bmp_missing when
  // present_gpu AVd on a second DXGI chain (ContentMapView HWND).
  if (const int rc =
          export_map2d_showcase_bmp(map2d, paths, showcase_w, showcase_h)) {
    return rc;
  }
  if (const int rc = verify_map2d_showcase_bmp(mode_name, paths)) {
    return rc;
  }

  // Optional FlyCube present smoke after BMP (src/render RHI 2D). Capture
  // already landed above; accept_nonzero_rc_if_bmp covers a late GPU fail.
  // Reuse one Device across cold + warm samples; report both separately.
  run_optional_map2d_gpu_present(browser, map2d, showcase_w, showcase_h);

  // Optional FPS bench: keep maps live, request presents, sample HUD FPS.
  // SMT_MAP2D_FPS_BENCH_MS=3000 (default off). Writes map2d-fps-bench.txt.
  run_optional_map2d_fps_bench(browser, map2d);
  return 0;
}

}  // namespace detail
}  // namespace app
