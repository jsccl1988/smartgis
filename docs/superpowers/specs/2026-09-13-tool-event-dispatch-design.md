<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Tool layer: session dispatch + event/operation split

**Date:** 2026-09-13  
**Status:** accepted (v1 landed)  
**Related:** leftover path split [`../archive/specs/2026-09-13-tool-legacy-split-design.md`](../archive/specs/2026-09-13-tool-legacy-split-design.md).  
**Scope:** one implementation plan. Replace the 2010 `SmtIATool` = event-bus + mouse-state-machine mash-up with four channels (Command, Input, Operation, Domain Event), scoped per map session/view. Align with QGIS `QAction`/`QgsMapTool` and ArcGIS `ICommand`/`ITool`. Do not rewrite leftover `Smt_*` ABI in this change.

## Goal

`src/tool` today is three copied int buses (`SmtListenerManager`, `SmtIAToolManager`, `SmtAModuleManager`) plus an exclusive `GetActiveIATool()` mouse sink. Menu items, mouse gestures, document writes, and ?get me the 2D view? RPC all share `long` + `WPARAM`/`LPARAM`.

The replacement is:

1. **Session-scoped dispatch** (not `GetSingletonPtr()`).
2. **Event and operation decoupled** from interaction: a tool never writes `SmtMap`; it yields a draft, `sdb::EditSession` commits, `content::EventBus` reports what already happened.

## Non-goals

- Do not rewrite leftover `SmtIATool` / `SmtGroupTool` classes or change `dll_stem` (`SmtToolCore`, `SmtGroupToolCore`).
- Do not add a fourth `*Manager` singleton.
- Do not put `EventBus` under `src/tool` (events follow the document / session).
- Do not use the bus as RPC (`GT_MSG_GET_*`, `SMT_MSG_GET_SYS_2DVIEW`). Queries are methods on `MapSession` / `MapContents`.
- Do not ship an OpenLayers-style arbitrary interaction stack in v1 (only exclusive current + two always-on handlers).
- Do not put HWND, `SmtMap*`, or `LPRENDERDEVICE` on new public headers.
- Do not vendor Qt signals, Boost.Signals2, or eventpp.
- Qt is banned.
- Do not change Mojo / `--type=` (existing `content::ToolRouter` stays the IPC face).

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| OSS model | QGIS / ArcGIS: Command vs MapTool. Borrow Mapbox Draw ?store commits, mode does not mutate? |
| Dispatch scope | Per `tool::Workspace` (one per map view). Not process-global |
| Input | `InputRouter` targeting, not EventBus |
| Commands | Stable string ids (`view.zoom_in`). Leftover `GT_MSG_*` mapped by adapter |
| Operations | `sdb::EditSession` is the only document write path for new code |
| Domain events | `content::EventBus` typed, session-scoped, main-thread v1 |
| Always-on | `wheel.zoom` + `hover.cursor` only |
| Delegate | `InteractionStack::push` / `pop` replaces `BeginDelegate` |
| Leftover tools | Keep compiling; new path does not `#include t_iatool.h` |

## Architecture

```
Chrome (Views / leftover MFC)
  CommandId  ???????????????????????????  tool::CommandDispatcher
  leftover pointer  ?? content::ToolRouter::dispatch
                              ?
                              ? renderer / in-process
                       tool::Workspace (per view)
                          ?
          ??????????????????????????????????
          ?               ?                ?
   InputRouter      CommandCatalog    sdb::EditSession
   always-on           ?                    ?
   + current      activate Interaction      commit
   InteractionStack                         ?
                                            ?
                                   content::EventBus
                                   SelectionChanged
                                   ExtentChanged
```

Chrome includes only `content/public`. It must not include `t_iatool.h`.

`content::ViewHost` is the per-view composition (owns `tool::Workspace` + `EventBus` + `sdb::EditSession`). `content::LocalToolRouter` is the in-process `ToolRouter`: command/interaction ids hit Workspace first; leftover OOP/web still send `HostMsg::kActivateTool` / `kPointerEvent` JSON when the named pipe is open. Leftover MFC `src/ui/xview` owns a `ViewHost` on `SmtXView` (Memory, or `MapEditSession` on the edit view). Mapped `GT_MSG_*` do not broadcast `SMT_POST_IATOOL_MSG`.

## Components

