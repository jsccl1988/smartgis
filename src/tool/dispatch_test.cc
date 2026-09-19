// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/events.h"
#include "plugin/host/legacy_cmd.h"
#include "gis/edit/edit_session.h"
#include "tool/command.h"
#include "tool/interaction.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

content::FeatureId make_id(uint8_t v) {
  content::FeatureId id{};
  id.len = 1;
  id.bytes[0] = v;
  return id;
}

content::InputEvent make_event(content::InputEvent::Kind kind) {
  content::InputEvent e{};
  e.kind = kind;
  return e;
}

class CountingTool final : public tool::Interaction {
 public:
  explicit CountingTool(const char* id) : id_(id) {}
  const char* id() const override { return id_; }
  void activate() override { ++activates; }
  void deactivate() override { ++deactivates; }
  bool on_input(const content::InputEvent& e) override {
    ++inputs;
    last = e.kind;
    return e.kind == content::InputEvent::Kind::kLDown;
  }

  int activates = 0;
  int deactivates = 0;
  int inputs = 0;
  content::InputEvent::Kind last = content::InputEvent::Kind::kMouseMove;

 private:
  const char* id_;
};

}  // namespace

int main() {
  {
    content::EventBus bus;
    content::EventBus::Connection a;
    content::EventBus::Connection b;
    int n = 0;
    a = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged&) {
          ++n;
          a.disconnect();
        });
    b = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged&) { ++n; });
    bus.publish(content::SelectionChanged{});
    expect(n == 2, "unsubscribe mid-publish still snapshots both");
    bus.publish(content::SelectionChanged{});
    expect(n == 3, "disconnected slot gone");

    int sel = 0;
    int ext = 0;
    auto cs = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged&) { ++sel; });
    auto ce = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++ext; });
    bus.publish(content::SelectionChanged{});
    expect(sel == 1 && ext == 0, "typed isolation");
    bus.publish(content::ExtentChanged{});
    expect(sel == 1 && ext == 1, "extent slot");

    content::EventBus::Connection empty =
        bus.subscribe<content::SelectionChanged>({});
    expect(!empty, "empty function ignored");
  }

  {
    tool::CommandCatalog catalog;
    expect(!catalog.add("", [](const tool::CommandArgs&) { return true; }),
           "empty command id");
    expect(catalog.add("selection.clear",
                       [](const tool::CommandArgs&) { return true; }),
           "add clear");
    expect(!catalog.add("selection.clear",
                        [](const tool::CommandArgs&) { return true; }),
           "duplicate add");
    tool::CommandDispatcher dispatcher(&catalog);
    expect(!dispatcher.execute("nope", {}), "unknown id");
    expect(dispatcher.execute("selection.clear", {}), "known id");

    expect(catalog.add("view.pan",
                       [](const tool::CommandArgs&) { return true; }),
           "add pan");
    std::vector<std::string> ids;
    catalog.for_each([&](std::string_view id) { ids.emplace_back(id); });
    expect(ids.size() == 2, "for_each count");
    expect(ids[0] == "selection.clear" && ids[1] == "view.pan",
           "for_each map order");
    catalog.for_each(std::function<void(std::string_view)>{});
    expect(true, "for_each empty fn no-op");
  }

  {
    tool::InteractionRegistry registry;
    CountingTool* first = nullptr;
    CountingTool* nested = nullptr;
    registry.add("select.rect", [&]() {
      auto t = std::make_unique<CountingTool>("select.rect");
      first = t.get();
      return t;
    });
    registry.add("draw.polygon", [&]() {
      auto t = std::make_unique<CountingTool>("draw.polygon");
      nested = t.get();
      return t;
    });

    tool::InteractionStack stack;
    expect(!stack.pop(), "pop empty");
    expect(stack.activate("select.rect", registry), "activate rect");
    expect(stack.current() && std::strcmp(stack.current()->id(), "select.rect") == 0,
           "current rect");
    expect(first && first->activates == 1, "activate called");
    expect(stack.push("draw.polygon", registry), "push draw");
    expect(stack.current() &&
               std::strcmp(stack.current()->id(), "draw.polygon") == 0,
           "current draw");
    expect(first && first->deactivates == 1, "rect deactivated on push");
    expect(stack.pop(), "pop draw");
    expect(stack.current() &&
               std::strcmp(stack.current()->id(), "select.rect") == 0,
           "restored rect");
    expect(first && first->activates == 2, "rect reactivated");

    expect(!stack.activate("missing", registry), "unknown activate");
    expect(stack.current() &&
               std::strcmp(stack.current()->id(), "select.rect") == 0,
           "current unchanged on failed activate");
    (void)nested;
  }

  {
    tool::InteractionRegistry registry;
    CountingTool* tool_ptr = nullptr;
    registry.add("select.rect", [&]() {
      auto t = std::make_unique<CountingTool>("select.rect");
      tool_ptr = t.get();
      return t;
    });
    tool::InteractionStack stack;
    stack.activate("select.rect", registry);
    tool::InputRouter router;
    router.add_always_on(tool::make_wheel_zoom());
    router.add_always_on(tool::make_hover_cursor());
    router.set_stack(&stack);

    expect(router.dispatch(make_event(content::InputEvent::Kind::kWheel)),
           "wheel consumed");
    expect(tool_ptr && tool_ptr->inputs == 0, "wheel not on exclusive");

    expect(!router.dispatch(make_event(content::InputEvent::Kind::kMouseMove)),
           "hover does not swallow");
    expect(tool_ptr && tool_ptr->inputs == 1, "move reached exclusive");
    expect(tool_ptr->last == content::InputEvent::Kind::kMouseMove, "kind move");

    expect(router.dispatch(make_event(content::InputEvent::Kind::kLDown)),
           "ldown handled by exclusive");
    expect(tool_ptr && tool_ptr->inputs == 2, "ldown counted");
  }

  {
    gis::MemoryEditSession edits;
    gis::FeatureMutation empty;
    expect(!edits.commit(empty), "empty id rejected");
    expect(!edits.can_undo() && !edits.can_redo(), "empty stacks");

    gis::FeatureMutation a;
    a.op = gis::EditOp::kAppend;
    a.id = make_id(1);
    expect(edits.commit(a), "commit a");
    gis::FeatureMutation b;
    b.op = gis::EditOp::kDelete;
    b.id = make_id(2);
    expect(edits.commit(b), "commit b");
    expect(edits.committed().size() == 2, "two committed");
    expect(edits.can_undo() && !edits.can_redo(), "can undo");
    expect(edits.undo(), "undo b");
    expect(edits.committed().size() == 1, "one left");
    expect(edits.can_redo(), "can redo");
    expect(edits.redo(), "redo b");
    expect(edits.committed().size() == 2, "two after redo");
    expect(edits.commit(a), "commit clears redo");
    expect(!edits.can_redo(), "redo cleared");
  }

  {
    content::EventBus bus;
    int clears = 0;
    auto sub = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged& ev) {
          ++clears;
          expect(ev.ids.empty(), "clear empty ids");
        });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.stack().current() == nullptr, "no current at start");
    expect(ws.execute("selection.clear", {}), "clear command");
    expect(ws.stack().current() == nullptr, "clear does not activate");
    expect(clears == 1, "clear published");
    expect(ws.execute("selection.rect", {}), "activate via command");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "select.rect") == 0,
           "rect current");
    expect(!ws.execute("nope", {}), "unknown command");
    expect(ws.activate("select.point"), "activate point");
    expect(std::strcmp(ws.stack().current()->id(), "select.point") == 0,
           "point replaced rect");
  }

  {
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsgViewZoomIn),
                        "view.zoom_in") == 0,
           "zoom in id");
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsgSelectClear),
                        "selection.clear") == 0,
           "clear id");
    expect(tool::command_id_from_gt_msg(-1) == nullptr, "unknown msg");
  }

  {
    content::EventBus bus;
    int sels = 0;
    auto sub = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged& ev) {
          ++sels;
          expect(ev.ids.size() == 1, "select one id");
        });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.execute("selection.rect", {}), "activate rect");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 1;
    down.y_px = 2;
    content::InputEvent up = make_event(content::InputEvent::Kind::kLUp);
    up.x_px = 10;
    up.y_px = 12;
    expect(ws.dispatch_input(down), "rect down");
    expect(ws.dispatch_input(up), "rect up");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "rect draft kind");
    expect(ws.last_draft().points.size() == 2, "rect two corners");
    expect(sels == 1, "selection published");
    expect(edits.committed().empty(), "select does not commit");
  }

  {
    gis::MemoryEditSession edits;
    tool::Workspace ws(nullptr, &edits);
    expect(ws.execute("edit.append.point", {}), "activate draw point");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 3;
    down.y_px = 4;
    expect(ws.dispatch_input(down), "draw down");
    expect(edits.committed().size() == 1, "append committed");
    expect(edits.committed()[0].op == gis::EditOp::kAppend, "append op");
    expect(ws.execute("edit.undo", {}), "edit.undo");
    expect(edits.committed().empty(), "undo cleared");
    expect(ws.execute("edit.redo", {}), "edit.redo");
    expect(edits.committed().size() == 1, "redo restored");
  }

  {
    content::EventBus bus;
    int commits = 0;
    auto sub = bus.subscribe<content::EditCommitted>(
        [&](const content::EditCommitted& ev) {
          ++commits;
          expect(ev.op == content::EditCommitted::Op::kAppend, "event append");
        });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.execute("edit.append.point", {}), "activate for EditCommitted");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 1;
    down.y_px = 1;
    expect(ws.dispatch_input(down), "draw for EditCommitted");
    expect(commits == 1, "EditCommitted published");
  }

  {
    gis::MemoryEditSession edits;
    tool::Workspace ws(nullptr, &edits);
    expect(ws.execute("edit.append.linestring", {}), "activate draw line");
    content::InputEvent a = make_event(content::InputEvent::Kind::kLDown);
    a.x_px = 0;
    a.y_px = 0;
    content::InputEvent b = make_event(content::InputEvent::Kind::kLDown);
    b.x_px = 4;
    b.y_px = 5;
    content::InputEvent finish = make_event(content::InputEvent::Kind::kRDown);
    expect(ws.dispatch_input(a), "line first");
    expect(ws.dispatch_input(b), "line second");
    expect(edits.committed().empty(), "line not committed until finish");
    expect(ws.dispatch_input(finish), "line finish");
    expect(edits.committed().size() == 1, "line committed");
    expect(ws.last_draft().kind == tool::DraftKind::kLineString, "line draft");
    expect(ws.last_draft().points.size() == 2, "line two verts");
  }

  {
    content::EventBus bus;
    int extents = 0;
    auto sub = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(!ws.flashing(), "flash off at start");
    expect(ws.execute("flash.start", {}), "flash start");
    expect(ws.flashing(), "flash on");
    expect(ws.execute("flash.stop", {}), "flash stop");
    expect(!ws.flashing(), "flash off");
    expect(ws.stack().current() == nullptr, "flash is not a tool");

    expect(ws.execute("view.pan", {}), "activate pan");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "view.pan") == 0,
           "pan current");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 2;
    down.y_px = 3;
    content::InputEvent up = make_event(content::InputEvent::Kind::kLUp);
    up.x_px = 8;
    up.y_px = 9;
    expect(ws.dispatch_input(down), "pan down");
    expect(ws.dispatch_input(up), "pan up");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "pan draft");
    expect(extents == 1, "pan published extent");
    expect(edits.committed().empty(), "pan does not commit");

    expect(ws.execute("view.full", {}), "view full");
    expect(extents == 2, "full published extent");
  }

  {
    tool::Workspace ws(nullptr, nullptr);
    expect(ws.execute("selection.rect", {}), "ws activate rect");
    expect(ws.stack().push("draw.polygon", ws.interactions()), "ws push draw");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "draw.polygon") == 0,
           "ws current draw");
    expect(ws.stack().pop(), "ws pop draw");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "select.rect") == 0,
           "ws restored rect");
  }

  {
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsgWsViewPan),
                        "view.pan") == 0,
           "ws pan id");
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsg3dViewFps),
                        "view3d.fps") == 0,
           "3d fps id");
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsgFlashStart),
                        "flash.start") == 0,
           "flash start id");
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsgAppendLineRect),
                        "edit.append.linestring") == 0,
           "append rect id");
    expect(std::strcmp(tool::command_id_from_gt_msg(tool::kGtMsgViewRefresh),
                        "view.refresh") == 0,
           "refresh id");
    const long keyed_refresh =
        tool::kGtMsgViewRefresh | (static_cast<long>(0x1234) << 16);
    const char* keyed_id = tool::command_id_from_gt_msg(keyed_refresh);
    expect(keyed_id && std::strcmp(keyed_id, "view.refresh") == 0,
           "keyed refresh id");
  }

  {
    expect(!tool::try_execute_gt_msg(nullptr, tool::kGtMsgViewPan),
           "try_execute null ws");
    tool::Workspace ws(nullptr, nullptr);
    expect(!tool::try_execute_gt_msg(&ws, -1), "try_execute unmapped");
    expect(tool::try_execute_gt_msg(&ws, tool::kGtMsgViewPan),
           "try_execute pan");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "view.pan") == 0,
           "try_execute activated pan");
    expect(tool::try_execute_gt_msg(&ws, tool::kGtMsgSelectPoint),
           "try_execute select point");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "select.point") == 0,
           "try_execute activated select.point");
    expect(tool::try_execute_gt_msg(&ws, tool::kGtMsgAppendPointDot),
           "try_execute append point");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "draw.point") == 0,
           "try_execute activated draw.point");
  }

  {
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgDemLoadTin),
                       "dem.load_tin") == 0,
           "plugin dem tin");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgDemLoadGrid),
                       "dem.load_grid") == 0,
           "plugin dem grid");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgDemAbout),
                       "dem.about") == 0,
           "plugin dem about");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgProjDoPrj),
                       "proj.do_prj") == 0,
           "plugin proj");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgPrintPreview),
                       "print.preview") == 0,
           "plugin print");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgModel3dSphere),
                       "model3d.add_sphere") == 0,
           "plugin model3d");
    expect(std::strcmp(plugin::command_id_from_am_msg(plugin::kAmMsgModel3dWater),
                       "model3d.add_water") == 0,
           "plugin model3d water");
    expect(std::strcmp(plugin::command_id_from_am_msg(
                           plugin::kAmMsgModel3dCreateTin),
                       "model3d.create_tin") == 0,
           "plugin model3d tin");
    expect(std::strcmp(plugin::command_id_from_am_msg(
                           plugin::kAmMsgOrthogridInputBoundary0),
                       "baogrid.input_boundary_0") == 0,
           "plugin baogrid");
    expect(std::strcmp(plugin::command_id_from_am_msg(
                           plugin::kAmMsgOrthogridLoadBoundary),
                       "baogrid.load_boundary") == 0,
           "plugin baogrid load");
    expect(std::strcmp(plugin::command_id_from_am_msg(tool::kGtMsgAppendLineString),
                       "edit.append.linestring") == 0,
           "plugin gt append line");
    expect(plugin::command_id_from_am_msg(-1) == nullptr, "plugin unknown am");
  }

  {
#ifdef HWND
    expect(false, "HWND leaked onto public dispatch headers");
#else
    expect(true, "no HWND on public headers");
#endif
  }

  {
    content::EventBus bus;
    int extents = 0;
    auto sub = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.execute("view3d.trackball", {}), "activate trackball");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "view3d.trackball") == 0,
           "trackball current");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 4;
    down.y_px = 5;
    content::InputEvent move = make_event(content::InputEvent::Kind::kMouseMove);
    move.x_px = 14;
    move.y_px = 16;
    content::InputEvent up = make_event(content::InputEvent::Kind::kLUp);
    up.x_px = 14;
    up.y_px = 16;
    expect(ws.dispatch_input(down), "trackball down consumed");
    expect(ws.dispatch_input(move), "trackball move consumed");
    expect(ws.dispatch_input(up), "trackball up consumed");
    expect(ws.last_draft().points.size() >= 1, "trackball draft");
    expect(extents >= 1, "trackball published extent");
    expect(edits.committed().empty(), "trackball does not commit");

    expect(ws.execute("view3d.sphere", {}), "activate sphere");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "view3d.sphere") == 0,
           "sphere current");
    expect(ws.dispatch_input(down), "sphere down consumed");

    expect(ws.execute("view3d.fps", {}), "activate fps");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "view3d.fps") == 0,
           "fps current");
    expect(ws.dispatch_input(move), "fps move consumed");

    expect(ws.execute("view3d.full", {}), "view3d full");
    expect(extents >= 2, "full published extent");
  }

  {
    content::EventBus bus;
    int backends = 0;
    uint32_t last_kind = 99;
    auto sub = bus.subscribe<content::RenderBackendChanged>(
        [&](const content::RenderBackendChanged& e) {
          ++backends;
          last_kind = e.kind;
        });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.catalog().contains("view.backend.rhi"), "rhi command registered");
    expect(ws.catalog().contains("view.backend.maplibre"),
           "maplibre command registered");
    expect(ws.execute("view.backend.maplibre", {}), "execute maplibre");
    expect(backends == 1 && last_kind == 1, "maplibre event kind");
    expect(ws.execute("view.backend.rhi", {}), "execute rhi");
    expect(backends == 2 && last_kind == 0, "rhi event kind");
  }

  {
    content::EventBus bus;
    int extents = 0;
    int sels = 0;
    auto se = bus.subscribe<content::ExtentChanged>(
        [&](const content::ExtentChanged&) { ++extents; });
    auto ss = bus.subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged& ev) {
          ++sels;
          expect(ev.ids.size() == 1, "3d pick one id");
        });
    gis::MemoryEditSession edits;
    tool::Workspace ws(&bus, &edits);
    expect(ws.execute("view3d.trackball", {}), "activate trackball keys");
    expect(ws.stack().current() &&
               std::strcmp(ws.stack().current()->id(), "view3d.trackball") == 0,
           "trackball keys current");

    content::InputEvent key = make_event(content::InputEvent::Kind::kKeyDown);
    key.key = 'W';
    expect(ws.dispatch_input(key), "3d key consumed");
    expect(ws.last_draft().kind == tool::DraftKind::kKey, "3d key draft");
    expect(ws.last_draft().key == 'W', "3d key code");
    expect(extents >= 1, "3d key published extent");

    content::InputEvent wheel = make_event(content::InputEvent::Kind::kWheel);
    wheel.wheel = -120;
    wheel.x_px = 8;
    wheel.y_px = 9;
    expect(ws.dispatch_input(wheel), "3d wheel consumed");
    expect(ws.last_draft().kind == tool::DraftKind::kWheel, "3d wheel draft");
    expect(ws.last_draft().wheel == -120, "3d wheel delta");
    expect(extents >= 2, "3d wheel published extent");

    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 6;
    down.y_px = 7;
    content::InputEvent up = make_event(content::InputEvent::Kind::kLUp);
    up.x_px = 6;
    up.y_px = 7;
    expect(ws.dispatch_input(down), "3d pick down");
    expect(ws.dispatch_input(up), "3d pick up");
    expect(ws.last_draft().kind == tool::DraftKind::kPick, "3d pick draft");
    expect(sels == 1, "3d pick published selection");
    expect(edits.committed().empty(), "3d leftover input does not commit");
  }

  {
    tool::Workspace ws(nullptr, nullptr);
    expect(ws.execute("flash.start", {}), "flash start command");
    expect(ws.flashing(), "flash command on");
    expect(ws.stack().current() == nullptr, "flash is command not exclusive");
    expect(!ws.dispatch_input(make_event(content::InputEvent::Kind::kLDown)),
           "flash does not consume click");
    expect(ws.execute("flash.stop", {}), "flash stop command");
    expect(!ws.flashing(), "flash command off");
  }

  {
    int applies = 0;
    gis::CommandEditSession cmds(
        [&](const gis::FeatureMutation& m, bool) {
          ++applies;
          return m.id.len > 0;
        });
    gis::FeatureMutation mutation;
    mutation.op = gis::EditOp::kAppend;
    mutation.id = make_id(9);
    expect(cmds.commit(mutation), "cmd commit");
    expect(cmds.can_undo(), "cmd can undo");
    expect(cmds.undo(), "cmd undo");
    expect(cmds.can_redo(), "cmd can redo");
    expect(cmds.redo(), "cmd redo");
    expect(applies == 3, "execute undo execute");
  }

  {
    int n = 0;
    tool::Draft seen;
    tool::Workspace ws(nullptr, nullptr);
    ws.set_draft_observer([&](const tool::Draft& d) {
      ++n;
      seen = d;
    });
    content::InputEvent wheel = make_event(content::InputEvent::Kind::kWheel);
    wheel.wheel = 120;
    wheel.x_px = 3;
    wheel.y_px = 4;
    expect(ws.dispatch_input(wheel), "2d wheel consumed");
    expect(n == 1, "draft observer wheel");
    expect(seen.kind == tool::DraftKind::kWheel, "observer wheel kind");
    expect(ws.last_draft().kind == tool::DraftKind::kWheel, "workspace last wheel");
    expect(ws.execute("selection.rect", {}), "activate rect for observer");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 1;
    down.y_px = 2;
    content::InputEvent up = make_event(content::InputEvent::Kind::kLUp);
    up.x_px = 8;
    up.y_px = 9;
    expect(ws.dispatch_input(down), "observer rect down");
    expect(ws.dispatch_input(up), "observer rect up");
    expect(n == 2, "observer rect draft");
    expect(seen.kind == tool::DraftKind::kRect, "observer rect kind");
  }

  {
    tool::Workspace ws(nullptr, nullptr);
    expect(ws.execute("selection.rect", {}), "activate rect overlay");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 1;
    down.y_px = 2;
    content::InputEvent move = make_event(content::InputEvent::Kind::kMouseMove);
    move.x_px = 10;
    move.y_px = 12;
    expect(ws.dispatch_input(down), "overlay down");
    ws.aux_draw();
    expect(ws.live_preview() == nullptr, "no rubber until move");
    expect(ws.dispatch_input(move), "overlay move");
    ws.aux_draw();
    const tool::AuxOverlay* o = ws.live_preview();
    expect(o && o->kind == tool::AuxOverlay::Kind::kRect, "rect overlay kind");
    expect(o && o->points.size() == 2, "overlay two corners");
    expect(o && o->points[0].x_px == 1 && o->points[1].x_px == 10,
           "overlay coords");
    expect(ws.last_draft().points.empty(), "no complete draft while dragging");
    content::InputEvent up = make_event(content::InputEvent::Kind::kLUp);
    up.x_px = 10;
    up.y_px = 12;
    expect(ws.dispatch_input(up), "overlay up");
    ws.aux_draw();
    expect(ws.live_preview() == nullptr, "overlay cleared after complete");
    expect(ws.last_draft().kind == tool::DraftKind::kRect, "complete rect draft");
  }

  {
    tool::Workspace ws(nullptr, nullptr);
    expect(ws.execute("edit.append.linestring", {}), "activate line overlay");
    content::InputEvent a = make_event(content::InputEvent::Kind::kLDown);
    a.x_px = 0;
    a.y_px = 0;
    content::InputEvent move = make_event(content::InputEvent::Kind::kMouseMove);
    move.x_px = 5;
    move.y_px = 6;
    expect(ws.dispatch_input(a), "line overlay first");
    expect(ws.dispatch_input(move), "line overlay hover");
    ws.aux_draw();
    const tool::AuxOverlay* o = ws.live_preview();
    expect(o && o->kind == tool::AuxOverlay::Kind::kPolyline, "line overlay kind");
    expect(o && o->points.size() == 2, "line overlay vertex plus hover");
  }

  {
    // SP1b: pan + select + append pointer chain on one Workspace.
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

    expect(ws.execute("view.pan", {}), "sp1b pan");
    content::InputEvent d = make_event(content::InputEvent::Kind::kLDown);
    d.x_px = 0;
    d.y_px = 0;
    content::InputEvent u = make_event(content::InputEvent::Kind::kLUp);
    u.x_px = 6;
    u.y_px = 6;
    expect(ws.dispatch_input(d), "sp1b pan down");
    expect(ws.dispatch_input(u), "sp1b pan up");
    expect(extents >= 1, "sp1b ExtentChanged");

    expect(ws.execute("selection.rect", {}), "sp1b select");
    d.x_px = 1;
    d.y_px = 1;
    u.x_px = 9;
    u.y_px = 9;
    expect(ws.dispatch_input(d), "sp1b sel down");
    expect(ws.dispatch_input(u), "sp1b sel up");
    expect(sels >= 1, "sp1b SelectionChanged");

    expect(ws.execute("edit.append.point", {}), "sp1b append");
    d.x_px = 2;
    d.y_px = 3;
    expect(ws.dispatch_input(d), "sp1b append down");
    expect(commits >= 1 && edits.can_undo(), "sp1b EditCommitted");
  }

  if (g_fails) {
    std::fprintf(stderr, "tool_dispatch_test: %d failed\n", g_fails);
    return 1;
  }
  std::fprintf(stdout, "tool_dispatch_test: ok\n");
  return 0;
}
