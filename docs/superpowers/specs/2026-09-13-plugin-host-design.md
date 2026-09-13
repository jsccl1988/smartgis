<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin layer: host, contributions, Python, store

**Date:** 2026-09-13  
**Status:** accepted (user skipped remaining section gates; implement from this spec + the sibling plan)  
**Scope:** one implementation plan, one cycle. Land a QGIS-shaped extension platform: host + contribution points, in-process Python, QGIS-style store, Views rewrite of leftover plugin dialogs, and processing isolation for algorithm workers only. Do not implement product C++ in this document.

## Goal

`src/plugin` today is six MFC `*.am` DLLs plus a leftover loader (`SmtPluginManager` scans `aux module\*.am`, `LoadLibrary`, `GetPluginVersion` / `StartPlugin` / `StopPlugin`) and a leftover runtime (`SmtAuxModule` + `SmtAModuleManager` singleton, `long` msgs, `AppendFuncItems` into menus and `ui/xambox`). Chrome reaches plugins through `SmtApp::InitSmtAuxModules` → `GetAppPath() + "aux module\\"`.

The replacement is a full extension platform:

1. **`plugin::Registry`** — load, install, signature, enable/disable. App-scoped. Not a third `*Manager` singleton.
2. **`content::PluginHost`** — QgsInterface analogue on `content/public`. Chrome includes only `content/public`. Plugins contribute commands / menus / docks / dialogs / processing and get `MapContents` only through this.
3. **In-process Python** — CPython embed; bind `content`, `tool` commands, `ui::views`. No Qt / PyQt.
4. **QGIS-style store** — `plugin.json` + zip; local directory and HTTP `plugins.json` index; default allow signed or builtin; unsigned requires explicit trust.
5. **Views rewrite** of every leftover plugin `CDialog` this cycle. Shared map-preview replaces the two `CDlg2DXView` copies.
6. **Isolation only for processing / algorithm workers** — plugin UI never goes to a child process.

Commands already live on `tool::Command` (`docs/superpowers/specs/2026-09-13-tool-event-dispatch-design.md`). Document writes go through `sdb::EditSession`. Domain events go through `content::EventBus`. This spec does not reopen that split.

## Non-goals

- Do not put `plugin::Registry` in `src/base`.
- Do not put `EventBus` under `src/plugin`.
- Do not use Mojo / `--type=` for plugin UI.
- Do not put processing algorithms inside dialog classes.
- Do not rewrite leftover `Smt_*` headers (`plugin.h`, `module.h`, `dlg_*.h`) as the new public API. Keep leftover ABI / `dll_stem` compiling.
- Do not add a third `*Manager` singleton. Leftover `SmtPluginManager` / `SmtAModuleManager` stay as leftover; `SmtPluginManager` becomes an adapter target for `*.am` only.
- Do not introduce Qt, PyQt, PySide, SIP, or any Qt module.
- Do not vendor OSGi, VS Code contribution-only manifests as the only model, or Chromium extension processes for UI.
- Do not add marketplace accounts, ratings, reviews, payments, or user identity.
- Do not spawn a per-plugin process for UI.
- Do not change leftover `dll_stem` values (`SmtAuxModule`, `SmtAMDemCreater`, `SmtAMMapProject`, `SmtAMMapPrint`, `SmtAM3DModelCreater`, `SmtAMBAOGridCreater`). `SmtAMMapServiceMgr` / `plugin/map_service` were deleted with the web stack.

## Decisions (locked)

| Topic | Choice |
| --- | --- |
| OSS model | QGIS-shaped (in-process plugins + Plugin Manager + repo). Not VS Code contribution-only. Not Chromium Mojo for plugin UI. |
| Destination this cycle | Host + contribution points + in-process Python + QGIS-style store + processing isolation for workers only |
| Isolation | Leftover `*.am` and new C++/Python plugins load **in-process**. Background processing may use a worker thread or future `--type=utility`. Plugin UI never leaves the chrome process. |
| UI | Rewrite all leftover plugin `CDialog`s to `ui::views`. Python binds the same Views widgets. Qt banned. |
| Packages | `plugin.json` + zip. Local directory **and** HTTP index. |
| Index format | **JSON** `plugins.json` (not XML). Artifacts are zip files. |
| Signature | **ed25519 detached** `<zip>.sig` beside the zip (64-byte raw signature of the zip SHA-256 digest). Not minisign CLI. |
| Trust | Default allow **builtin** or **signed with a pinned official public key**. Unsigned requires `Registry::trust_unsigned(id)`. |
| Registry | `plugin::Registry` in `src/plugin`. App-scoped (owned by chrome). Not in `src/base`. |
| Host | `content::PluginHost` on `content/public`. Map/session chrome includes only `content/public`. Plugin Manager may include `plugin/registry.h`. |
| Commands | Contribute into existing `tool::CommandCatalog`. Same string-id rules as the tool spec. |
| Document writes | `sdb::EditSession` only for new code. |
| Domain events | `content::EventBus` (already specified). Not under `src/plugin`. |
| Tree | Host sources in `src/plugin/`. Domain plugins stay `plugin/dem`, `plugin/proj`, `plugin/print`, `plugin/model3d`, `plugin/baogrid`. Shared Views widgets in `src/plugin/widgets`. |
| Leftover loader | `SmtPluginManager` adapter for `*.am` only. Still `LoadLibrary` + the three exports. |
| Namespaces | Public C++ at most two levels (`plugin`, `content`). Helpers in `detail` or anonymous. New functions `snake_case`. C++23. Comments in English. |

