<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin host + store + Python Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** Land a QGIS-shaped in-process extension platform: `plugin::Registry`, `content::PluginHost`, Views dialog rewrite, CPython embed, zip/`plugins.json` store, and a processing worker thread.

**Architecture:** Registry is app-scoped in `src/plugin`. Chrome map code includes only `content/public`; Plugin Manager may include `plugin/registry.h`. Plugins contribute into existing `tool::CommandCatalog`. Document writes stay on `sdb::EditSession`; domain events stay on `content::EventBus`. Leftover `SmtPluginManager` only loads `*.am`. Plugin UI never leaves the chrome process.

**Tech Stack:** C++23, GN/Ninja (`build.bat` / `ninja -C out`), `testing/test.gni` `expect`/`main`, CPython 3.12 embed, SUPERCOP ref10 ed25519, `net::HttpClient`. No Qt. No new `*Manager` singleton. `plugin/map_service` was deleted with the web stack; do not recreate it.

## Global Constraints

- Work on `master` only. Do not create branches or worktrees.
- Do not `git commit` unless the user explicitly asks.
- Copyright: `Copyright (c) 2026 The Mogu Authors.` on every new engineering file; bump year on touched Mogu headers.
- Public namespaces: two levels (`plugin`, `content`). Deeper = `detail` or anonymous.
- New functions `snake_case`. Types PascalCase.
- Comments in English. Class purpose in one or two sentences.
- Product C++23 (`cc_std = "c++23"`). Output only under repo-root `out/` (never `out/Default`).
- Exact APIs: copy from `docs/superpowers/specs/2026-09-13-plugin-host-design.md`.
- No Qt / PyQt. No Mojo for plugin UI. No Registry in `src/base`. No EventBus under `src/plugin`. No algorithms inside dialog classes.
- Leftover `SmtAuxModule` / six `SmtAM*` `dll_stem`s keep compiling. Do not rewrite leftover `dlg_*.h` as the new public API.
- Do not add a third `*Manager` singleton.

## File map

| Path | Responsibility |
| --- | --- |
| `src/plugin/manifest.h` `.cc` | `plugin.json` parse |
| `src/plugin/registry.h` `.cc` | enable / disable / trust / start-stop |
| `src/plugin/legacy_am.h` `.cc` | leftover `*.am` name map + `SmtPluginManager` |
| `src/content/public/plugin_host.h` | `PluginHost` / `MapContents` / contribution structs |
| `src/content/plugin_host.cc` | default host |
| `src/ui/views/{label,button,textfield,checkbox,radio_button,combobox,tab_strip,table_view,file_picker,message_box}.*` | toolkit controls (flat files) |
| `src/plugin/widgets/map_preview.h` `.cc` | shared map preview |
| `src/plugin/widgets/about_dialog.h` `.cc` | shared about |
| `src/plugin/dem/*_dialog.*` | DEM Views |
| `src/plugin/proj/map_prj_*` | projection Views |
| `src/plugin/print/print_preview_dialog.*` | print shell |
| `src/plugin/model3d/model3d_commands.*` | nine commands |
| `src/plugin/baogrid/baogrid_commands.*` | four commands |
| `src/plugin/manager_view.h` `.cc` | Plugin Manager |
| `src/plugin/python/runtime.h` `.cc` `bindings.cc` | CPython + `smartgis.*` |
| `src/plugin/samples/hello/` | sample Python plugin |
| `src/plugin/store.h` `.cc` `signature.h` `.cc` `official_key.h` | store + ed25519 |
| `src/plugin/processing.h` `.cc` | worker pool |
| `src/plugin/host_test.cc` | C++ host tests |
| `src/plugin/python/python_test.cc` | embed tests |
| leftover `src/base/plugin*` `src/plugin/module*` domain `dlg_*` | unchanged ABI |

**Exact APIs** live in the spec. Copy signatures; do not invent parallel names.

**Parallelism:** Tasks 1–3 have no shared new files and may run together. Tasks 4–9 need Tasks 2 and 3; they do not share files with each other and may run in parallel. Tasks 10–13 need Task 2 (Task 12 also needs Task 1). Task 14 last.

---

### Task 1: Manifest + Registry + leftover adapter

**Files:**
- Create: `src/plugin/manifest.h`
- Create: `src/plugin/manifest.cc`
- Create: `src/plugin/registry.h`
- Create: `src/plugin/registry.cc`
- Create: `src/plugin/legacy_am.h`
- Create: `src/plugin/legacy_am.cc`
- Create: `src/plugin/host_test.cc` (extend in later tasks; this task adds the first `main`)
- Modify: `src/plugin/BUILD.gn` — add `source_set("host")` and `test("plugin_host_test")`
- Modify: `src/BUILD.gn` — add `"//src/plugin:host"` to `src_all`
- Modify: `BUILD.gn` — add `"//src/plugin:plugin_host_test"` to `test_all`

**Interfaces:**
- Consumes: none (forward-declare `content::PluginHost`)
- Produces: `plugin::parse_manifest`, `plugin::Registry`, `plugin::legacy_id_from_stem`, `plugin::legacy_id_from_display_name`

- [x] **Step 1: Write the failing test** in `src/plugin/host_test.cc`.

