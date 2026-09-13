# Leftover chrome 组合对等 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `SmartGisViews.exe` 上闭环 FeatureInfo/AttributeTable/Ambox 枚举/Chart 模态，完成 leftover chrome 组合优先对等。

**Architecture:** 不改 leftover MFC。补 `CommandCatalog` 枚举；`AmboxView` 用枚举填组；`BrowserView` 订阅 `SelectionChanged` 写 inspector，并加 Chart 菜单。字符串-only，无 GIS 指针。

**Tech Stack:** C++23, GN/`build.bat views`, `ui::views`, `content::EventBus`, `tool::CommandCatalog`

## Global Constraints

- Work on `master` only; do not create feature branches.
- Do not modify leftover `src/ui/{xview,xcatalog,xambox,chart,gui,mfc_ex}` for features (compile-only fixes OK).
- No Qt; no true dock/MDI; widgets never hold `SmtFeature*`.
- Source comments English; user-facing docs Chinese.
- Copyright `(c) 2026 The Mogu Authors.` on touched/new files.
- New-tree functions `snake_case`; public namespaces ≤ two layers (`ui::views`, `tool`, `content`).
- **Do not `git commit` unless the user explicitly asks.**
- Spec: `docs/superpowers/specs/2026-09-14-ui-leftover-chrome-parity-design.md`

## File map

| File | Role |
| --- | --- |
| `src/tool/command.h` `.cc` | `CommandCatalog::for_each` / id enumeration |
| `src/tool/dispatch_test.cc` or command test | enumerate unit coverage |
| `src/ui/views/ambox_view.cc` | build groups from catalog when non-empty |
| `src/app/views/browser_view.h` `.cc` | hold FeatureInfo*/AttributeTable*; subscribe selection; Chart menu |
| `src/ui/views/views_unittests.cc` | Ambox enumerate + Chart series + FeatureInfo fill |
| `src/app/views/README.md` | note Chart + selection wiring |

---

### Task 1: CommandCatalog enumeration

**Files:**
- Modify: `src/tool/command.h`, `src/tool/command.cc`
- Modify: `src/tool/dispatch_test.cc` (or nearest command test)
- Consumes: existing `CommandCatalog::add`
- Produces: `void for_each(const std::function<void(std::string_view id)>& fn) const;`

- [ ] **Step 1: Add failing test** that registers two ids and collects them via `for_each`, expects both present (order = map order OK).

- [ ] **Step 2: Implement**

```cpp
// command.h — inside CommandCatalog public:
void for_each(const std::function<void(std::string_view id)>& fn) const;

// command.cc
void CommandCatalog::for_each(
    const std::function<void(std::string_view id)>& fn) const {
  if (!fn) {
    return;
  }
  for (const auto& kv : handlers_) {
    fn(kv.first);
  }
}
```

Include `<functional>` in the header if missing.

- [ ] **Step 3: Run** the tool/dispatch test target that covers CommandCatalog (prefer existing `dispatch_test` / tool test via `build.bat` or ninja target already used in-tree). Expect PASS.
- [ ] **Step 4: Do not commit.**

---

### Task 2: AmboxView from CommandCatalog

**Files:**
- Modify: `src/ui/views/ambox_view.cc`
- Modify: `src/ui/views/views_unittests.cc`
- Consumes: `CommandCatalog::for_each`
- Produces: non-dummy groups when `host->commands()` has ids

- [ ] **Step 1: Failing test** — build a `tool::CommandCatalog`, `add("selection.point", …)`, `add("view.pan", …)`, create `PluginHost` or call a testable path. Prefer unit-testing by making `populate_from_plugin_host` use `commands()->for_each` and asserting group item ids include `selection.point` / `view.pan` when host non-null with catalog. If PluginHost construction is heavy, extract a package-visible helper or test via temporary catalog injected through a small overload:

```cpp
void populate_from_commands(tool::CommandCatalog* catalog);
```

Call it from `populate_from_plugin_host` when `host && host->commands()`.