## Architecture

```
Chrome (Views / leftover MFC)
  map/session: content/public only
  Plugin Manager: + plugin/registry.h
        │
        ▼
content::PluginHost          QgsInterface analogue
  MapContents                (no SmtMap* on this header)
  EventBus*                  (session-scoped; already specified)
  contribute_command ───────► tool::CommandCatalog / CommandDispatcher
  contribute_menu / dock / dialog
  run_processing ───────────► plugin::ProcessingPool
                                    │
                                    ├─ worker thread (v1)
                                    └─ --type=utility stub (same PE)
                                    ▼
                              src/algorithm/*  (tin / proj / baogrid / geo)
        ▲
        │ start / stop / contribute
plugin::Registry             app-scoped; not GetSingletonPtr()
  Manifest (plugin.json)
  Signature / trust
  Store (local dir + HTTP plugins.json + zip)
  LegacyAmAdapter ──► SmtPluginManager (*.am only)
        │
        ├─ builtin C++ (dem, proj, print, map_service, model3d, baogrid)
        ├─ leftover *.am (in-process LoadLibrary)
        └─ Python (in-process CPython; binds content / tool / ui::views)
```

Chrome map/session code includes only `content/public` (same rule as the tool spec). Plugin Manager may include `plugin/registry.h` and `plugin/manager_view.h`. Chrome and new plugins must not include leftover `plugin.h`, `module.h`, or `dlg_*.h`. New plugin code must not include `t_iatool.h`. Plugin UI is Views in the chrome process.

## Leftover inventory (verified)

Loader: `src/base/plugin.h` + `plugin.cpp` + `pluginmanager.cpp`. Scans `*.am`, `LoadLibrary`, resolves `GetPluginVersion` / `StartPlugin` / `StopPlugin`. Missing exports → treat as non-plugin, `FreeLibrary`, leftover `MessageBox`.

Runtime: `src/plugin/module.h` `SmtAuxModule` (a `SmtListener`) and `src/plugin/module_manager.h` `SmtAModuleManager` singleton. `AppendFuncItems` fills menus and `ui/xambox`. `long` msgs via `SMT_POST_AM_MSG`.

App load path: `SmtApp::InitSmtAuxModules` → `SmtPluginManager::GetSingletonPtr()->LoadAllPlugin(GetAppPath() + "aux module\\")`.

Five domain DLLs:

| Tree | `dll_stem` | Leftover UI | New command ids |
| --- | --- | --- | --- |
| `plugin/dem` | `SmtAMDemCreater` | `CDlgTinLoader`, `CDlgGridLoader`, `CDlgAbout` | `dem.load_tin`, `dem.load_grid`, `dem.about` |
| `plugin/proj` | `SmtAMMapProject` | `CDlgMapPrj` (tab host), `CDlgMapPrjDoXY`, `CDlgMapPrjDoGrid` | `proj.do_prj` |
| `plugin/print` | `SmtAMMapPrint` | `CDlg2DXView` | `print.preview` |
| `plugin/model3d` | `SmtAM3DModelCreater` | no custom `CDialog`; `CFileDialog` + `MessageBox` + scene mutations | `model3d.add_pointcloud` … `model3d.layer_polygons_to_3d` (nine commands, leftover `SMT_MSG_3DMODELCREATER_1`–`9`) |
| `plugin/baogrid` | `SmtAMBAOGridCreater` | no custom `CDialog`; `CFileDialog` + `MessageBox` + leftover IA line tool | `baogrid.input_boundary_0`, `baogrid.input_boundary_2`, `baogrid.save_boundary`, `baogrid.load_boundary` |

Leftover plugin `CDialog`s rewrite to `ui::views`. Shared `plugin::MapPreviewView` is the print preview canvas. model3d / baogrid extra UI is file picker + message box.

## Components