```cpp
// Copyright (c) 2026 The Mogu Authors.
// All rights reserved.

#include "plugin/host/legacy_am.h"
#include "plugin/host/manifest.h"
#include "plugin/host/registry.h"

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
}  // namespace

int main() {
  {
    plugin::Manifest m;
    std::string err;
    const char* json =
        "{\"id\":\"smartgis.dem\",\"name\":\"DEM\",\"version\":\"1.0.0\","
        "\"api_version\":2,\"kind\":\"builtin\",\"author\":\"\","
        "\"description\":\"\"}";
    expect(plugin::parse_manifest(json, &m, &err), "happy manifest");
    expect(m.id == "smartgis.dem", "id");
    expect(m.api_version == 2, "api 2");
    expect(m.kind == plugin::PluginKind::kBuiltin, "kind");
  }
  {
    plugin::Manifest m;
    std::string err;
    expect(!plugin::parse_manifest("{\"name\":\"x\"}", &m, &err),
           "missing id fails");
  }
  {
    plugin::Manifest m;
    std::string err;
    const char* json =
        "{\"id\":\"smartgis.sample_hello\",\"name\":\"H\",\"version\":\"1.0.0\","
        "\"api_version\":1,\"kind\":\"python\",\"author\":\"\","
        "\"description\":\"\",\"entry\":\"plugin.py\"}";
    expect(!plugin::parse_manifest(json, &m, &err),
           "api_version 1 rejected for python");
  }
  {
    plugin::Registry reg;
    plugin::Manifest m;
    m.id = "test.fixture";
    m.name = "F";
    m.version = "1.0.0";
    m.api_version = 2;
    m.kind = plugin::PluginKind::kBuiltin;
    expect(reg.add_manifest(m, plugin::TrustClass::kBuiltin), "add builtin");
    expect(reg.set_enabled("test.fixture", true, nullptr), "enable builtin");
    expect(reg.find("test.fixture")->state == plugin::PluginState::kEnabled,
           "enabled");
    expect(reg.set_enabled("test.fixture", false, nullptr), "disable");
    expect(reg.find("test.fixture")->state == plugin::PluginState::kDisabled,
           "disabled");
  }
  {
    plugin::Registry reg;
    plugin::Manifest m;
    m.id = "user.unsigned";
    m.name = "U";
    m.version = "1.0.0";
    m.api_version = 2;
    m.kind = plugin::PluginKind::kPython;
    m.entry = "plugin.py";
    expect(reg.add_manifest(m, plugin::TrustClass::kDenied), "add unsigned");
    expect(!reg.set_enabled("user.unsigned", true, nullptr),
           "unsigned without trust");
    expect(reg.trust_unsigned("user.unsigned"), "trust");
    expect(reg.set_enabled("user.unsigned", true, nullptr),
           "enable after trust");
  }
  {
    expect(std::string(plugin::legacy_id_from_stem("plugin_dem")) ==
               "smartgis.dem",
           "dem stem");
    expect(std::string(plugin::legacy_id_from_stem("FooBar")) == "legacy.foobar",
           "unknown stem");
    expect(std::string(plugin::legacy_id_from_display_name("DEM生成")) ==
               "smartgis.dem",
           "dem display");
  }
  if (g_fails) {
    std::fprintf(stderr, "plugin_host_test: %d fail(s)\n", g_fails);
    return 1;
  }
  std::puts("plugin_host_test: ok");
  return 0;
}
```

- [x] **Step 2: Run the test to verify it fails to compile / link**

Run: `ninja -C out plugin_host_test`  
Expected: FAIL — `plugin/manifest.h` not found (or GN target missing).

- [x] **Step 3: Implement headers and parse / registry / adapter**

`manifest.h` types (copy field list from the spec). `parse_manifest`:

- Recursive-descent in `plugin::detail` (no nlohmann / RapidJSON / Qt). Extra JSON keys ignored.
- `kind` map: `builtin` / `native` / `python` / `legacy_am`.
- Reject empty / invalid `id` (spec regex). Reject `api_version != 2` when `kind` is not `legacy_am`. Reject `kind=python` with empty `entry`. Reject duplicate contribution ids inside one manifest.
- `legacy_am` + `api_version==1` is allowed.

`Registry`:

```cpp
namespace plugin {

enum class PluginKind { kBuiltin, kNative, kPython, kLegacyAm };
enum class PluginState {
  kDisabled,
  kEnabled,
  kError,
  kInvalidExports,
  kLoadFailed
};
enum class TrustClass {
  kBuiltin,
  kSignedOfficial,
  kUnsignedTrusted,
  kDenied
};

class Registry {
 public:
  Registry();
  bool add_manifest(const Manifest& m, TrustClass trust);
  bool set_enabled(std::string_view id, bool enabled,
                   content::PluginHost* host);
  bool trust_unsigned(std::string_view id);
  bool unload(std::string_view id, content::PluginHost* host);
  const PluginRecord* find(std::string_view id) const;
  std::vector<PluginRecord> list() const;
  const std::string& last_error() const;
  bool register_builtin_hooks(std::string_view id,
                              bool (*start)(content::PluginHost*),
                              void (*stop)());
};

}  // namespace plugin
```

`add_manifest` fails on empty id or duplicate id. `trust_unsigned` fails on empty id or `kind==kBuiltin`. `set_enabled(true)` fails when trust is `kDenied` (`last_error` mentions unsigned). When `host` is nullptr, flip `state` only (no `plugin_start`); Task 2 wires start. `set_enabled(false)` sets `kDisabled` and, if `host` is non-null, calls `host->withdraw(id)`.

`legacy_am.cc` (this file may include leftover `src/base/pluginmanager.h`):

```cpp
const char* legacy_id_from_stem(std::string_view stem);
const char* legacy_id_from_display_name(std::string_view name);
bool scan_legacy_am(Registry* registry, const char* aux_module_dir);
```

Stem table (case-insensitive file stem without extension):

