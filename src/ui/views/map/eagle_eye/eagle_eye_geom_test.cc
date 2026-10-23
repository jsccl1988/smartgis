// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "ui/views/map/eagle_eye/eagle_eye_geom.h"

#include <cstdio>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

}  // namespace

int main() {
  using ui::views::detail::EagleEyeHit;
  using ui::views::detail::EagleEyeRectPx;
  using ui::views::detail::eagle_eye_center_view_on;
  using ui::views::detail::eagle_eye_frame_on_parent;
  using ui::views::detail::eagle_eye_hit_test;
  using ui::views::detail::eagle_eye_view_rect_px;
  using ui::views::detail::eagle_eye_world_to_px;

  const EagleEyeRectPx frame = eagle_eye_frame_on_parent(1280, 720);
  expect(frame.w >= 120 && frame.h >= 90, "frame sized");
  expect(frame.x + frame.w <= 1280 - 12, "frame right margin");
  expect(frame.y + frame.h <= 720 - 12, "frame bottom margin");

  const content::Extent2 world{0, 0, 100, 50};
  const content::Extent2 view{25, 10, 75, 40};
  const EagleEyeRectPx content{10, 20, 200, 100};
  const EagleEyeRectPx view_px = eagle_eye_view_rect_px(world, view, content);
  expect(view_px.w > 0 && view_px.h > 0, "view rect nonempty");

  double px = 0;
  double py = 0;
  expect(eagle_eye_world_to_px(world, 50, 25, content, &px, &py), "world_to_px");
  expect(px > content.x && px < content.x + content.w, "px in content x");
  expect(py > content.y && py < content.y + content.h, "px in content y");

  const content::Extent2 centered =
      eagle_eye_center_view_on(world, view, 50, 25);
  expect(centered.xmin < 50 && centered.xmax > 50, "centered contains mx");
  expect(centered.ymin < 25 && centered.ymax > 25, "centered contains my");

  EagleEyeRectPx outer{0, 0, 220, 140};
  expect(eagle_eye_hit_test(view_px.x, view_px.y, outer, view_px) ==
             EagleEyeHit::kHandleNW,
         "nw handle");
  expect(eagle_eye_hit_test(view_px.x + view_px.w / 2,
                            view_px.y + view_px.h / 2, outer, view_px) ==
             EagleEyeHit::kBox,
         "box interior");
  expect(eagle_eye_hit_test(1, 1, outer, view_px) == EagleEyeHit::kFrame,
         "frame chrome");

  if (g_fails) {
    std::fprintf(stderr, "%d eagle_eye_geom_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("eagle_eye_geom_test ok\n");
  return 0;
}