| Unit | Tree | Namespace | Role |
| --- | --- | --- | --- |
| `Manifest` | `src/plugin/manifest.h` | `plugin` | Parse / validate `plugin.json` |
| `Registry` | `src/plugin/registry.h` | `plugin` | Discover, verify, enable/disable, start/stop. Owned by chrome |
| `Signature` | `src/plugin/signature.h` | `plugin` | SHA-256 + ed25519 verify; trust store |
| `Store` | `src/plugin/store.h` | `plugin` | Local dir scan, zip install/uninstall, HTTP `plugins.json` |
| `LegacyAmAdapter` | `src/plugin/legacy_am.h` | `plugin` | Calls leftover `SmtPluginManager` for `*.am` only |
| `PluginHost` / `MapContents` | `src/content/public/plugin_host.h` | `content` | QgsInterface analogue; contribution points |
| Contribution points | same header | `content` | command, menu, dock, dialog, processing |
| Views form controls | `src/ui/views/*.h` (flat; no `controls/` nest) | `ui::views` | Label, Button, Textfield, Checkbox, RadioButton, Combobox, TabStrip, TableView, FilePicker, MessageBox |
| Shared map-preview | `src/plugin/widgets/map_preview.h` | `plugin` | Replaces both leftover `CDlg2DXView` |
| Domain Views dialogs | each `plugin/<domain>/` | `plugin` | One Views type per leftover dialog (see widget table) |
| Python runtime | `src/plugin/python/runtime.h` | `plugin` | Embed CPython 3.12 in-process |
| Bindings | `src/plugin/python/bindings.cc` | `plugin` | `smartgis.content` / `smartgis.tool` / `smartgis.ui` |
| Processing pool | `src/plugin/processing.h` | `plugin` | Worker thread v1; `--type=utility` stub |
| Plugin Manager UI | `src/plugin/manager_view.h` | `plugin` | Views list / enable / install / trust |
| Leftover DLL | `src/plugin/*.h` + domain `dlg_*.h` | `Smt_AM` / MFC | Unchanged exports; not the new API |

New host modules are **source_sets**, not new DLLs. Leftover `//src/plugin:plugin` (`SmtAuxModule`) and the six `smt_mfc_shared_library` targets stay. Tests: `plugin_host_test`, `plugin_python_test`. Public C++ stays two levels (`plugin`, `content`). Helpers in `plugin::detail` / `content::detail` or an anonymous namespace.

### Manifest (`plugin.json`)

Parse with a small recursive-descent reader in `plugin::detail` (`manifest.cc`). Do not vendor nlohmann, RapidJSON, or Qt JSON. Extra fields are ignored (forward-compatible). Unknown required-field types fail parse.

| Field | Type | Rules |
| --- | --- | --- |
| `id` | string | Reverse-DNS. `[a-z0-9]+(\.[a-z0-9_-]+)+`. Max 64 chars. Builtin ids are listed below. |
| `name` | string | Display name. Non-empty. Max 80 chars. |
| `version` | string | Semver `MAJOR.MINOR.PATCH` (no prerelease / build). |
| `api_version` | integer | Leftover `*.am` reports `1` via `GetPluginVersion`. **New plugins must be `2`.** Host accepts only `2` for `kind` other than `legacy_am`. |
| `kind` | string | Exactly one of: `builtin`, `native`, `python`, `legacy_am`. |
| `author` | string | May be empty. Max 80 chars. |
| `description` | string | May be empty. Max 400 chars. |

Optional fields (defaults in parentheses):

| Field | Type | Default / rules |
| --- | --- | --- |
| `homepage` | string | `""`. URL or empty. |
| `min_host_version` | string | `"1.0.0"`. Semver; host `1.0.0` this cycle. |
| `library` | string | Native / leftover file name (`SmtAMDemCreater.am`). Empty for `python` / `builtin`. |
| `entry` | string | Python entry module file (`plugin.py`). Required when `kind` is `python`; ignored otherwise. |
| `contributes` | object | See below. Default empty object. |

`contributes` object, all arrays optional and default `[]`:

```json
{
  "commands": [
    {"id": "dem.load_tin", "title": "TIN from XYZ", "menu": "tools.dem"}
  ],
  "menus": [
    {"id": "tools.dem", "title": "DEM", "parent": "tools"}
  ],
  "docks": [
    {"id": "map_service.catalog", "title": "Services", "area": "left"}
  ],
  "dialogs": [
    {"id": "dem.tin_loader", "title": "TIN loader"}
  ],
  "processing": [
    {"id": "dem.tin_from_xyz", "title": "TIN from XYZ"}
  ]
}
```

Contribution `id` rules match `tool::Command` ids: lowercase, dotted, non-empty. `menu` on a command is a menu id (not a path string). `parent` on a menu is another menu id or `"tools"` / `"file"` / `"view"` (host-provided roots). `area` on a dock is exactly `left`, `right`, `bottom`, or `float`. Duplicate contribution ids inside one manifest fail parse.

Builtin plugin ids (compiled in; `kind` is `builtin`; signature not required):

| id | Domain tree |
| --- | --- |
| `smartgis.dem` | `plugin/dem` |
| `smartgis.proj` | `plugin/proj` |
| `smartgis.print` | `plugin/print` |
| `smartgis.model3d` | `plugin/model3d` |
| `smartgis.baogrid` | `plugin/baogrid` |

Sample Python plugin id: `smartgis.sample_hello` (`kind` is `python`).

### Signature and trust

Scheme: **ed25519 detached signature**.

- Artifact: `name-version.zip`
- Digest: SHA-256 of the **entire zip file** (raw bytes).
- Signature file: `name-version.zip.sig` — exactly 64 bytes, raw ed25519 signature of that 32-byte digest (not hex, not minisign armored).
- Official public key: 32-byte raw key in `src/plugin/official_key.h` as `plugin::kOfficialPublicKey`. One key this cycle (dev/test key generated in the store task). The matching private key lives only in `src/plugin/signature_test_key.h` (test-only, not a production secret). `SignatureVerifier` always takes the pubkey as an argument; Store remote installs pass `kOfficialPublicKey`.
- Verifier: public-domain SUPERCOP **ref10** ed25519 in `third_party/ed25519` (verify + sign-for-tests only). `plugin::SignatureVerifier::verify(zip_bytes, sig64, pubkey32)` / `sign(...)`.