| stem | id |
| --- | --- |
| `SmtAMDemCreater` | `smartgis.dem` |
| `SmtAMMapProject` | `smartgis.proj` |
| `SmtAMMapPrint` | `smartgis.print` |
| `SmtAM3DModelCreater` | `smartgis.model3d` |
| `SmtAMBAOGridCreater` | `smartgis.baogrid` |

Unknown stem → `legacy.<lowercase_stem>` written into a static/thread-local buffer **or** returned via `std::string legacy_id_from_stem_string(...)`. Prefer `std::string` if the `const char*` buffer is awkward; then update the test to compare `std::string`. Display-name table: `DEM生成` → `smartgis.dem`, `三维对象` → `smartgis.model3d`, `边界适应正交网格` → `smartgis.baogrid`; unknown display → empty string.

`scan_legacy_am` calls leftover `SmtPluginManager::GetSingletonPtr()->LoadAllPlugin(aux_module_dir)` and `add_manifest` with `kind=legacy_am`, `api_version=1`, `TrustClass::kBuiltin` for mapped ids. Do not rewrite leftover headers.

GN (`src/plugin/BUILD.gn` keep leftover `smt_shared_library("plugin")` as-is):

```gn
source_set("host") {
  sources = [
    "legacy_am.cc",
    "legacy_am.h",
    "manifest.cc",
    "manifest.h",
    "registry.cc",
    "registry.h",
  ]
  include_dirs += [ "//src" ]
  deps = [ "//src/base:core" ]
}

test("plugin_host_test") {
  output_name = "plugin_host_test"
  sources = [ "host_test.cc" ]
  include_dirs += [ "//src" ]
  deps = [ ":host" ]
}
```

- [x] **Step 4: Run the test**

Run: `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 2: `content::PluginHost` + command contribution

**Files:**
- Create: `src/content/public/plugin_host.h`
- Create: `src/content/plugin_host.cc`
- Modify: `src/content/BUILD.gn` — add both files to `source_set("content")`
- Modify: `src/plugin/registry.cc` — when `host` is non-null, call builtin / python / native / leftover start
- Modify: `src/plugin/BUILD.gn` — `host` deps `//src/content:content` and `//src/tool:dispatch`
- Modify: `src/plugin/host_test.cc` — add host / withdraw / dialog / processing (sync) cases

**Interfaces:**
- Consumes: spec `PluginHost` / `MapContents`; `tool::CommandCatalog` / `CommandArgs` / `CommandHandler`; `content::EventBus`
- Produces: `content::create_plugin_host`, `Registry::set_enabled` starts plugins

- [x] **Step 1: Add `plugin_host.h` exactly as the spec API block** (`MenuContribution`, `DockContribution`, `DialogContribution`, `ProcessingContribution`, `MapContents`, `PluginHost`). Add:

```cpp
PluginHost* create_plugin_host(tool::CommandCatalog* catalog,
                               EventBus* events,
                               MapContents* maps);
```

Caller owns the returned host (`delete` after `Registry` is destroyed). `MapContents` may be nullptr in tests; `map_contents()` then returns nullptr.

- [x] **Step 2: Implement `plugin_host.cc`**

- Store contributions per `plugin_id`.
- `contribute_command`: `catalog->add(command_id, handler)` then record `{plugin_id, command_id}`. Empty id or failed `add` → false.
- `execute`: `tool::CommandDispatcher(catalog).execute(id, args)`.
- `open_dialog`: look up factory; missing → false; else invoke `factory(this)` and return true.
- `run_processing`: look up factory; missing → false; else **call factory on this thread** (Task 13 moves it off-thread). Empty factory → false.
- `withdraw(plugin_id)`: drop that plugin's commands from an internal overlay (re-`add` is not possible after catalog add — **do not erase from `CommandCatalog`**). Instead wrap handlers: `execute` returns false for withdrawn command ids. Keep a `withdrawn_` set. `contribute_command` after withdraw of the same id is allowed (re-enable). Simpler approach that matches tests: `PluginHost` owns a **side catalog** of plugin commands and `execute` checks host first then the leftover workspace catalog. Implement `execute` as: if command was contributed and not withdrawn, run it; if withdrawn, return false; else dispatch to the constructor `CommandCatalog*` (may be nullptr).

```cpp
bool PluginHostImpl::execute(std::string_view command_id,
                             const tool::CommandArgs& args) {
  if (withdrawn_commands_.count(std::string(command_id)))
    return false;
  auto it = handlers_.find(std::string(command_id));
  if (it == handlers_.end()) {
    if (!catalog_)
      return false;
    tool::CommandDispatcher disp(catalog_);
    return disp.execute(command_id, args);
  }
  return it->second(args);
}
```

`withdraw` moves that plugin's command ids into `withdrawn_commands_` and erases them from `handlers_`.

- [x] **Step 3: Registry start hooks**

When `set_enabled(id, true, host)` and `host` is non-null:

- `kBuiltin`: call `register_builtin_hooks` start; fail → `kError`.
- `kLegacyAm`: `LegacyAmAdapter` start (`SmtPlugin` `StartPlugin` by id); no `contribute_*` on leftover.
- `kPython` / `kNative`: return false with `last_error` `python not wired` / `native not wired` until Tasks 11 / 12. **Exception:** if hooks were registered via `register_builtin_hooks` for a python/native test fixture, call them.

When disabling with non-null host: `host->withdraw(id)` then stop hook.

- [x] **Step 4: Extend `host_test.cc`**

