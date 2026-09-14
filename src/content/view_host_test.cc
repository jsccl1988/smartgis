// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "content/public/event_bus.h"
#include "content/public/events.h"
#include "content/public/local_tool_router.h"
#include "content/public/view_host.h"
#include "sdb/edit/edit_session.h"
#include "tool/interaction.h"
#include "tool/legacy_msg.h"
#include "tool/workspace.h"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

int g_fails = 0;

void expect(bool ok, const char* msg) {
  if (!ok) {
    std::fprintf(stderr, "FAIL: %s\n", msg);
    ++g_fails;
  }
}

content::InputEvent make_event(content::InputEvent::Kind kind, int wheel = 0) {
  content::InputEvent e{};
  e.kind = kind;
  e.wheel = wheel;
  return e;
}

}  // namespace

int main() {
  {
    content::ViewHost host;
    expect(host.events() != nullptr, "host events");
    expect(host.edits() != nullptr, "host memory edits");
    expect(host.workspace() != nullptr, "host workspace");
    expect(host.execute("selection.rect"), "execute selection.rect");
    tool::Interaction* cur = host.workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "select.rect") == 0, "stack select.rect");
    expect(!host.execute("no.such.command"), "unknown command");
    expect(host.activate("select.point"), "activate interaction");
    cur = host.workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "select.point") == 0,
           "stack select.point");
    expect(!host.activate("no.such.tool"), "unknown interaction");
  }

  {
    content::ViewHost host;
    int n = 0;
    auto conn = host.events()->subscribe<content::SelectionChanged>(
        [&](const content::SelectionChanged& e) {
          ++n;
          expect(e.view_id == 7, "clear view_id");
          expect(e.ids.empty(), "clear empty ids");
        });
    expect(host.execute("selection.clear", 7), "execute selection.clear");
    expect(n == 1, "SelectionChanged published");
  }

  {
    content::ViewHost host;
    expect(host.dispatch_input(make_event(content::InputEvent::Kind::kWheel, 1)),
           "wheel consumed");
    expect(!host.dispatch_input(make_event(content::InputEvent::Kind::kMouseMove)),
           "hover does not swallow");
  }

  {
    content::ViewHost host;
    expect(host.execute_legacy(tool::kGtMsgViewZoomIn), "legacy zoom_in");
    tool::Interaction* cur = host.workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "view.zoom_in") == 0, "legacy mapped");
    expect(host.execute_legacy(tool::kGtMsg3dViewFps), "legacy 3d fps");
    cur = host.workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "view3d.fps") == 0, "3d legacy mapped");
    expect(!host.execute_legacy(0x7fffffff), "unknown leftover");
    expect(!host.execute_legacy(-1), "unmapped is not a command");
    expect(!host.execute("print.preview"), "plugin id not in workspace catalog");
  }

  {
    content::ViewHost host;
    expect(!host.flashing(), "flash off at start");
    expect(host.execute("flash.start"), "flash.start");
    expect(host.flashing(), "host flashing follows workspace");
    expect(host.execute("flash.stop"), "flash.stop");
    expect(!host.flashing(), "flash off after stop");
  }

  {
    sdb::MemoryEditSession edits;
    content::ViewHost host(&edits);
    expect(host.edits() == &edits, "supplied EditSession");
    sdb::FeatureMutation m;
    m.id.len = 1;
    m.id.bytes[0] = 1;
    expect(host.edits()->commit(m), "commit via host edits");
    expect(edits.committed().size() == 1, "memory log");
  }

  {
    content::ViewHost host;
    int commits = 0;
    auto conn = host.events()->subscribe<content::EditCommitted>(
        [&](const content::EditCommitted& e) {
          ++commits;
          expect(e.op == content::EditCommitted::Op::kAppend, "append op");
        });
    expect(host.execute("edit.append.point"), "execute edit.append.point");
    tool::Interaction* cur = host.workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "draw.point") == 0, "stack draw.point");
    content::InputEvent down = make_event(content::InputEvent::Kind::kLDown);
    down.x_px = 5;
    down.y_px = 6;
    expect(host.dispatch_input(down), "draw point input");
    expect(commits == 1, "EditCommitted published");
    expect(host.edits() && host.edits()->can_undo(), "can undo after draw");
    expect(host.execute("edit.undo"), "edit.undo");
    expect(!host.edits()->can_undo(), "undo emptied");
    expect(host.execute("edit.redo"), "edit.redo");
    expect(host.edits()->can_undo(), "redo restored");
  }

  {
    int ipc_n = 0;
    std::string ipc_id;
    content::LocalToolRouter router;
    router.set_activate_ipc([&](uint32_t view_id, const char* tool_id) {
      ++ipc_n;
      expect(view_id == 3, "ipc view");
      ipc_id = tool_id ? tool_id : "";
    });
    router.activate(3, "selection.rect");
    tool::Interaction* cur = router.host(3)->workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "select.rect") == 0,
           "router local execute");
    expect(ipc_n == 1, "ipc leftover still called");
    expect(ipc_id == "selection.rect", "ipc tool_id");

    router.activate(3, "select.polygon");
    cur = router.host(3)->workspace()->stack().current();
    expect(cur && std::strcmp(cur->id(), "select.polygon") == 0,
           "router fallback activate");
    expect(ipc_n == 2, "ipc on interaction id");
  }

  {
    int ptr_n = 0;
    content::LocalToolRouter router;
    router.set_dispatch_ipc([&](uint32_t, const content::InputEvent&) { ++ptr_n; });
    expect(router.host(1) != nullptr, "lazy host");
    router.dispatch(1, make_event(content::InputEvent::Kind::kWheel, 120));
    expect(ptr_n == 1, "pointer ipc leftover");
  }

  if (g_fails) {
    std::fprintf(stderr, "content_view_host_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::printf("content_view_host_test: ok\n");
  return 0;
}