Trust classes:

| Class | When allowed |
| --- | --- |
| `kBuiltin` | Always (compiled-in ids above) |
| `kSignedOfficial` | `verify` succeeds with `kOfficialPublicKey` |
| `kUnsignedTrusted` | Caller previously invoked `Registry::trust_unsigned(id)` and the id is persisted |
| `kDenied` | Everything else (unsigned and not trusted; bad signature; unknown key) |

`Registry::trust_unsigned(id)` fails on empty id or builtin id (builtins are already allowed). Persistence file: `<user_data>/plugin_state.json` key `trusted_unsigned` (array of ids). Default allow is **builtin or signed official only**.

SHA-256 mismatch against the index `sha256` field (64 lowercase hex chars) is a store error, not a signature error.

### Store and index

Pick (locked): **JSON index named `plugins.json`** + zip artifacts. Not QGIS XML.

Local roots (scanned in this order; later duplicates of the same `id` lose):

1. `<app>/plugins/` (shipped / installed)
2. `<user_data>/plugins/` (user-installed zips, extracted one directory per id)
3. Leftover `<app>/aux module\*.am` via `LegacyAmAdapter` (ids mapped; see adapter)

HTTP: `Store::refresh_index(url)` uses `net::HttpClient::get`. Body must parse as:

```json
{
  "api_version": 1,
  "name": "SmartGIS official plugins",
  "plugins": [
    {
      "id": "smartgis.sample_hello",
      "name": "Hello Views",
      "version": "1.0.0",
      "api_version": 2,
      "kind": "python",
      "download_url": "https://example.invalid/plugins/hello-1.0.0.zip",
      "sha256": "64-lowercase-hex",
      "sig_url": "https://example.invalid/plugins/hello-1.0.0.zip.sig"
    }
  ]
}
```

Index `api_version` must be `1` this cycle. Missing `download_url` / `sha256` / `sig_url` on a remote entry is a parse error for that entry (other entries still load). `Store::install(id)` downloads zip then `.sig`, checks SHA-256, verifies ed25519, extracts to `<user_data>/plugins/<id>/` requiring a root `plugin.json` whose `id` matches.

Zip layout (no leading junk directory required; if the zip has a single top folder, unwrap it):

```
plugin.json
plugin.py            # kind=python
resources/           # optional
```

`kind=native` zips may include `library` as a DLL / `*.am`. `kind=builtin` is never installed from the store.

Uninstall deletes `<user_data>/plugins/<id>/` and disables the plugin. Builtin and leftover `*.am` cannot be uninstalled through Store (disable only).

No marketplace accounts, comments, or ratings.

### Leftover `*.am` adapter

`plugin::LegacyAmAdapter` is the only new code that includes leftover `src/base/plugin.h` / `pluginmanager.h`.

- `scan(const char* aux_module_dir)` calls `SmtPluginManager::GetSingletonPtr()->LoadAllPlugin(aux_module_dir)` (same path `SmtApp` uses: `GetAppPath() + "aux module\\"`).
- Maps leftover display names / file stems to builtin ids:

| Leftover stem or `SmtAuxModule` name | id |
| --- | --- |
| `SmtAMDemCreater` / `DEM生成` | `smartgis.dem` |
| `SmtAMMapProject` / projection plugin | `smartgis.proj` |
| `SmtAMMapPrint` | `smartgis.print` |
| `SmtAM3DModelCreater` / `三维对象` | `smartgis.model3d` |
| `SmtAMBAOGridCreater` / `边界适应正交网格` | `smartgis.baogrid` |

Unmapped `*.am` gets id `legacy.<stem_lower>` and `kind=legacy_am`, `api_version=1`.

- Enable leftover: `StartPlugin` if not already started.
- Disable leftover: `StopPlugin`. HMODULE stays loaded unless `Registry::unload` is called (then leftover `UnLoadPlugin`).
- Missing exports stay leftover behavior (do not start; record `PluginState::kInvalidExports`).
- Do not treat leftover `GetPluginVersion()==1` as a new-API plugin.

Leftover MFC `CDialog` sources remain in the six `smt_mfc_shared_library` targets so `dll_stem` and `SmartGis.exe` keep compiling. Views chrome never instantiates `CDlg*`. New public types are the Views classes below.

### `content::PluginHost`

QgsInterface analogue. Header: `src/content/public/plugin_host.h`. Implementation may live in `src/content/plugin_host.cc` and may use `plugin::` internally; **the public header must not include `src/plugin` or leftover headers**.