```cpp
  {
    content::EventBus bus;
    tool::CommandCatalog catalog;
    content::PluginHost* host =
        content::create_plugin_host(&catalog, &bus, nullptr);
    int n = 0;
    expect(host->contribute_command(
               "test.fixture", "test.ping", "Ping", "tools",
               [&](const tool::CommandArgs&) {
                 ++n;
                 return true;
               }),
           "contribute");
    expect(host->execute("test.ping", {}), "execute");
    expect(n == 1, "ran");
    host->withdraw("test.fixture");
    expect(!host->execute("test.ping", {}), "withdrawn");
    int opened = 0;
    expect(host->contribute_dialog(
               "test.fixture", {"test.dlg", "Dlg"},
               [&](content::PluginHost*) { ++opened; }),
           "dialog");
    expect(host->open_dialog("test.dlg"), "open");
    expect(opened == 1, "opened");
    expect(!host->open_dialog("missing.dlg"), "missing dialog");
    expect(!host->run_processing("missing.proc", "{}"), "missing proc");
    delete host;
  }
```

- [x] **Step 5: Build and run**

Run: `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 3: Views form controls + shared `MapPreviewView`

**Files:**
- Create: `src/ui/views/label.h` `.cc`, `button.h` `.cc`, `textfield.h` `.cc`, `checkbox.h` `.cc`, `radio_button.h` `.cc`, `combobox.h` `.cc`, `tab_strip.h` `.cc`, `table_view.h` `.cc`, `file_picker.h` `.cc`, `message_box.h` `.cc`
- Create: `src/plugin/widgets/BUILD.gn`
- Create: `src/plugin/widgets/map_preview.h` `.cc`
- Create: `src/plugin/widgets/about_dialog.h` `.cc`
- Modify: `src/ui/views/BUILD.gn` — add the new sources to `source_set("views")`

**Interfaces:**
- Consumes: `ui::views::View`, `MapViewport`
- Produces: toolkit controls + `plugin::MapPreviewView` + `plugin::AboutDialog`

- [x] **Step 1: Add toolkit controls as `ui::views::View` subclasses** (flat files, no `controls/` directory).

Minimum API (implement paint as fill + text via existing Skia canvas if available; otherwise empty `paint_self` is fine for v1 as long as getters/setters work):

```cpp
namespace ui {
namespace views {

class Label : public View {
 public:
  explicit Label(std::string text);
  void set_text(std::string text);
  const std::string& text() const;
};

class Button : public View {
 public:
  explicit Button(std::string text);
  void set_click(std::function<void()> fn);
  bool on_mouse_event(const MouseEvent& e) override;
};

class Textfield : public View {
 public:
  Textfield();
  void set_text(std::string text);
  const std::string& text() const;
};

class Checkbox : public View {
 public:
  explicit Checkbox(std::string label);
  void set_checked(bool on);
  bool is_checked() const;
};

class RadioButton : public View {
 public:
  RadioButton(std::string label, int group_id);
  void set_selected(bool on);
  bool is_selected() const;
  int group_id() const;
};

class Combobox : public View {
 public:
  void add_item(std::string item);
  void set_selected_index(int i);
  int selected_index() const;
  const std::string& selected_text() const;
};

class TabStrip : public View {
 public:
  int add_tab(std::string title, std::unique_ptr<View> page);
  void set_active(int i);
  int active() const;
};

class TableView : public View {
 public:
  void set_columns(const std::vector<std::string>& cols);
  void add_row(const std::vector<std::string>& cells);
  void clear_rows();
  size_t row_count() const;
};

struct FilePickerResult {
  bool accepted = false;
  std::string path;
};
FilePickerResult pick_open_file(const wchar_t* filter);
FilePickerResult pick_save_file(const wchar_t* filter);

enum class MessageBoxKind { kInfo, kError };
void show_message_box(MessageBoxKind kind, const std::string& text);

}  // namespace views
}  // namespace ui
```

`FilePicker` / `MessageBox` wrap Win32 `GetOpenFileNameW` / `MessageBoxW`. No `CFileDialog`. No Qt.

`TabStrip::add_tab` takes ownership of the page View. `set_active` hides inactive pages (`set_bounds` zero or skip paint).

- [x] **Step 2: Shared plugin widgets**

```cpp
namespace plugin {

class MapPreviewView : public ui::views::View {
 public:
  MapPreviewView();
  bool open_document(std::string_view path);  // empty path = current map
  ui::views::MapViewport* viewport();
};

class AboutDialog : public ui::views::View {
 public:
  explicit AboutDialog(std::string text);
};

}  // namespace plugin
```

`MapPreviewView` ctor `add_child` a `MapViewport`. `open_document` stores the path (print uses this type — do not add a second preview class). `AboutDialog` is a `Label` + OK `Button`.

`src/plugin/widgets/BUILD.gn`:

```gn
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

source_set("widgets") {
  sources = [
    "about_dialog.cc",
    "about_dialog.h",
    "map_preview.cc",
    "map_preview.h",
  ]
  include_dirs += [ "//src" ]
  public_deps = [ "//src/ui/views:views" ]
}
```

- [x] **Step 3: Compile**

Run: `ninja -C out //src/plugin/widgets:widgets //src/ui/views:views`  
Expected: ninja success (no `out/Default`).

Do not commit unless the user asks.

---

### Task 4: DEM Views dialogs (`smartgis.dem`)

**Files:**
- Create: `src/plugin/dem/tin_loader_dialog.h` `.cc`
- Create: `src/plugin/dem/grid_loader_dialog.h` `.cc`
- Create: `src/plugin/dem/dem_commands.h` `.cc`
- Modify: leftover `demcreater_plug.cpp` only if needed to keep compiling — do **not** replace leftover `dlg_*.h` as the public API
- Modify: `src/plugin/BUILD.gn` or `src/plugin/dem/BUILD.gn` — `source_set("dem_views")` (not a new DLL)