| Unit | Tree | Namespace | Role |
| --- | --- | --- | --- |
| `EventBus` | `src/content/public/event_bus.h` | `content` | Typed pub/sub. Header-only |
| Domain events | `src/content/public/events.h` | `content` | `SelectionChanged`, `ExtentChanged` |
| `Command` / `Catalog` / `Dispatcher` | `src/tool/command.h` | `tool` | Fire-and-forget ids |
| `Interaction` / `Stack` / `InputRouter` | `src/tool/interaction.h` | `tool` | Mouse state machines |
| `Workspace` | `src/tool/workspace.h` | `tool` | Composition root per view |
| `EditSession` | `src/sdb/edit/edit_session.h` | `sdb` | Undoable document mutations |
| `command_id_from_gt_msg` | `src/tool/legacy_msg.h` | `tool` | `GT_MSG_*` ? string id |
| Leftover | `src/legacy/tool/t_*.h`, `legacy/tool/group` | `Smt_IATool` | Unchanged DLL |

New modules are **source_sets**, not DLLs. `//src/tool:dispatch` and `//src/sdb/edit:edit` join `src_all`. Tests: `tool_dispatch_test`.

Public C++ stays two levels (`tool`, `sdb`, `content`). Helpers in `detail` or an anonymous namespace.

## Command ids (v1)

| id | Kind | Notes |
| --- | --- | --- |
| `view.zoom_in` | activate Interaction | leftover `GT_MSG_VIEW_ZOOMIN` |
| `view.zoom_out` | activate | |
| `view.pan` | activate | `GT_MSG_VIEW_ZOOMMOVE` |
| `view.full` | Command | no mouse; leftover restore |
| `view.refresh` | Command | |
| `selection.point` | activate `select.point` | |
| `selection.rect` | activate `select.rect` | |
| `selection.polygon` | activate `select.polygon` | |
| `selection.clear` | Command | writes selection via EditSession / EventBus; not a tool |
| `edit.append.point` | activate `draw.point` | |
| `edit.append.linestring` | activate `draw.linestring` | |
| `edit.append.polygon` | activate `draw.polygon` | |
| `flash.start` / `flash.stop` | Command | overlay only; not an EditSession op |
| `view3d.trackball` / `sphere` / `fps` | activate Interaction | leftover `GT_MSG_3DVIEW_*`; camera objects from `make_view3d_camera` |
| `view3d.full` | Command | leftover `GT_MSG_3DVIEW_RESTORE` |

`GT_MSG_WSVIEW_*` aliases the 2D `view.*` ids. Extra leftover append types (anno, spline, rect, fan) map to `edit.append.point` / `linestring` / `polygon`.

Always-on ids `wheel.zoom` and `hover.cursor` are **not** in the catalog and cannot be `execute`d.

## API

### EventBus

```cpp
namespace content {

struct SelectionChanged {
  uint32_t view_id = 0;
  std::vector<FeatureId> ids;
};

struct ExtentChanged {
  uint32_t view_id = 0;
  Extent2 extent{};
};

class EventBus {
 public:
  class Connection {
   public:
    Connection() = default;
    Connection(Connection&&) noexcept;
    Connection& operator=(Connection&&) noexcept;
    ~Connection();
    void disconnect();
    explicit operator bool() const;
  };

  template <typename E>
  Connection subscribe(std::function<void(const E&)> fn);

  template <typename E>
  void publish(const E& e) const;
};

}  // namespace content
```

v1 is single-threaded (renderer / test main). `publish` snapshots the slot list so a subscriber may `disconnect` or `subscribe` during a callback. Duplicate subscribe is allowed (two connections). There is no process-wide instance.

### Command

```cpp
namespace tool {

struct CommandArgs {
  uint32_t view_id = 0;
  std::string_view payload;  // optional, may be empty
};

using CommandHandler = std::function<bool(const CommandArgs&)>;

class CommandCatalog {
 public:
  bool add(std::string_view id, CommandHandler handler);
  const CommandHandler* find(std::string_view id) const;
  bool contains(std::string_view id) const;
};

class CommandDispatcher {
 public:
  explicit CommandDispatcher(CommandCatalog* catalog);
  bool execute(std::string_view id, const CommandArgs& args);
};

}  // namespace tool
```