```cpp
namespace content {

struct MenuContribution {
  std::string id;
  std::string title;
  std::string parent;  // "tools" / "file" / "view" / another menu id
};

struct DockContribution {
  std::string id;
  std::string title;
  std::string area;  // left | right | bottom | float
};

struct DialogContribution {
  std::string id;
  std::string title;
};

struct ProcessingContribution {
  std::string id;
  std::string title;
};

class MapContents {
 public:
  virtual ~MapContents() = default;
  virtual uint32_t active_view_id() const = 0;
  virtual Extent2 extent() const = 0;
  virtual void set_extent(const Extent2& e) = 0;
};

using DialogFactory = std::function<void(PluginHost*)>;
using ProcessingFactory =
    std::function<bool(PluginHost*, std::string_view args_json)>;

class PluginHost {
 public:
  virtual ~PluginHost() = default;

  virtual MapContents* map_contents() = 0;
  virtual EventBus* events() = 0;
  virtual tool::CommandCatalog* commands() = 0;

  virtual bool contribute_command(std::string_view plugin_id,
                                  std::string_view command_id,
                                  std::string_view title,
                                  std::string_view menu_id,
                                  tool::CommandHandler handler) = 0;
  virtual bool contribute_menu(std::string_view plugin_id,
                               const MenuContribution& menu) = 0;
  virtual bool contribute_dock(std::string_view plugin_id,
                               const DockContribution& dock,
                               DialogFactory factory) = 0;
  virtual bool contribute_dialog(std::string_view plugin_id,
                                 const DialogContribution& dialog,
                                 DialogFactory factory) = 0;
  virtual bool contribute_processing(std::string_view plugin_id,
                                     const ProcessingContribution& proc,
                                     ProcessingFactory factory) = 0;

  virtual bool execute(std::string_view command_id,
                       const tool::CommandArgs& args) = 0;
  virtual bool open_dialog(std::string_view dialog_id) = 0;
  virtual bool run_processing(std::string_view processing_id,
                              std::string_view args_json) = 0;

  virtual void withdraw(std::string_view plugin_id) = 0;
};

}  // namespace content
```

`contribute_command` calls `commands()->add(command_id, handler)` and records the menu placement. Duplicate command ids fail (false), matching `CommandCatalog::add`. `execute` is `CommandDispatcher::execute`. `withdraw` removes that plugin's commands / menus / docks / dialogs / processing and does not touch other plugins.

`MapContents` is the only map face plugins get on the new path. It wraps the existing `content::MapSession` (do not rename `MapSession` in this cycle). No `SmtMap*`, HWND, or `LPRENDERDEVICE` on this header.

`PluginHost` is constructed by chrome per app (one host). It is not a singleton accessor.

### Contribution points

| Point | Registers | Runs on |
| --- | --- | --- |
| command | `tool::Command` id + handler | chrome / renderer main thread via `CommandDispatcher` |
| menu | menu id + parent + title; commands reference it | chrome Views menu |
| dock | id + area + factory | chrome; factory builds a `ui::views::View` |
| dialog | id + factory | chrome; factory opens a Views `Widget` |
| processing | id + factory | **worker thread** (v1); completion posted back to main |

A command that only opens a dialog calls `host->open_dialog`. A command that mutates the document calls `sdb::EditSession` (via chrome-owned session), then `events()->publish`. A command that runs analysis calls `host->run_processing`. Dialog factories must not call algorithm kernels.

### Views widget set

`src/ui/views` stays a **single module**. Add form-control headers next to `view.h` (no `src/ui/views/controls/` nest — `ui-views-skia.md` forbids extra public nests).

| Control | File | Role |
| --- | --- | --- |
| `Label` | `src/ui/views/label.h` | Static text |
| `Button` | `src/ui/views/button.h` | Click → `std::function<void()>` |
| `Textfield` | `src/ui/views/textfield.h` | Single-line text / number |
| `Checkbox` | `src/ui/views/checkbox.h` | Bool |
| `RadioButton` | `src/ui/views/radio_button.h` | Exclusive group |
| `Combobox` | `src/ui/views/combobox.h` | String list |
| `TabStrip` | `src/ui/views/tab_strip.h` | Tab host (proj + map_service) |
| `TableView` | `src/ui/views/table_view.h` | Column preview (TIN XYZ sample) |
| `FilePicker` | `src/ui/views/file_picker.h` | Native open/save; not a `CFileDialog` subclass |
| `MessageBox` | `src/ui/views/message_box.h` | Modal info / error; replaces leftover `AfxMessageBox` |

Shared plugin widgets (`src/plugin/widgets`, namespace `plugin`):

| Type | File | Replaces |
| --- | --- | --- |
| `MapPreviewView` | `widgets/map_preview.h` | print `CDlg2DXView` **and** map_service `CDlg2DXView` |
| `AboutDialog` | `widgets/about_dialog.h` | dem `CDlgAbout` (reusable) |

`MapPreviewView` **owns** a `ui::views::MapViewport` child and optional Save. Print and map_service both construct this type; they do not copy a second preview class. map_service may pass a map-document path into `MapPreviewView::open_document(path)`.

Domain Views types (new files; leftover `dlg_*.h` stay leftover):