**Interfaces:**
- Consumes: Task 2 `PluginHost`, Task 3 controls, leftover `tin_loader.h` / `grid_loader.h` **only from processing factories**
- Produces: `plugin::register_dem(content::PluginHost*)`, dialogs `TinLoaderDialog`, `GridLoaderDialog`

- [x] **Step 1: Implement dialogs as Views.** Fields match leftover `CDlgTinLoader` / `CDlgGridLoader` (vertex path, separator radios, head/line skip, XYZ column comboboxes, scales, texture checkbox + path, generate-2D-TIN checkbox + layer combobox; grid: heightmap path, scales, starts, texture, color type). **OK** builds a JSON object string and calls `host->run_processing`. Do not `#include` `tin.h` / GEOS / PROJ from the dialog `.cc`.

About uses `plugin::AboutDialog`.

- [x] **Step 2: `register_dem`**

```cpp
namespace plugin {
bool register_dem(content::PluginHost* host);
}
```

Contributes commands `dem.load_tin`, `dem.load_grid`, `dem.about` (titles: leftover Chinese menu strings are OK in `title`); dialogs `dem.tin_loader`, `dem.grid_loader`, `dem.about`; processing `dem.tin_from_xyz`, `dem.grid_from_heightmap`. Command handlers only `open_dialog`. Processing factories call existing `Smt3DTinLoader` / `Smt3DGridLoader` (or `tin::read_xyz_points` when that symbol exists) and return bool.

- [x] **Step 3: Test in `host_test.cc`**

```cpp
  {
    content::PluginHost* host =
        content::create_plugin_host(nullptr, nullptr, nullptr);
    expect(plugin::register_dem(host), "register dem");
    expect(host->execute("dem.about", {}), "about command");
    delete host;
  }
```

`dem.about` factory must not crash (construct `AboutDialog`).

- [x] **Step 4: Build**

Run: `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 5: Projection Views dialogs (`smartgis.proj`)

**Files:**
- Create: `src/plugin/proj/map_prj_dialog.h` `.cc`
- Create: `src/plugin/proj/map_prj_xy_page.h` `.cc`
- Create: `src/plugin/proj/map_prj_grid_page.h` `.cc`
- Create: `src/plugin/proj/proj_commands.h` `.cc`

**Interfaces:**
- Consumes: `TabStrip`, `Textfield`, `Button`, `PluginHost`
- Produces: `plugin::register_proj(content::PluginHost*)`

- [x] **Step 1: `MapPrjDialog` hosts `TabStrip` with `MapPrjXyPage` and `MapPrjGridPage`.** Pages collect scale / XY or grid params (same fields as leftover `CDlgMapPrjDoXY` / `CDlgMapPrjDoGrid`). Apply calls `host->run_processing("proj.transform_xy" | "proj.transform_grid", args_json)`. No PROJ headers in the dialog `.cc`; the factory in `proj_commands.cc` calls `src/algorithm/proj`.

- [x] **Step 2: `register_proj`** contributes `proj.do_prj` → dialog `proj.dialog`, processing ids above.

- [x] **Step 3: Test** `register_proj` + `execute("proj.do_prj")` opens the factory (count++).

- [x] **Step 4: Build** `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 6: Print preview (`smartgis.print`)

**Files:**
- Create: `src/plugin/print/print_preview_dialog.h` `.cc`
- Create: `src/plugin/print/print_commands.h` `.cc`

**Interfaces:**
- Consumes: `plugin::MapPreviewView`, `ui::views::Button`
- Produces: `plugin::register_print(content::PluginHost*)`

- [x] **Step 1: `PrintPreviewDialog` is a shell: `MapPreviewView` + Save button.** Save uses `pick_save_file`. Do not copy `CDlg2DXView`. Do not add a second preview type.

- [x] **Step 2: `register_print`** contributes `print.preview` → dialog `print.preview`.

- [x] **Step 3: Test** `execute("print.preview")` invokes the factory.

- [x] **Step 4: Build** `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 7: Map service Views (`smartgis.map_service`)

**Cancelled 2026-09-13.** `plugin/map_service` and the web stack were deleted. Ignore the create-file steps below.

**Files:**
- Create: `src/plugin/map_service/service_mgr_dialog.h` `.cc`
- Create: `src/plugin/map_service/svr_cfg_base_page.h` `.cc`
- Create: `src/plugin/map_service/svr_cfg_tile_page.h` `.cc`
- Create: `src/plugin/map_service/map_client_dialog.h` `.cc`
- Create: `src/plugin/map_service/map_service_commands.h` `.cc`

**Interfaces:**
- Consumes: `TabStrip`, `MapPreviewView`, leftover map-service types **only from command / processing `.cc`**, not from page headers if that would pull MFC into Views chrome
- Produces: `plugin::register_map_service(content::PluginHost*)`

- [x] **Step 1: Port leftover fields.** `ServiceMgrDialog`: tree placeholder (`TableView` of service names) + `TabStrip` (`SvrCfgBasePage`, `SvrCfgTilePage`) + install/start/stop/uninstall/restart buttons that call host commands `map_service.install` … (handlers talk to leftover `SmtMapService` in `map_service_commands.cc`). Review Map / View Tiles / client canvas all construct `plugin::MapPreviewView` — **delete `CDlg2DXView` as a new type** (leftover file stays).

- [x] **Step 2: `register_map_service`** contributes `map_service.manage` → `map_service.mgr` dialog; `map_service.client` → `MapClientDialog`.

- [x] **Step 3: Test** `execute("map_service.manage")` factory count.

- [x] **Step 4: Build** `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 8: model3d commands (`smartgis.model3d`)

