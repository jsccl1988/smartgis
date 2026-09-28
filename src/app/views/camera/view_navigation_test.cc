// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "app/views/camera/view_navigation.h"

#include <cstdio>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

content::Extent2 box(double x) {
  return content::Extent2{x, 0.0, x + 1.0, 1.0};
}

}  // namespace

int run_view_frame_tests();

int main() {
  {
    app::ViewNavigation nav;
    nav.reset(box(0));
    expect(nav.commit(box(1)), "commit pushes previous");
    expect(nav.commit(box(2)), "second commit");
    expect(app::extents_equal(nav.extent(), box(2)), "current is newest");
    expect(nav.previous(), "previous");
    expect(app::extents_equal(nav.extent(), box(1)), "previous restores");
    expect(nav.previous(), "previous again");
    expect(app::extents_equal(nav.extent(), box(0)), "back at start");
    expect(!nav.previous(), "no previous");
    expect(nav.status() == "No previous extent", "no previous status");
    expect(app::extents_equal(nav.extent(), box(0)), "failed previous stays");
    expect(nav.next(), "next");
    expect(app::extents_equal(nav.extent(), box(1)), "next restores");
    expect(nav.next(), "next again");
    expect(!nav.next(), "no next");
    expect(nav.status() == "No next extent", "no next status");
    expect(app::extents_equal(nav.extent(), box(2)), "failed next stays");
    const size_t size_before = nav.history_size();
    expect(nav.previous(), "step back before branch");
    expect(nav.commit(box(9)), "new nav truncates forward");
    expect(!nav.next(), "forward dropped");
    expect(nav.status() == "No next extent", "truncated next status");
    expect(nav.history_size() == size_before, "truncated size");
    expect(app::extents_equal(nav.extent(), box(9)), "branched extent");
  }

  {
    app::ViewNavigation nav;
    nav.reset(box(0));
    for (int i = 1; i <= 40; ++i) {
      expect(nav.commit(box(static_cast<double>(i))), "cap commit");
    }
    expect(nav.history_size() == app::ViewNavigation::kExtentCap, "cap 32");
    expect(app::extents_equal(nav.extent(), box(40)), "newest kept");
    int steps = 0;
    while (nav.previous()) {
      ++steps;
    }
    expect(steps == 31, "31 previous steps inside the cap");
    expect(nav.status() == "No previous extent", "oldest dropped");
    expect(nav.extent().xmin == 9.0, "oldest remaining is extent 9");
  }

  {
    app::ViewNavigation nav;
    nav.reset(box(3));
    const size_t before = nav.history_size();
    expect(!nav.zoom_to(nullptr, "No active layer"), "empty layer");
    expect(nav.status() == "No active layer", "no active layer");
    expect(app::extents_equal(nav.extent(), box(3)), "layer miss keeps extent");
    expect(nav.history_size() == before, "layer miss does not push");
    expect(!nav.zoom_to(nullptr, "No selection"), "empty selection");
    expect(nav.status() == "No selection", "no selection");
    expect(app::extents_equal(nav.extent(), box(3)), "selection miss keeps extent");
    expect(nav.history_size() == before, "selection miss does not push");
    content::Extent2 empty{1, 1, 1, 2};
    expect(!nav.zoom_to(&empty, "No selection"), "degenerate selection");
    expect(nav.history_size() == before, "degenerate does not push");
  }

  {
    app::ViewNavigation nav;
    nav.reset(box(4));
    nav.add_bookmark();
    nav.add_bookmark();
    expect(nav.bookmarks().size() == 2, "two bookmarks");
    expect(nav.bookmarks()[0].label == "Bookmark 1", "default bookmark 1");
    expect(nav.bookmarks()[1].label == "Bookmark 2", "default bookmark 2");
    expect(nav.commit(box(8)), "leave bookmark");
    expect(nav.go_bookmark(0), "go bookmark");
    expect(app::extents_equal(nav.extent(), box(4)), "bookmark restored");
    expect(nav.previous(), "bookmark jump pushed");
    expect(app::extents_equal(nav.extent(), box(8)), "pre-jump extent");
  }

  {
    app::ViewNavigation nav;
    nav.reset(box(1));
    nav.add_bookmark("Home");
    nav.add_bookmark("Home");
    nav.add_bookmark("Home");
    expect(nav.bookmarks()[0].label == "Home", "first label kept");
    expect(nav.bookmarks()[1].label == "Home (2)", "collision (2)");
    expect(nav.bookmarks()[2].label == "Home (3)", "collision (3)");
    expect(nav.bookmarks().size() == 3, "older bookmark kept");
  }

  {
    app::ViewNavigation nav;
    // 254 m across 96 px → 1:10000. xmax > 180 so the box is projected meters.
    nav.reset(content::Extent2{0, 0, 254, 100});
    expect(nav.scale_text(96) == "1:10000", "meter scale");
    expect(nav.scale_text(0) == "1:—", "width 0");
    nav.reset(content::Extent2{200, 0, 200.00001, 10});
    expect(nav.scale_text(10000) == "1:1", "sub-1 scale clamps");
    // 1 degree at latitude 30, 96 px.
    nav.reset(content::Extent2{100, 29, 101, 31});
    expect(nav.scale_text(96) == "1:3795510", "geographic scale");
    nav.reset(content::Extent2{0, -1, 254.0 / 111320.0, 1});
    expect(nav.scale_text(96) == "1:10000", "equator scale");
  }

  g_fails += run_view_frame_tests();
  if (g_fails) {
    std::fprintf(stderr, "%d view_navigation_test fail(s)\n", g_fails);
    return 1;
  }
  std::printf("view_navigation_test ok\n");
  return 0;
}