| Leftover | New Views type | File | Shared vs per-plugin |
| --- | --- | --- | --- |
| `CDlgTinLoader` | `TinLoaderDialog` | `plugin/dem/tin_loader_dialog.h` | per-plugin; uses TableView, Combobox, FilePicker, Checkbox |
| `CDlgGridLoader` | `GridLoaderDialog` | `plugin/dem/grid_loader_dialog.h` | per-plugin |
| `CDlgAbout` | `plugin::AboutDialog` | `plugin/widgets/about_dialog.h` | **shared** |
| `CDlgMapPrj` | `MapPrjDialog` | `plugin/proj/map_prj_dialog.h` | per-plugin tab host |
| `CDlgMapPrjDoXY` | `MapPrjXyPage` | `plugin/proj/map_prj_xy_page.h` | per-plugin tab page |
| `CDlgMapPrjDoGrid` | `MapPrjGridPage` | `plugin/proj/map_prj_grid_page.h` | per-plugin tab page |
| `CDlg2DXView` (print) | `PrintPreviewDialog` | `plugin/print/print_preview_dialog.h` | per-plugin **shell**; preview child is shared `MapPreviewView` |
| model3d `CFileDialog` / `MessageBox` | `FilePicker` + `MessageBox` | toolkit | shared |
| baogrid `CFileDialog` / `MessageBox` | `FilePicker` + `MessageBox` | toolkit | shared |

TIN / grid / projection / BAO-grid **kernels** stay in `src/algorithm/{tin,proj,baogrid}` and leftover loaders (`tin_loader.cpp`, `grid_loader.cpp`) as callable functions. Dialogs collect params and call `PluginHost::run_processing`.

### Python runtime and bindings

- Embed **CPython 3.12** (Windows embeddable package) via `third_party/manifest.json` key `python`. GN `//third_party:python` copies `python312.dll` and the stdlib zip next to `out/`.
- `plugin::PythonRuntime` lives in the **chrome process**. `Py_InitializeFromConfig` with `isolated=1`. One interpreter this cycle (no sub-interpreters).
- `kind=python` start: `PyRun_SimpleFile` on `entry` (`plugin.py`) then call `start(host)` if defined. Stop: call `stop()` if defined, then drop the module.
- Bindings module name: `smartgis`. Surfaces:

| Python | C++ |
| --- | --- |
| `smartgis.content.host` | the `PluginHost` passed to `start` |
| `smartgis.content.events.subscribe(name, fn)` | `EventBus::subscribe` for `SelectionChanged` / `ExtentChanged` |
| `smartgis.tool.execute(id, view_id=0, payload="")` | `PluginHost::execute` |
| `smartgis.ui.Label` / `Button` / `Textfield` / `Checkbox` / `Widget` | matching `ui::views` types |

No `smartgis.qt`. No PyQt. No `sip`. Missing Views binding → `contribute_dialog` / Python UI call returns an error object; the plugin stays loaded; the dialog does not open.

Python exceptions during `start` / command handlers: catch at the C boundary, log, return false from the command; do not abort chrome.

Sample plugin (shipped, builtin-trust as a fixture under `src/plugin/samples/hello/`):

```
plugin.json   id=smartgis.sample_hello, kind=python, entry=plugin.py
plugin.py     start(host) contributes command sample.hello and a Views dialog
```

### Processing isolation boundary

| Work | Where |
| --- | --- |
| Plugin UI (Views, Python widgets, leftover `CDialog` in MFC exe) | Chrome process, main thread |
| Command handlers that only open UI or dispatch tools | Main thread |
| TIN / grid / projection / BAO-grid kernels | `plugin::ProcessingPool` worker thread (v1) |
| Future heavy jobs | Same PE `--type=utility` (`content::ProcessType::kUtility` already exists). Stub: `ProcessingPool` can be constructed with `kThread` or `kUtilityStub` (stub still runs in-process on a worker thread and records the chosen mode). No real child process required this cycle. |

```cpp
namespace plugin {

enum class ProcessingMode { kThread, kUtilityStub };

class ProcessingPool {
 public:
  explicit ProcessingPool(ProcessingMode mode);
  bool submit(std::string processing_id, std::string args_json,
              ProcessingFactory factory,
              std::function<void(bool ok, std::string message)> done);
};

}  // namespace plugin
```

`submit` returns false if `processing_id` is empty or `factory` is empty. `done` is invoked on the **main thread** (chrome posts back). The factory runs off the UI thread and must not touch Views or leftover `CDialog`. Dialog classes call `host->run_processing` only.

v1 pool size is **1** worker (deterministic tests). `kUtilityStub` is the same thread implementation plus `mode() == kUtilityStub` so a later agent can hang a real `--type=utility` without changing callers.

### Plugin Manager Views UI

`plugin::ManagerView` (`src/plugin/manager_view.h`) is a `ui::views::View`:

- Table of installed plugins: id, name, version, kind, state (`enabled` / `disabled` / `error`), trust class.
- Buttons: Enable, Disable, Install from zip, Install from index, Trust unsigned, Uninstall (user-installed only).
- Error line: last `Registry` error string (bad zip, failed signature, API mismatch, Python exception).

Chrome (`src/app/views`) hosts this view. It includes `content/public/plugin_host.h` and a chrome-owned `plugin::Registry`; it does not include leftover plugin headers.

## Data flow

**Install (zip, signed)**