**Files:**
- Create: `src/plugin/model3d/model3d_commands.h` `.cc`

**Interfaces:**
- Consumes: `FilePicker`, `show_message_box`, `PluginHost`, leftover scene APIs only inside command / processing `.cc`
- Produces: `plugin::register_model3d(content::PluginHost*)`

Leftover has **no** custom `CDialog`. Extra UI is `CFileDialog` + `MessageBox` (verified in `model3d_creater_plugin.cpp`).

- [x] **Step 1: Contribute the nine leftover actions** as command ids:

| id | Leftover |
| --- | --- |
| `model3d.add_pointcloud` | `SMT_MSG_3DMODELCREATER_1` |
| `model3d.add_sphere` | `_2` |
| `model3d.add_water` | `_3` |
| `model3d.add_terrain_grid` | `_4` |
| `model3d.add_terrain_tin` | `_5` |
| `model3d.create_tin` | `_6` |
| `model3d.layer_points_to_3d` | `_7` |
| `model3d.layer_lines_to_3d` | `_8` |
| `model3d.layer_polygons_to_3d` | `_9` |

Point-cloud / file actions use `pick_open_file`. Errors use `show_message_box`. Scene mutations stay in `model3d_commands.cc` (or `run_processing` for TIN build `model3d.build_tin`). Do not add a new `CDialog`.

- [x] **Step 2: Test** `register_model3d` + `host` contains `model3d.add_sphere` (execute may fail without a scene; handler must return false, not crash).

- [x] **Step 3: Build** `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 9: baogrid commands (`smartgis.baogrid`)

**Files:**
- Create: `src/plugin/baogrid/baogrid_commands.h` `.cc`

**Interfaces:**
- Consumes: `tool::Workspace` / `edit.append.linestring` from the tool spec; `FilePicker`; `PluginHost`
- Produces: `plugin::register_baogrid(content::PluginHost*)`

Leftover has **no** custom `CDialog` (verified). Line input used leftover `GTT_InputLine`. New path: command `baogrid.input_boundary_0` / `baogrid.input_boundary_2` call `host->execute("edit.append.linestring", args)` when a workspace catalog is attached; if that command is missing, return false. `baogrid.load_boundary` uses `pick_open_file`. Grid build is processing `baogrid.create_orth_grid` (calls leftover `SmtBAOrthGrid` from the factory `.cc`, not from a dialog).

- [x] **Step 1: Implement `register_baogrid`** with the four command ids from the spec table.

- [x] **Step 2: Test** register + `execute("baogrid.save_boundary")` returns false (leftover body was empty) without crashing.

- [x] **Step 3: Build** `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 10: Plugin Manager Views UI

**Files:**
- Create: `src/plugin/manager_view.h`
- Create: `src/plugin/manager_view.cc`
- Modify: `src/app/views/main.cc` — host `ManagerView` as a child or a command `plugin.manager` (do not include leftover plugin headers)

**Interfaces:**
- Consumes: `Registry::list`, `set_enabled`, `trust_unsigned`, `Store` (Task 12; until then buttons that need Store call a `std::function` hook or hide)
- Produces: `plugin::ManagerView`

- [x] **Step 1: `ManagerView` is a `ui::views::View`**

```cpp
namespace plugin {
class ManagerView : public ui::views::View {
 public:
  ManagerView(Registry* registry, content::PluginHost* host);
  void refresh();
};
}
```

Table columns: id, name, version, kind, state, trust. Buttons: Enable, Disable, Trust unsigned. Install / Uninstall buttons may be no-op until Task 12 (`set_install_handler`). Error `Label` shows `registry->last_error()`.

- [x] **Step 2: Unit-test `refresh` via a Registry with one builtin** (no HWND required): after `refresh()`, `row_count()` on the internal table is 1. Expose `size_t plugin_row_count() const` for the test.

- [x] **Step 3: Build** `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 11: Python embed + bindings + sample plugin

**Files:**
- Create: `src/plugin/python/runtime.h` `.cc`
- Create: `src/plugin/python/bindings.cc`
- Create: `src/plugin/python/BUILD.gn`
- Create: `src/plugin/python/python_test.cc`
- Create: `src/plugin/samples/hello/plugin.json`
- Create: `src/plugin/samples/hello/plugin.py`
- Create: `third_party/python/README.md` (drop-in of official Windows embeddable 3.12)
- Modify: `src/plugin/registry.cc` — `kind=python` start/stop
- Modify: `BUILD.gn` — add `//src/plugin/python:plugin_python_test` to `test_all`

**Interfaces:**
- Consumes: CPython 3.12 C API, `PluginHost`
- Produces: `plugin::PythonRuntime`, module `smartgis`

- [x] **Step 1: Third-party drop-in.** `third_party/python/README.md` tells the implementer to unpack official `python-3.12.x-embed-amd64.zip` here (`python312.dll`, `python312.zip`). GN:

```gn
# src/plugin/python/BUILD.gn
source_set("python") {
  sources = [
    "bindings.cc",
    "runtime.cc",
    "runtime.h",
  ]
  include_dirs += [ "//src", "//third_party/python/include" ]
  deps = [ "//src/plugin:host" ]
  libs = [ "python312.lib" ]
  lib_dirs = [ "//third_party/python/libs" ]
}
```

If `python312.dll` is missing, **do not** fail GN configure: wrap the lib in `if (file_exists)` or keep `python` an optional target and make `plugin_python_test` compile a stub `main` that skips.