`add` fails (returns false) on empty id or duplicate id. `execute` returns false if the id is missing or the handler returns false. Unknown ids are not broadcast.

### Interaction + InputRouter

```cpp
namespace tool {

struct AuxPoint {
  int32_t x_px = 0;
  int32_t y_px = 0;
};

struct AuxOverlay {
  enum class Kind { kNone, kRect, kPolyline };
  Kind kind = Kind::kNone;
  std::vector<AuxPoint> points;
};

class Interaction {
 public:
  virtual ~Interaction() = default;
  virtual const char* id() const = 0;
  virtual void activate() {}
  virtual void deactivate() {}
  virtual bool on_input(const content::InputEvent& e) = 0;
  virtual void aux_draw() {}
  virtual const AuxOverlay* aux_overlay() const { return nullptr; }
};

using InteractionFactory = std::function<std::unique_ptr<Interaction>()>;

class InteractionRegistry {
 public:
  bool add(std::string_view id, InteractionFactory factory);
  std::unique_ptr<Interaction> make(std::string_view id) const;
};

class InteractionStack {
 public:
  Interaction* current() const;
  bool activate(std::string_view id, const InteractionRegistry& registry);
  bool push(std::string_view id, const InteractionRegistry& registry);
  bool pop();
};

class InputRouter {
 public:
  void add_always_on(std::unique_ptr<Interaction> handler);
  void set_stack(InteractionStack* stack);
  bool dispatch(const content::InputEvent& e);
};

}  // namespace tool
```

`InputRouter::dispatch` order: always-on in insertion order, then `stack->current()`. A handler that returns true stops the chain. `hover.cursor` must return false so the current tool still sees moves. `wheel.zoom` returns true on `kWheel` only.

`activate` replaces the current exclusive tool (deactivate old, activate new). `push` keeps the previous exclusive tool on a stack (legacy `BeginDelegate`). `pop` deactivates the top and re-activates the previous. Empty stack is valid (no exclusive tool).

### EditSession

```cpp
namespace sdb {

enum class EditOp { kAppend, kDelete, kModify };

struct FeatureMutation {
  EditOp op = EditOp::kAppend;
  content::FeatureId id{};
};

class EditSession {
 public:
  virtual ~EditSession() = default;
  virtual bool commit(const FeatureMutation& mutation) = 0;
  virtual bool undo() = 0;
  virtual bool redo() = 0;
  virtual bool can_undo() const = 0;
  virtual bool can_redo() const = 0;
};

class MemoryEditSession : public EditSession {
 public:
  const std::vector<FeatureMutation>& committed() const;
  // undo/redo stacks as specified in the plan tests
};

}  // namespace sdb
```

`commit` records the mutation and clears redo. `undo` / `redo` move the last mutation between stacks. New code does not call `SmtMap::AppendFeature` from an `Interaction`. A later `SmtMapEditSession` (out of v1) wraps leftover `SmtCommandManager`.

`EditSession` does **not** publish events. The caller publishes `SelectionChanged` after a successful selection command. That is the decoupling.

### Workspace

```cpp
namespace tool {

class Workspace {
 public:
  Workspace(content::EventBus* events, sdb::EditSession* edits);
  CommandCatalog& catalog();
  CommandDispatcher& dispatcher();
  InteractionRegistry& interactions();
  InteractionStack& stack();
  InputRouter& router();

  bool execute(std::string_view command_id, const CommandArgs& args);
  bool activate(std::string_view interaction_id);
  bool dispatch_input(const content::InputEvent& e);
  void aux_draw();
  const AuxOverlay* live_preview() const;
};

}  // namespace tool
```

Constructor installs always-on `wheel.zoom` and `hover.cursor`, and registers v1 command ids that either `activate` an interaction or (for `selection.clear` / `view.refresh` / `view.full` / `flash.*`) run without a tool. `content::ToolRouter::activate` maps to `Workspace::execute` when the string is a command id, else `Workspace::activate`.

### Legacy adapter

```cpp
namespace tool {
const char* command_id_from_gt_msg(long msg);  // nullptr if unknown
}
```

`command_id_from_gt_msg` uses numeric copies of leftover `GT_MSG_CMD_*` in `legacy_msg.h` (do not include `legacy/tool/group/defs.h` from dispatch TUs ? that header pulls WinSock through `msg.h` and breaks ASIO/`fd_set`). Keep the enum in sync with `defs.h`.