- [ ] **Step 2: Implement grouping**
  - ids starting with `selection.` or equal `select` / `identify` → group Select
  - ids starting with `view.` or equal `pan` → group Pan / View
  - other command ids → group Tools
  - Always ensure at least Select/Pan/Identify rows exist (merge dummy if catalog empty or missing those).
  - Keep click ids stable: map catalog `selection.point` button id to `selection.point` (BrowserView already `activate`/`execute`).

- [ ] **Step 3: Run** `views_unittests` (or ninja `//src/ui/views:views_unittests`). Expect PASS.
- [ ] **Step 4: Do not commit.**

---

### Task 3: BrowserView selection → inspector + Chart menu

**Files:**
- Modify: `src/app/views/browser_view.h`, `src/app/views/browser_view.cc`
- Modify: `src/app/views/README.md`
- Consumes: `content::SelectionChanged`, `FeatureInfo`, `AttributeTable`, `ChartView::run_modal`
- Produces: live inspector updates; Chart menu

- [ ] **Step 1: Members** on `BrowserView`:

```cpp
ui::views::FeatureInfo* feature_info_ = nullptr;
ui::views::AttributeTable* attribute_table_ = nullptr;
ui::views::AmboxView* ambox_ = nullptr;
// keep EventBus::Connection members if Connection is RAII — store connections
```

When building inspector tabs, assign raw pointers from `get()` before `add_tab` move, same pattern as `catalog_`.

- [ ] **Step 2: Helper** (anonymous or `app::detail`):

```cpp
std::string feature_id_token(const content::FeatureId& id);
void apply_selection(ui::views::FeatureInfo* info,
                     ui::views::AttributeTable* table,
                     const content::SelectionChanged& ev);
```

`feature_id_token`: hex of `id.bytes[0..id.len)` (cap 32). Empty ids → clear both widgets.

`apply_selection`: first id → `set_feature_id(token)`; fields `{{"id", token},{"view_id", std::to_string(ev.view_id)}}`; table columns `{"id"}` and one row `{token}` (or clear).

- [ ] **Step 3: Subscribe** after hosts created — for each of `edit_host_`, `data_host_`, `scene_host_`, if `events()`, subscribe `SelectionChanged` → `apply_selection`. Store `Connection` objects as members so they outlive callbacks.

- [ ] **Step 4: Chart menu** — `menu->add_item("Chart", [this]{ on_chart(); });`  
  `on_chart`: build series from `catalog_->layer_tree()` visible layers (label=name, value=1.0); if empty use `{ {"A",1},{"B",2},{"C",1.5} }`; call `ChartView::run_modal(hwnd(), L"Chart", series)` when dialog available.

- [ ] **Step 5: Ambox** — keep pointer; optionally `populate_from_plugin_host` if BrowserView later owns PluginHost. Minimum: ensure command handler still maps `identify`/`select` → `selection.point`, `pan` → `view.pan`, else `execute(id)`.

- [ ] **Step 6: Update** `src/app/views/README.md` — Chart menu + SelectionChanged → inspector.
- [ ] **Step 7: Build** `build.bat views` (or ninja SmartGisViews + views_unittests). Fix compile. Do not commit.

---

### Task 4: Unit tests for inspector + Chart

**Files:**
- Modify: `src/ui/views/views_unittests.cc`

- [ ] **Step 1:** Test FeatureInfo `set_fields` / `clear` already exist — add AttributeTable `set_rows` round-trip if missing.
- [ ] **Step 2:** Test ChartView `set_series` non-empty (paint not required).
- [ ] **Step 3:** Run views_unittests PASS. Do not commit.

---

### Task 5: Docs index touch

**Files:**
- Modify: `docs/README.md` only if it lists active superpowers specs — add one line pointing at the parity spec/plan.
- Modify: `docs/build/ui-views-skia.md` Status bullet if needed (one sentence: composition parity work package 2026-09-14).

- [ ] **Step 1:** Minimal link edits. Do not commit.

---

## Self-review (plan)

- Spec coverage: enumeration, Ambox, selection→inspector, Chart, tests, leftover freeze → Tasks 1–5.
- No leftover MFC edits in tasks.
- No commit steps requiring user-unasked commits.