- [x] **Step 2: `PythonRuntime`**

```cpp
namespace plugin {
class PythonRuntime {
 public:
  bool init();
  void shutdown();
  bool start(std::string_view directory, std::string_view entry,
             content::PluginHost* host);
  void stop();
  bool is_ready() const;
};
}
```

`init` uses `Py_InitializeFromConfig` with isolated=1. `start` inserts `directory` on `sys.path`, `PyRun_SimpleFile(entry)`, then `PyObject_CallMethod` `start` with a capsule/host wrapper. Bindings (`bindings.cc`) create module `smartgis` with `content`, `tool`, `ui` submodules as in the spec. No Qt symbols.

- [x] **Step 3: Sample**

`src/plugin/samples/hello/plugin.json`:

```json
{
  "id": "smartgis.sample_hello",
  "name": "Hello Views",
  "version": "1.0.0",
  "api_version": 2,
  "kind": "python",
  "author": "The Mogu Authors",
  "description": "Sample in-process Python plugin",
  "entry": "plugin.py",
  "contributes": {
    "commands": [{"id": "sample.hello", "title": "Hello", "menu": "tools"}]
  }
}
```

`plugin.py`:

```python
def start(host):
    def _run(args):
        return True
    host.contribute_command("smartgis.sample_hello", "sample.hello", "Hello",
                            "tools", _run)

def stop():
    pass
```

Expose `contribute_command` on the Python host object so this works.

- [x] **Step 4: `python_test.cc`**

```cpp
int main() {
  plugin::PythonRuntime py;
  if (!py.init()) {
    std::puts("plugin_python_test: skip (no python)");
    return 0;
  }
  // write a temp plugin.py that contributes sample.hello, start, execute
  std::puts("plugin_python_test: ok");
  return 0;
}
```

A second case: `plugin.py` that `raise RuntimeError` in `start` → `start` returns false.

- [x] **Step 5: Registry** `kind=python` + enabled + host → `PythonRuntime::start(directory, entry, host)`.

- [x] **Step 6: Build**

Run: `ninja -C out plugin_python_test && out\plugin_python_test.exe`  
Expected: `plugin_python_test: ok` **or** `plugin_python_test: skip (no python)`

Do not commit unless the user asks.

---

### Task 12: Store (zip, HTTP index, ed25519, unsigned trust)

**Files:**
- Create: `third_party/ed25519/ed25519.h` `.c` `BUILD.gn` (SUPERCOP ref10 verify + sign)
- Create: `src/plugin/signature.h` `.cc`
- Create: `src/plugin/official_key.h`
- Create: `src/plugin/signature_test_key.h`
- Create: `src/plugin/store.h` `.cc`
- Modify: `src/plugin/BUILD.gn` — add sources to `host`; dep `//third_party/ed25519:ed25519`
- Modify: `src/plugin/host_test.cc` — hash / sig / zip / escape / uninstall cases
- Modify: `src/plugin/manager_view.cc` — wire Install from zip / index / Uninstall

**Interfaces:**
- Consumes: `net::HttpClient`, `parse_manifest`, `Registry`, spec index schema
- Produces: `plugin::SignatureVerifier`, `plugin::Store`

- [x] **Step 1: ed25519 + `SignatureVerifier`**

```cpp
namespace plugin {
struct SignatureVerifier {
  static bool sign(std::string_view zip_bytes, const uint8_t seed[32],
                   uint8_t sig64[64]);
  static bool verify(std::string_view zip_bytes, const uint8_t sig64[64],
                     const uint8_t pubkey32[32]);
  static std::string sha256_hex(std::string_view bytes);
};
}
```

Hash zip bytes with SHA-256 (implement in `signature.cc` — public-domain SHA-256,  one `.cc`, no OpenSSL). Sign/verify the **32-byte digest**. Generate a dev keypair once; put public in `official_key.h` as `kOfficialPublicKey[32]`, private seed in `signature_test_key.h` as `kTestPrivateSeed[32]`.

- [x] **Step 2: `Store`**

```cpp
namespace plugin {
class Store {
 public:
  Store(Registry* registry, std::string user_plugins_dir);
  bool refresh_index(const std::string& url);
  bool install_from_index(const std::string& id);
  bool install_zip(const std::string& zip_path, const std::string& sig_path,
                   bool signed_official);
  bool uninstall(const std::string& id);
  const std::string& last_error() const;
};
}
```

Zip: support ZIP `STORE` (no deflate required in v1). Reject entries whose cleaned path contains `..` or starts with `/` or `\`. If the zip has a single top-level directory, unwrap it. Require root `plugin.json` whose `id` matches the requested id (or the parsed id for `install_zip`). `signed_official` true → `verify` with `kOfficialPublicKey`; false → `add_manifest` with `kDenied` (enable needs `trust_unsigned`). HTTP index: `net::HttpClient::get`; parse spec JSON (`api_version` must be 1). `install_from_index` downloads zip and `.sig`, checks `sha256` hex (lowercase), then verify.

- [x] **Step 3: Tests** (append to `host_test.cc`)

- Known vector: sign `hello` zip bytes with `kTestPrivateSeed`, `verify` true; flip one sig byte → false.
- Write a STORE zip containing `plugin.json` for `user.zipplug` (`api_version` 2, `kind` python, `entry` plugin.py) + `plugin.py`. Install with matching sig → files under `user_plugins_dir/user.zipplug/`. Bad zip (`not-a-zip`) → false, no dir. Zip with `../evil.json` → `kBadZip` / false, no write outside dest.
- `uninstall` removes that directory.

Helper: write `write_store_zip(path, files)` in the test TU (local STORE writer is ~80 lines).

- [x] **Step 4: Build**

Run: `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 13: Processing worker isolation