## Data flow

**Activate select-rect from chrome**

1. Chrome `execute("selection.rect")` or `ToolRouter::activate(view, "selection.rect")`.
2. `CommandDispatcher` runs the catalog handler.
3. Handler calls `stack.activate("select.rect")`.
4. No EventBus fire (nothing happened to the document yet).

**Pointer**

1. Chrome hit-tests chrome first. Misses go to `ToolRouter::dispatch`.
2. `InputRouter`: `hover.cursor` sees `kMouseMove` (returns false); current `select.rect` sees `kLDown`/`kLUp`.
3. Rubber-band lives in the Interaction. No `SMT_POST_IATOOL_MSG`.

**Gesture complete**

1. Interaction yields a draft (envelope or ring) to the command handler / Workspace callback.
2. Selection: Workspace updates selection ids and `events->publish(SelectionChanged{...})`.
3. Digitizing: `edits->commit({kAppend, id})`. Interaction does not touch `SmtMap`.

**Nested draw (legacy delegate)**

`select.polygon` `push("draw.polygon")`. On finish, `pop()`, then apply the ring as selection. The draw interaction never registers a menu item.

## Error handling

| Case | Result |
| --- | --- |
| Unknown command id | `execute` returns false; no broadcast |
| Duplicate `catalog.add` | returns false; first handler kept |
| `activate` unknown interaction | returns false; current tool unchanged |
| `pop` on empty stack | returns false |
| `commit` with zero-length `FeatureId` | returns false; stacks unchanged |
| Subscribe with empty `std::function` | ignored; empty Connection |
| `publish` during `publish` | allowed; snapshot isolates the outer list |

## Testing

`testing/test.gni` `test()` + `expect`/`main` like `scene_test`. Target `//src/tool:tool_dispatch_test`, added to `//:test_all`.

Must cover:

- EventBus: two subscribers, unsubscribe mid-publish, typed isolation (`SelectionChanged` does not invoke an `ExtentChanged` slot).
- Dispatcher: unknown id false; duplicate add false; `selection.clear` does not change `stack.current()`.
- Router: wheel consumed by always-on; `kLDown` reaches exclusive tool; hover does not swallow moves.
- Stack: activate replace; push/pop restores previous id.
- MemoryEditSession: commit / undo / redo; empty id rejected.
- `command_id_from_gt_msg(kGtMsgViewZoomIn) == "view.zoom_in"`.
- Rubber-band: `select.rect` LDown+move fills `live_preview` (`kRect`, two points) before LUp emits the draft.

## Docs

Same change set: `docs/README.md` index, `docs/build/src-layout.md` tool row + `sdb/edit`, `src/README.md` tool paragraph. Root `README.md` only if the module table still claims tool is ?interactive tools? with no dispatch split ? add one clause, refresh ????.

## Leftover plugin menus

Leftover `AppendFuncItems` / `Notify` longs resolve through `plugin::command_id_from_am_msg`: `GT_MSG_*` (including `SMT_MSG_KEY` LOWORD) via `command_id_from_gt_msg` to `view.*` / `selection.*` / `edit.append.*` / `flash.*` / `view3d.*`; plugin `AM_MSG_*` to `dem.*` / `proj.do_prj` / `print.preview` / `model3d.*` / `baogrid.*` (leftover BAOGrid AM numbers). No third int bus. Mapped menu commands `execute` on `ViewHost` **and** leftover `Notify` (camera / dialogs / `SetActive`). Unmapped menu ids do **not** `SMT_IATOOL_MSG_BROADCAST` / `SMT_AM_MSG_BROADCAST`. Pointer and wheel go only through `Workspace`; leftover tools implement `apply_draft` and do not own a second Interaction. Rubber-band geometry lives on `Interaction::aux_draw` / `aux_overlay`; leftover chrome paints that overlay. 3D camera objects are constructed in `src/render/render3d` (`make_view3d_camera`); leftover tools bind ETU / scene / FPS win center. The new Views command may execute `edit.append.linestring` when leftover is not loaded.

## Out of v1 (later tasks, not this plan)

- OL-style multi-interaction stack.

v2 (this change): leftover `SmtSelectTool` / `SmtAppendFeatureTool` mouse and undo go through `Interaction` / `MapEditSession`.