1. User picks a zip in Plugin Manager, or Store downloads zip + `.sig` from `plugins.json`.
2. Store hashes zip; mismatch with index `sha256` → `kBadHash`; stop.
3. `SignatureVerifier::verify` with `kOfficialPublicKey`; fail → `kBadSignature`; stop.
4. Extract to `<user_data>/plugins/<id>/`; parse `plugin.json`; `id` mismatch → delete extract, `kBadManifest`.
5. Registry records the plugin as disabled until enable.

**Install (unsigned)**

1. Same extract + parse.
2. Trust class is `kDenied` until `trust_unsigned(id)`.
3. Enable while `kDenied` returns false (`kUnsignedNotTrusted`).

**Enable → start → contribute**

1. `Registry::set_enabled(id, true)` checks trust + `api_version`.
2. `kind=builtin`: call the compiled `plugin_start(PluginHost*)` for that id.
3. `kind=python`: `PythonRuntime::start(dir, entry, host)`.
4. `kind=legacy_am`: `LegacyAmAdapter::start(id)` → leftover `StartPlugin` (still `AppendFuncItems` for MFC).
5. `kind=native`: `LoadLibrary` of `library`, resolve `plugin_start` (`bool plugin_start(content::PluginHost*)`). Missing symbol → `kInvalidExports`.
6. `plugin_start` / Python `start` call `contribute_*`. Commands land in `tool::CommandCatalog`.

**Command / dialog**

1. Chrome menu or Plugin Manager runs `host->execute("dem.load_tin")`.
2. Handler calls `host->open_dialog("dem.tin_loader")`.
3. Dialog factory builds `TinLoaderDialog` (Views). OK collects JSON args and `host->run_processing("dem.tin_from_xyz", args)`.
4. Worker runs `tin::` / leftover loader. `done` on main thread. Success: chrome commits via `sdb::EditSession` if a feature was created, then `EventBus` (for example `ExtentChanged`). The dialog does not write `SmtMap`.

**Disable / unload**

1. `set_enabled(id, false)` → `host->withdraw(id)` then kind-specific stop (`plugin_stop` / Python `stop` / leftover `StopPlugin`).
2. `unload` additionally `FreeLibrary` leftover / native, or drop the Python module. Builtin code stays linked.

## Error handling

| Case | Result |
| --- | --- |
| Bad zip (not zip, truncated, path escape `..`) | `Store::install` false; `kBadZip`; nothing left under `<user_data>/plugins/<id>/` |
| SHA-256 mismatch | `kBadHash`; zip not extracted |
| Failed ed25519 / missing `.sig` on a remote install | `kBadSignature`; not installed |
| Unsigned local zip / dir without prior `trust_unsigned` | Installed files may exist; `set_enabled` false; `kUnsignedNotTrusted` |
| `plugin.json` parse / missing required field / bad `id` | `kBadManifest`; extract removed |
| `api_version` ≠ 2 for non-`legacy_am` | `kApiMismatch`; not started |
| `legacy_am` `GetPluginVersion` missing or `StartPlugin` / `StopPlugin` missing | `kInvalidExports`; leftover already `FreeLibrary`s |
| Duplicate `plugin.json` `id` already enabled | Second start refused; first kept |
| Duplicate `contribute_command` id | `contribute_command` false; first handler kept |
| Missing Views binding (Python opens unknown widget) | Python exception → command false; plugin stays enabled |
| Python exception in `start` | Plugin state `kError`; not enabled |
| Python exception in a command | Command returns false; plugin stays enabled |
| `run_processing` unknown id | false; no worker work |
| Processing factory throws / returns false | `done(false, message)` on main; UI unchanged |
| HTTP index fetch fail | `refresh_index` false; previously cached index kept |
| Leftover `LoadLibrary` fail | Adapter records `kLoadFailed`; other plugins continue |

Registry last-error is a single UTF-8 string `Registry::last_error()` for the Manager UI. No leftover `MessageBox` on the new path.

## Testing

`testing/test.gni` `test()` + `expect` / `main` like `tool_dispatch_test`. Output only under repo-root `out/`.

`//src/plugin:plugin_host_test` (added to `//:test_all`) must cover:

- Manifest: happy `plugin.json`; missing `id`; `api_version` 1 rejected for `kind=python`; duplicate contribute id rejected.
- Registry: enable builtin fixture; disable withdraws commands (`catalog.contains` false); unsigned without trust refuses enable.
- Signature: known vector verifies; flipped byte fails; official key mismatch fails.
- Store: install a tiny zip with matching sha256 + valid sig; bad zip; path-escape zip rejected; uninstall removes the directory.
- Leftover adapter: a fixture DLL is **not** required; map table maps `SmtAMDemCreater` → `smartgis.dem`; unknown stem → `legacy.foo`.
- PluginHost: contribute command then `execute` runs it; `withdraw` removes it; `open_dialog` invokes the factory; `run_processing` invokes factory off the calling thread (see processing test).
- Processing: factory does not run on the `submit` caller thread; `done` runs on the caller thread when the test pumps `ProcessingPool::flush_for_test`.

`//src/plugin/python:plugin_python_test`:

- If `python312.dll` is absent, print `plugin_python_test: skip (no python)` and exit 0.
- If present: embed, `start` a sample `plugin.py` that contributes `sample.hello`, `execute` returns true; a `plugin.py` that raises in `start` leaves the plugin in `kError`.

No gtest.

## File / tree map

| Path | Responsibility |
| --- | --- |
| `src/content/public/plugin_host.h` | `PluginHost`, `MapContents`, contribution structs |
| `src/content/plugin_host.cc` | Default `PluginHost` implementation |
| `src/plugin/manifest.h` `.cc` | `plugin.json` |
| `src/plugin/registry.h` `.cc` | enable / disable / start / stop |
| `src/plugin/signature.h` `.cc` | SHA-256 + ed25519 |
| `src/plugin/official_key.h` | pinned 32-byte official public key |
| `src/plugin/store.h` `.cc` | local + HTTP index + zip |
| `src/plugin/legacy_am.h` `.cc` | `*.am` adapter |
| `src/plugin/processing.h` `.cc` | worker pool |
| `src/plugin/manager_view.h` `.cc` | Plugin Manager Views |
| `src/plugin/widgets/map_preview.h` `.cc` | shared preview |
| `src/plugin/widgets/about_dialog.h` `.cc` | shared about |
| `src/plugin/python/runtime.h` `.cc` | CPython embed |
| `src/plugin/python/bindings.cc` | `smartgis.*` |
| `src/plugin/samples/hello/` | sample Python plugin |
| `src/plugin/dem/*_dialog.*` | DEM Views |
| `src/plugin/proj/map_prj_*` | projection Views |
| `src/plugin/print/print_preview_dialog.*` | print shell + shared preview |
| `src/plugin/model3d/model3d_commands.*` | command handlers (no leftover CDialog) |
| `src/plugin/baogrid/baogrid_commands.*` | commands + processing |
| `src/ui/views/{label,button,textfield,checkbox,radio_button,combobox,tab_strip,table_view,file_picker,message_box}.*` | toolkit controls |
| leftover `src/base/plugin*.`, `src/plugin/module*.`, domain `dlg_*.h` | unchanged ABI |
| `third_party/ed25519/` | verify-only ed25519 |
| `third_party` Python embeddable | CPython 3.12 |

Includes: `"content/public/plugin_host.h"`, `"plugin/registry.h"`, `"plugin/widgets/map_preview.h"`, `"ui/views/button.h"`.

## Industry mapping

| QGIS | ArcGIS Pro | This repo |
| --- | --- | --- |
| `QgsInterface` | `IApplication` / `IPlugin` host | `content::PluginHost` |
| `metadata.txt` + zip | add-in XML / `.esriaddin` | `plugin.json` + zip |
| Plugin repo XML | ESRI / org portal | HTTP `plugins.json` + local dir |
| In-process Python + PyQGIS | ArcPy in-process | CPython embed + `smartgis.*` |
| PyQt widgets | Qt / WinUI add-in UI | `ui::views` (Qt banned) |
| `QAction` | `ICommand` / `Button` | `tool::Command` |
| `QgsMapCanvas` in a dialog | `MapView` embed | `plugin::MapPreviewView` → `ui::views::MapViewport` |
| Processing Toolbox | Geoprocessing | `contribute_processing` + `src/algorithm/*` |
| Plugin Manager | Add-In Manager | `plugin::ManagerView` |
| `iface.mapCanvas()` | `MapView` from host | `PluginHost::map_contents()` |
| Separate plugin process | rare | **not used for UI**; workers only |

## Processing isolation (summary)

Plugin UI is in-process, like QGIS. The only isolation boundary is **algorithm work**: `ProcessingPool` on a worker thread this cycle, with a `--type=utility` stub (`content::ProcessType::kUtility`) that does not yet spawn a child. Dialogs never call `tin::` / PROJ / BAO-grid kernels directly.

## YAGNI

- No marketplace accounts, reviews, ratings, payments, or comments.
- No OSGi / Eclipse bundle runtime.
- No per-plugin processes for UI.
- No minisign CLI, no XML plugin repo, no VS Code `package.json` contribution-only host.
- No Mojo pipes for dialogs.
- No second Python (conda / venv per plugin).
- No plugin signing CA / certificate chain — one pinned ed25519 official key.
- No live-reload / hot-swap beyond disable → enable.
- No third public namespace.

## Docs (same change set as implementation)

- `src/README.md` — expand the `plugin/` bullet to host + Registry + PluginHost + Python + store; point at this spec.
- Root `README.md` — if the module / directory table still implies plugin is only leftover domain DLLs, add one clause; refresh **最后更新** to 2026-09-13.
- `docs/README.md` — index this spec and the implementation plan.
- `docs/build/src-layout.md` — plugin row: host source_set + domain children + widgets + python.

## Out of this cycle

- Real `--type=utility` child with a Mojo pipe for job bytes (stub only here).
- Retiring leftover `CDlg*` from the MFC `dll_stem` graphs (they stay compiling).
- Multi-interpreter Python, pip install into the embed, or third-party Python GIS stacks.
- Plugin sandbox (seccomp / job object) for native code.
- Renaming `content::MapSession` to `MapContents` globally (this spec adds `MapContents` as the plugin face only).