**Files:**
- Create: `src/plugin/processing.h`
- Create: `src/plugin/processing.cc`
- Modify: `src/content/plugin_host.cc` — `run_processing` uses `ProcessingPool`
- Modify: `src/plugin/host_test.cc` — thread identity test
- Modify: `src/plugin/BUILD.gn` — add processing sources to `host`

**Interfaces:**
- Consumes: spec `ProcessingPool` / `ProcessingMode`
- Produces: off-UI-thread factories; `done` on the submitter thread via `flush_for_test`

- [x] **Step 1: Implement**

```cpp
namespace plugin {
enum class ProcessingMode { kThread, kUtilityStub };

class ProcessingPool {
 public:
  explicit ProcessingPool(ProcessingMode mode);
  ~ProcessingPool();
  ProcessingMode mode() const;
  bool submit(std::string processing_id, std::string args_json,
              content::ProcessingFactory factory,
              std::function<void(bool ok, std::string message)> done);
  void flush_for_test();  // wait worker + run posted done callbacks
};
}
```

v1: one worker thread. `kUtilityStub` uses the same thread and reports `mode()==kUtilityStub` (do not spawn `--type=utility`). `submit` false if `processing_id` empty or `factory` empty. Factory must not touch Views. `PluginHost::run_processing` calls `pool.submit` and returns true if queued; tests call `flush_for_test`.

Host constructor creates `ProcessingPool(ProcessingMode::kThread)`. Add `ProcessingPool* processing_pool()` on the impl for tests, or `create_plugin_host` overload that takes a pool.

- [x] **Step 2: Test**

```cpp
  {
    std::thread::id submitter = std::this_thread::get_id();
    std::thread::id worker{};
    plugin::ProcessingPool pool(plugin::ProcessingMode::kThread);
    bool done_ok = false;
    expect(pool.submit(
               "dem.tin_from_xyz", "{}",
               [&](content::PluginHost*, std::string_view) {
                 worker = std::this_thread::get_id();
                 return true;
               },
               [&](bool ok, std::string) { done_ok = ok; }),
           "submit");
    pool.flush_for_test();
    expect(worker != submitter, "factory off caller thread");
    expect(done_ok, "done true");
    plugin::ProcessingPool stub(plugin::ProcessingMode::kUtilityStub);
    expect(stub.mode() == plugin::ProcessingMode::kUtilityStub, "stub mode");
  }
```

`submit` needs a `PluginHost*` for the factory — pass nullptr into the factory from the pool (`factory(nullptr, args)`). Dialogs still go through `host->run_processing`, which binds `this`.

- [x] **Step 3: Build**

Run: `ninja -C out plugin_host_test && out\plugin_host_test.exe`  
Expected: `plugin_host_test: ok`

Do not commit unless the user asks.

---

### Task 14: Docs

**Files:**
- Modify: `src/README.md` — expand the `plugin/` bullet
- Modify: `README.md` — one clause if the directory / module table still implies plugin is only leftover domain DLLs; set **最后更新** to `2026-09-13`
- Modify: `docs/README.md` — index this spec and this plan; **最后更新** `2026-09-13`
- Modify: `docs/build/src-layout.md` — plugin row: host source_set + widgets + python + domain children

**Interfaces:**
- Consumes: shipped behavior of Tasks 1–13
- Produces: accurate docs

- [x] **Step 1: `src/README.md` plugin bullet** becomes: host `plugin::Registry` + leftover `SmtAuxModule`; chrome talks through `content::PluginHost`; Python embed; zip / `plugins.json` store. Spec: `docs/superpowers/specs/2026-09-13-plugin-host-design.md`. Keep **最后更新:** 2026-09-13.

- [x] **Step 2: Root `README.md`** — in the directory table or the Views/docs paragraph, add that `src/plugin` is the extension platform (Registry / store / Python), not only leftover `*.am`. Refresh **最后更新** to 2026-09-13.

- [x] **Step 3: `docs/README.md` index rows**

| 文档 | 内容 |
| --- | --- |
| [`superpowers/specs/2026-09-13-plugin-host-design.md`](superpowers/specs/2026-09-13-plugin-host-design.md) | 插件层：PluginHost / Registry / Views / Python / store |
| [`superpowers/plans/2026-09-13-plugin-host.md`](superpowers/plans/2026-09-13-plugin-host.md) | 实现计划：扩展平台（含 leftover `*.am` 适配） |

- [x] **Step 4: `docs/build/src-layout.md` plugin row** — host `//src/plugin:host` (not a new DLL); domain children keep `dll_stem`; `plugin/widgets` shared preview; `plugin/python` embed.

Do not commit unless the user asks.

---

## Self-review (plan vs spec)

| Spec section | Task |
| --- | --- |
| Manifest / Registry / leftover adapter | 1 |
| PluginHost + commands → `tool::Command` | 2 |
| Shared map-preview + form controls | 3 |
| DEM / proj / print / map_service / model3d / baogrid Views | 4–9 |
| Plugin Manager | 10 |
| Python embed + sample | 11 |
| Store + ed25519 + unsigned trust | 12 |
| Processing worker + utility stub | 13 |
| Docs | 14 |
| YAGNI / no Qt / no Mojo UI / no Registry in base | Global Constraints |

No TBD. Types match the spec (`contribute_command`, `run_processing`, `trust_unsigned`, `ProcessingMode::kUtilityStub`). Leftover `CDlg*` stay compiling; new path never instantiates them.
