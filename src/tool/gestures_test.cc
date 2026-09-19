// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/events.h"
#include "content/public/map_types.h"
#include "gis/edit/edit_session.h"
#include "tool/gestures.h"
#include "tool/workspace.h"

#include <cstdio>
#include <cstring>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

content::InputEvent make_event(content::InputEvent::Kind kind) {
  content::InputEvent e{};
  e.kind = kind;
  return e;
}

content::InputEvent at(content::InputEvent::Kind kind, int x, int y) {
  content::InputEvent e = make_event(kind);
  e.x_px = x;
  e.y_px = y;
  return e;
}

}  // namespace

int main() {
  namespace df = tool::draft_flags;

  {
    const uint32_t f = df::pack(df::kFamilyPoint, /*PT_Text*/ 2);
    expect(df::family_of(f) == df::kFamilyPoint, "family point");
    expect(df::code_of(f) == 2, "code PT_Text");
    const uint32_t circle =
        df::pack(df::kFamilySelect, df::kSelectCircleCode);
    expect(df::is_select_circle(circle), "circle predicate");
    expect(!df::is_select_circle(f), "point not circle");
  }

  {
    content::EventBus bus;
    int extents = 0;
    auto sub = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    tool::Workspace ws(&bus, nullptr);
    expect(ws.execute("view.pan", {}), "activate pan");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 1, 1)),
           "pan down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 20, 15)),
           "pan move");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "pan mid-drag draft");
    expect(ws.last_draft().points.size() == 2, "pan mid two corners");
    expect(ws.last_draft().points[0].x_px == 1 &&
               ws.last_draft().points[1].x_px == 20,
           "pan continuous horizontal delta");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 20, 15)),
           "pan up");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "pan draft rect");
    expect(ws.last_draft().points.size() == 2, "pan two corners");
    expect(extents >= 1, "pan ExtentChanged");
  }

  {
    // Trackpad / mouse horizontal wheel → touch-pan draft (always-on).
    content::EventBus bus;
    int drafts = 0;
    tool::Workspace ws(&bus, nullptr);
    ws.set_draft_observer([&](const tool::Draft& d) {
      ++drafts;
      expect(d.kind == tool::DraftKind::kRect, "hwheel draft rect");
      expect(tool::draft_flags::is_touch_pan(d.flags), "hwheel touch pan");
      expect(d.points.size() == 2, "hwheel two pts");
      expect(d.points[1].x_px - d.points[0].x_px == 120, "hwheel dx");
    });
    expect(ws.execute("view.pan", {}), "activate pan for hwheel");
    content::InputEvent hw = at(content::InputEvent::Kind::kWheel, 50, 60);
    hw.wheel = 120;
    hw.flags = content::input_flags::kHorizontalWheel;
    expect(ws.dispatch_input(hw), "hwheel consumed");
    expect(drafts == 1, "hwheel one draft");
  }

  {
    // Two-finger pan: midpoint sequence with pointer_count >= 2.
    content::EventBus bus;
    int extents = 0;
    int drafts = 0;
    auto sub = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    tool::Workspace ws(&bus, nullptr);
    ws.set_draft_observer([&](const tool::Draft& d) {
      ++drafts;
      expect(tool::draft_flags::is_touch_pan(d.flags), "2d touch pan flag");
    });
    expect(ws.execute("view.pan", {}), "activate pan for touch");
    content::InputEvent down = at(content::InputEvent::Kind::kLDown, 10, 20);
    down.pointer_count = 2;
    expect(ws.dispatch_input(down), "2d two-finger down");
    content::InputEvent move = at(content::InputEvent::Kind::kMouseMove, 40, 22);
    move.pointer_count = 2;
    expect(ws.dispatch_input(move), "2d two-finger move");
    expect(drafts >= 1, "2d two-finger mid-drag draft");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "2d touch rect");
    expect(ws.last_draft().points.size() == 2, "2d touch two pts");
    expect(ws.last_draft().points[0].x_px == 10 &&
               ws.last_draft().points[1].x_px == 40,
           "2d touch horizontal delta");
    content::InputEvent up = at(content::InputEvent::Kind::kLUp, 50, 24);
    up.pointer_count = 2;
    expect(ws.dispatch_input(up), "2d two-finger up");
    expect(extents >= 1, "2d touch ExtentChanged");
  }

  {
    // view3d.trackball: two-finger pans (kTouchPan), single-finger still orbits.
    content::EventBus bus;
    int extents = 0;
    auto sub = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    tool::Workspace ws(&bus, nullptr);
    expect(ws.execute("view3d.trackball", {}), "3d for touch pan");
    content::InputEvent down = at(content::InputEvent::Kind::kLDown, 5, 5);
    down.pointer_count = 2;
    expect(ws.dispatch_input(down), "3d two-finger down");
    content::InputEvent move = at(content::InputEvent::Kind::kMouseMove, 25, 8);
    move.pointer_count = 2;
    expect(ws.dispatch_input(move), "3d two-finger move");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "3d touch rect");
    expect(tool::draft_flags::is_touch_pan(ws.last_draft().flags),
           "3d touch pan flag");
    content::InputEvent up = at(content::InputEvent::Kind::kLUp, 25, 8);
    up.pointer_count = 2;
    expect(ws.dispatch_input(up), "3d two-finger up");
    expect(extents >= 1, "3d touch ExtentChanged");
  }

  {
    content::EventBus bus;
    int sels = 0;
    auto sub = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged&) { ++sels; });
    tool::Workspace ws(&bus, nullptr);
    expect(ws.execute("selection.rect", {}), "activate select.rect");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 2, 3)),
           "sel down");
    ws.aux_draw();
    expect(ws.live_preview() == nullptr, "no preview until move");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 12, 13)),
           "sel move");
    ws.aux_draw();
    const tool::AuxOverlay* o = ws.live_preview();
    expect(o && o->kind == tool::AuxOverlay::Kind::kRect, "live rect preview");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 12, 13)),
           "sel up");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "sel draft");
    expect(sels == 1, "SelectionChanged");
  }

  {
    tool::Workspace ws(nullptr, nullptr);
    ws.set_draft_flags(df::pack(df::kFamilySelect, df::kSelectCircleCode));
    expect(ws.execute("selection.rect", {}), "activate rect for circle flags");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 0, 0)),
           "circle down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 10, 0)),
           "circle up");
    expect(df::is_select_circle(ws.last_draft().flags),
           "pending flags stamped");

    expect(ws.activate("select.circle"), "activate select.circle");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "select.circle") == 0,
           "circle interaction id");
    ws.set_draft_flags(0);
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 1, 1)),
           "builtin circle down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 5, 5)),
           "builtin circle up");
    expect(df::is_select_circle(ws.last_draft().flags),
           "interaction default flags");
  }

  {
    content::EventBus bus;
    int commits = 0;
    auto sub = bus.subscribe<content::EditCommitted>(
        [&](const content::EditCommitted&) { ++commits; });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.execute("edit.append.point", {}), "activate draw.point");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 4, 5)),
           "draw point down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 4, 5)),
           "draw point up swallowed");
    expect(ws.last_draft().kind == tool::DraftKind::kPoint, "point draft");
    expect(commits == 1, "EditCommitted once");
    expect(edits.can_undo(), "can undo");

    // Second click still works (no permanent capture).
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 7, 8)),
           "second point");
    expect(commits == 2, "second EditCommitted");
  }

  {
    gis::MemoryEditSession edits;
    tool::Workspace ws(nullptr, &edits);
    expect(ws.execute("edit.append.linestring", {}), "activate line");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 0, 0)),
           "line v0");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 3, 4)),
           "line v1");
    content::InputEvent esc = make_event(content::InputEvent::Kind::kKeyDown);
    esc.key = 0x1B;
    expect(ws.dispatch_input(esc), "esc cancel");
    expect(edits.committed().empty(), "esc no commit");
    ws.aux_draw();
    expect(ws.live_preview() == nullptr, "cancel clears overlay");

    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 1, 1)),
           "line again v0");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 2, 2)),
           "line again v1");
    expect(ws.dispatch_input(make_event(content::InputEvent::Kind::kRDown)),
           "line finish");
    expect(ws.last_draft().kind == tool::DraftKind::kLineString, "line draft");
    expect(edits.committed().size() == 1, "line commit");
  }

  {
    tool::Workspace ws(nullptr, nullptr);
    expect(!ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 1, 1)),
           "hover does not swallow");
    expect(ws.execute("selection.rect", {}), "rect for capture swallow");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 0, 0)),
           "capture down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 9, 9)),
           "captured move swallowed");
  }

  {
    // Vertical slice chain: pan → select → append on one workspace.
    content::EventBus bus;
    int extents = 0;
    int sels = 0;
    int commits = 0;
    auto se = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    auto ss = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged&) { ++sels; });
    auto sc = bus.subscribe<content::EditCommitted>(
        [&](const content::EditCommitted&) { ++commits; });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);

    expect(ws.execute("view.pan", {}), "chain pan");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 0, 0)),
           "chain pan down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 5, 5)),
           "chain pan up");
    expect(extents >= 1, "chain extent");

    expect(ws.execute("selection.rect", {}), "chain select");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 1, 1)),
           "chain sel down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 8, 8)),
           "chain sel up");
    expect(sels >= 1, "chain selection");

    expect(ws.execute("edit.append.point", {}), "chain append");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 3, 3)),
           "chain append");
    expect(commits >= 1 && edits.can_undo(), "chain commit");
  }

  {
    // view3d key / wheel / click-pick (same sequence as dispatch_test).
    content::EventBus bus;
    int extents = 0;
    int sels = 0;
    auto se = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    auto ss = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged&) { ++sels; });
    tool::Workspace ws(&bus, nullptr);
    expect(ws.execute("view3d.trackball", {}), "activate trackball keys");
    content::InputEvent key = make_event(content::InputEvent::Kind::kKeyDown);
    key.key = 'W';
    expect(ws.dispatch_input(key), "3d key consumed");
    expect(ws.last_draft().kind == tool::DraftKind::kKey, "3d key draft");
    expect(ws.last_draft().key == 'W', "3d key code");
    expect(extents >= 1, "3d key published extent");
  }

  {
    // view3d.* pointer / wheel / key — aligns with leftover apply_draft shell.
    content::EventBus bus;
    int extents = 0;
    int drafts = 0;
    auto se = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    tool::Workspace ws(&bus, nullptr);
    ws.set_draft_observer([&](const tool::Draft&) { ++drafts; });

    expect(ws.execute("view3d.trackball", {}), "3d trackball");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLDown, 2, 2)),
           "3d down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 12, 8)),
           "3d move");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "3d drag draft");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kLUp, 12, 8)),
           "3d up");
    expect(extents >= 1 && drafts >= 1, "3d observer + extent");

    content::InputEvent mdown = at(content::InputEvent::Kind::kRDown, 4, 4);
    mdown.flags = 0x0010;  // MK_MBUTTON
    expect(ws.dispatch_input(mdown), "3d orbit down");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 9, 9)),
           "3d orbit move");
    expect((ws.last_draft().flags & 0x0010) != 0 ||
               (ws.last_draft().flags & 0x0002) != 0,
           "3d orbit flags");

    expect(ws.execute("view3d.fps", {}), "3d fps");
    expect(ws.dispatch_input(at(content::InputEvent::Kind::kMouseMove, 1, 1)),
           "fps look");
    expect(ws.last_draft().kind == tool::DraftKind::kPoint, "fps look draft");

    content::InputEvent wheel = make_event(content::InputEvent::Kind::kWheel);
    wheel.wheel = 120;
    wheel.x_px = 3;
    wheel.y_px = 3;
    expect(ws.dispatch_input(wheel), "3d wheel");
    expect(ws.last_draft().kind == tool::DraftKind::kWheel, "3d wheel draft");
  }

  {
    // Matrix: draw.* covers Input* digitize gestures on bound Append path.
    // Input* keep GTT_* factory ABI + apply_draft only (no pointer SM).
    tool::Workspace ws(nullptr, nullptr);
    expect(ws.catalog().contains("edit.append.point"), "draw.point cmd");
    expect(ws.catalog().contains("edit.append.linestring"), "draw.line cmd");
    expect(ws.catalog().contains("edit.append.polygon"), "draw.poly cmd");
    expect(ws.execute("edit.append.point", {}), "draw.point vs InputPoint");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "draw.point") == 0,
           "InputPoint → draw.point");
    expect(ws.execute("edit.append.linestring", {}),
           "draw.line vs InputLine");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "draw.linestring") == 0,
           "InputLine → draw.linestring");
    expect(ws.execute("edit.append.polygon", {}), "draw.poly vs InputRegion");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "draw.polygon") == 0,
           "InputRegion → draw.polygon");
  }

  if (g_fails) {
    std::fprintf(stderr, "gestures_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("gestures_test: ok\n");
  return 0;
}
