<!--
Copyright (c) 2026 The Mogu Authors.
All rights reserved.
-->

# Plugin layer: host, contributions, Python, store

**Status:** accepted  
**Date:** 2026-09-13  
**Updated:** 2026-10-04 — world3d top-level: `grid/` holds DEM + orthogrid + hexgrid; `scene/` stays; `register_world3d_grid`. Prior: sink inner `loader/dialog` / lattice/session/solve. Prior: tighten flattened `dem/views`/`dem/resources`. Prior: domain subdirs (`dem/` `scene/` `orthogrid/` `hexgrid/`; no root `scene_commands.cc` / `processing/` / `views/`). Prior: command stack layered (`register_world3d` façade + `detail::contribute_*` aliases). Prior: product `orthogrid` + `orthogrid3d` merged into `smartgis.world3d`; leftover `plugin_orthogrid` AM stem maps to `smartgis.world3d`. Prior: world3d DEM loaders renamed (`heightmap` / `trimesh`); orthogrid3d ABI: `geo::HexGrid` deleted from gis.dll; 3D structured grid is `OGRMultiPoint` XYZ + nx/ny/nz owned by `world3d/grid/hexgrid` (`HexLattice` non-exported). Prior 2026-10-03 — v1 host checklist archived; follow-on product plugins (world3d / stormsurge / analysis) still open. Prior 2026-10-02 — world3d True Earth P0b (global DEM / satellite / atmosphere); geochem; mine/stratum; traffic+flood; `runtime/host` role subdirs; leftover `legacy/plugin` layout; product Python / gis analysis ownership.  
**Plans:** v1 [`../archive/plans/2026-09-13-plugin-host.md`](../archive/plans/2026-09-13-plugin-host.md) (landed) · leftover plugin [`../plans/2026-09-29-legacy-plugin-subdirectory-layout.md`](../plans/2026-09-29-legacy-plugin-subdirectory-layout.md) · stormsurge [`../plans/2026-09-30-stormsurge-3d-disaster.md`](../plans/2026-09-30-stormsurge-3d-disaster.md) · world3d / analysis on later § Plan lines  
**Diagram:** [`../diagrams/plugin-product-world3d.html`](../diagrams/plugin-product-world3d.html)  
**Scope:** one implementation plan, one cycle. Land a QGIS-shaped extension platform: host + contribution points, in-process Python, QGIS-style store, Views rewrite of leftover plugin dialogs, and processing isolation for algorithm workers only. Do not implement product C++ in this document.

## Goal

`src/plugin` today is six MFC `*.am` DLLs plus a leftover loader (`SmtPluginManager` scans `aux module\*.am`, `LoadLibrary`, `GetPluginVersion` / `StartPlugin` / `StopPlugin`) and a leftover runtime (`SmtAuxModule` + `SmtAModuleManager` singleton, `long` msgs, `AppendFuncItems` into menus and `ui/xambox`). Chrome reaches plugins through `App::InitSmtAuxModules` → `GetAppPath() + "aux module\\"`.

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
        ��
        ��
content::PluginHost          QgsInterface analogue
  MapContents                (no Map* on this header)
  EventBus*                  (session-scoped; already specified)
  contribute_command ��������������? tool::CommandCatalog / CommandDispatcher
  contribute_menu / dock / dialog
  run_processing ����������������������? plugin::ProcessingPool
                                    ��
                                    ���� worker thread (v1)
                                    ���� --type=utility stub (same PE)
                                    ��
                              src/algorithm/*  (tin / proj / baogrid / geo)
        ��
        �� start / stop / contribute
plugin::Registry             app-scoped; not GetSingletonPtr()
  Manifest (plugin.json)
  Signature / trust
  Store (local dir + HTTP plugins.json + zip)
  LegacyAmAdapter ����? SmtPluginManager (*.am only)
        ��
        ���� builtin C++ (dem, proj, print, map_service, model3d, baogrid)
        ���� leftover *.am (in-process LoadLibrary)
        ���� Python (in-process CPython; binds content / tool / ui::views)
```

Chrome map/session code includes only `content/public` (same rule as the tool spec). Plugin Manager may include `plugin/registry.h` and `plugin/manager_view.h`. Chrome and new plugins must not include leftover `plugin.h`, `module.h`, or `dlg_*.h`. New plugin code must not include `t_iatool.h`. Plugin UI is Views in the chrome process.

## Leftover inventory (verified)

Loader: `src/base/plugin.h` + `plugin.cpp` + `pluginmanager.cpp`. Scans `*.am`, `LoadLibrary`, resolves `GetPluginVersion` / `StartPlugin` / `StopPlugin`. Missing exports �� treat as non-plugin, `FreeLibrary`, leftover `MessageBox`.

Runtime: `src/plugin/module.h` `SmtAuxModule` (a `SmtListener`) and `src/plugin/module_manager.h` `SmtAModuleManager` singleton. `AppendFuncItems` fills menus and `ui/xambox`. `long` msgs via `POST_AM_MSG`.

App load path: `App::InitSmtAuxModules` �� `SmtPluginManager::GetSingletonPtr()->LoadAllPlugin(GetAppPath() + "aux module\\")`.

Five domain DLLs:

| Tree | `dll_stem` | Leftover UI | New command ids |
| --- | --- | --- | --- |
| `plugin/dem` | `SmtAMDemCreater` | `CDlgTinLoader`, `CDlgGridLoader`, `CDlgAbout` | `dem.load_tin`, `dem.load_grid`, `dem.about` |
| `plugin/proj` | `SmtAMMapProject` | `CDlgMapPrj` (tab host), `CDlgMapPrjDoXY`, `CDlgMapPrjDoGrid` | `proj.do_prj` |
| `plugin/print` | `SmtAMMapPrint` | `CDlg2DXView` | `print.preview` |
| `plugin/model3d` | `SmtAM3DModelCreater` | no custom `CDialog`; `CFileDialog` + `MessageBox` + scene mutations | `model3d.add_pointcloud` �� `model3d.layer_polygons_to_3d` (nine commands, leftover `MSG_3DMODELCREATER_1`�C`9`) |
| `plugin/baogrid` | `SmtAMBAOGridCreater` | no custom `CDialog`; `CFileDialog` + `MessageBox` + leftover IA line tool | `baogrid.input_boundary_0`, `baogrid.input_boundary_2`, `baogrid.save_boundary`, `baogrid.load_boundary` |

Leftover plugin `CDialog`s rewrite to `ui::views`. Shared `plugin::MapPreviewView` is the print preview canvas. model3d / baogrid extra UI is file picker + message box.

## Components

| Unit | Tree | Namespace | Role |
| --- | --- | --- | --- |
| `Manifest` | `src/plugin/manifest.h` | `plugin` | Parse / validate `plugin.json` |
| `Registry` | `src/plugin/registry.h` | `plugin` | Discover, verify, enable/disable, start/stop. Owned by chrome |
| `Signature` | `src/plugin/signature.h` | `plugin` | SHA-256 + ed25519 verify; trust store |
| `Store` | `src/plugin/store.h` | `plugin` | Local dir scan, zip install/uninstall, HTTP `plugins.json` |
| `LegacyAmAdapter` | `src/legacy/plugin/runtime/bridge/am.h` | `plugin` | Calls leftover `SmtPluginManager` for `*.am` only |
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
- Signature file: `name-version.zip.sig` �� exactly 64 bytes, raw ed25519 signature of that 32-byte digest (not hex, not minisign armored).
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

- `scan(const char* aux_module_dir)` calls `SmtPluginManager::GetSingletonPtr()->LoadAllPlugin(aux_module_dir)` (same path `App` uses: `GetAppPath() + "aux module\\"`).
- Maps leftover display names / file stems to builtin ids:

| Leftover stem or `SmtAuxModule` name | id |
| --- | --- |
| `SmtAMDemCreater` / `DEM����` | `smartgis.dem` |
| `SmtAMMapProject` / projection plugin | `smartgis.proj` |
| `SmtAMMapPrint` | `smartgis.print` |
| `SmtAM3DModelCreater` / `��ά����` | `smartgis.model3d` |
| `SmtAMBAOGridCreater` / `�߽���Ӧ��������` | `smartgis.baogrid` |

Unmapped `*.am` gets id `legacy.<stem_lower>` and `kind=legacy_am`, `api_version=1`.

- Enable leftover: `StartPlugin` if not already started.
- Disable leftover: `StopPlugin`. HMODULE stays loaded unless `Registry::unload` is called (then leftover `UnLoadPlugin`).
- Missing exports stay leftover behavior (do not start; record `PluginState::kInvalidExports`).
- Do not treat leftover `GetPluginVersion()==1` as a new-API plugin.

Leftover MFC `CDialog` sources remain in the six `smt_mfc_shared_library` targets so `dll_stem` and `SmartGIS-Legacy.exe` keep compiling. Views chrome never instantiates `CDlg*`. New public types are the Views classes below.

### `content::PluginHost`

QgsInterface analogue. Header: `src/content/public/plugin_host.h`. Implementation lives in `src/content/browser/plugin/plugin_host.cc` and may use `plugin::` internally; **the public header must not include `src/plugin` or leftover headers**.

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

`MapContents` is the only map face plugins get on the new path. It wraps the existing `content::MapSession` (do not rename `MapSession` in this cycle). No `Map*`, HWND, or `LPRENDERDEVICE` on this header.

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

`src/ui/views` stays a **single module**. Add form-control headers next to `view.h` (no `src/ui/views/controls/` nest �� `ui-views-skia.md` forbids extra public nests).

| Control | File | Role |
| --- | --- | --- |
| `Label` | `src/ui/views/label.h` | Static text |
| `Button` | `src/ui/views/button.h` | Click �� `std::function<void()>` |
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
| `CDlgTinLoader` | `TrimeshLoaderDialog` | `plugin/product/world3d/grid/dem/dialog/trimesh_loader_dialog.h` | per-plugin; uses TableView, Combobox, FilePicker, Checkbox |
| `CDlgGridLoader` | `HeightmapLoaderDialog` | `plugin/product/world3d/grid/dem/dialog/heightmap_loader_dialog.h` | per-plugin |
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

No `smartgis.qt`. No PyQt. No `sip`. Missing Views binding �� `contribute_dialog` / Python UI call returns an error object; the plugin stays loaded; the dialog does not open.

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
2. Store hashes zip; mismatch with index `sha256` �� `kBadHash`; stop.
3. `SignatureVerifier::verify` with `kOfficialPublicKey`; fail �� `kBadSignature`; stop.
4. Extract to `<user_data>/plugins/<id>/`; parse `plugin.json`; `id` mismatch �� delete extract, `kBadManifest`.
5. Registry records the plugin as disabled until enable.

**Install (unsigned)**

1. Same extract + parse.
2. Trust class is `kDenied` until `trust_unsigned(id)`.
3. Enable while `kDenied` returns false (`kUnsignedNotTrusted`).

**Enable �� start �� contribute**

1. `Registry::set_enabled(id, true)` checks trust + `api_version`.
2. `kind=builtin`: call the compiled `plugin_start(PluginHost*)` for that id.
3. `kind=python`: `PythonRuntime::start(dir, entry, host)`.
4. `kind=legacy_am`: `LegacyAmAdapter::start(id)` �� leftover `StartPlugin` (still `AppendFuncItems` for MFC).
5. `kind=native`: `LoadLibrary` of `library`, resolve `plugin_start` (`bool plugin_start(content::PluginHost*)`). Missing symbol �� `kInvalidExports`.
6. `plugin_start` / Python `start` call `contribute_*`. Commands land in `tool::CommandCatalog`.

**Command / dialog**

1. Chrome menu or Plugin Manager runs `host->execute("dem.load_tin")`.
2. Handler calls `host->open_dialog("world3d.trimesh_loader")`.
3. Dialog factory builds `TrimeshLoaderDialog` (Views). OK collects JSON args and `host->run_processing("world3d.trimesh_from_xyz", args)`.
4. Worker runs `tin::` / leftover loader. `done` on main thread. Success: chrome commits via `sdb::EditSession` if a feature was created, then `EventBus` (for example `ExtentChanged`). The dialog does not write `Map`.

**Disable / unload**

1. `set_enabled(id, false)` �� `host->withdraw(id)` then kind-specific stop (`plugin_stop` / Python `stop` / leftover `StopPlugin`).
2. `unload` additionally `FreeLibrary` leftover / native, or drop the Python module. Builtin code stays linked.

## Error handling

| Case | Result |
| --- | --- |
| Bad zip (not zip, truncated, path escape `..`) | `Store::install` false; `kBadZip`; nothing left under `<user_data>/plugins/<id>/` |
| SHA-256 mismatch | `kBadHash`; zip not extracted |
| Failed ed25519 / missing `.sig` on a remote install | `kBadSignature`; not installed |
| Unsigned local zip / dir without prior `trust_unsigned` | Installed files may exist; `set_enabled` false; `kUnsignedNotTrusted` |
| `plugin.json` parse / missing required field / bad `id` | `kBadManifest`; extract removed |
| `api_version` �� 2 for non-`legacy_am` | `kApiMismatch`; not started |
| `legacy_am` `GetPluginVersion` missing or `StartPlugin` / `StopPlugin` missing | `kInvalidExports`; leftover already `FreeLibrary`s |
| Duplicate `plugin.json` `id` already enabled | Second start refused; first kept |
| Duplicate `contribute_command` id | `contribute_command` false; first handler kept |
| Missing Views binding (Python opens unknown widget) | Python exception �� command false; plugin stays enabled |
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
- Leftover adapter: a fixture DLL is **not** required; map table maps `SmtAMDemCreater` �� `smartgis.dem`; unknown stem �� `legacy.foo`.
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
| `src/content/browser/plugin/plugin_host.cc` | Default `PluginHost` implementation |
| `src/plugin/manifest.h` `.cc` | `plugin.json` |
| `src/plugin/registry.h` `.cc` | enable / disable / start / stop |
| `src/plugin/signature.h` `.cc` | SHA-256 + ed25519 |
| `src/plugin/official_key.h` | pinned 32-byte official public key |
| `src/plugin/store.h` `.cc` | local + HTTP index + zip |
| `src/legacy/plugin/runtime/bridge/am.h` `.cc` | `*.am` adapter |
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
| `src/plugin/product/world3d/commands.h` | Public façade: `register_world3d` + writers/commits |
| `src/plugin/product/world3d/grid/` | Parent: DEM + 2D orthogrid + 3D hex; `register_world3d_grid` |
| `src/plugin/product/world3d/grid/dem/` | `register` façade; `loader/` kernels; `dialog/` Views+markup; `tests/` |
| `src/plugin/product/world3d/resources/data/` | Data stub (copy → `out/plugins/world3d/data`) |
| `src/plugin/product/world3d/scene/` | True-Earth + leftover `model3d.*` |
| `src/plugin/product/world3d/grid/{orthogrid,hexgrid}/` | 2D lattice/session/solve; 3D lattice/sample/io/solve |
| `src/ui/views/{label,button,textfield,checkbox,radio_button,combobox,tab_strip,table_view,file_picker,message_box}.*` | toolkit controls |
| leftover `src/base/plugin*.`, `src/plugin/module*.`, domain `dlg_*.h` | unchanged ABI |
| `third_party/ed25519/` | verify-only ed25519 |
| `third_party` Python embeddable | CPython 3.12 |

Includes: `"content/public/plugin_host.h"`, `"plugin/host/registry.h"`, `"plugin/widgets/map_preview.h"`, `"ui/views/button.h"`.

## Industry mapping

| QGIS | ArcGIS Pro | This repo |
| --- | --- | --- |
| `QgsInterface` | `IApplication` / `IPlugin` host | `content::PluginHost` |
| `metadata.txt` + zip | add-in XML / `.esriaddin` | `plugin.json` + zip |
| Plugin repo XML | ESRI / org portal | HTTP `plugins.json` + local dir |
| In-process Python + PyQGIS | ArcPy in-process | CPython embed + `smartgis.*` |
| PyQt widgets | Qt / WinUI add-in UI | `ui::views` (Qt banned) |
| `QAction` | `ICommand` / `Button` | `tool::Command` |
| `QgsMapCanvas` in a dialog | `MapView` embed | `plugin::MapPreviewView` �� `ui::views::MapViewport` |
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
- No plugin signing CA / certificate chain �� one pinned ed25519 official key.
- No live-reload / hot-swap beyond disable �� enable.
- No third public namespace.

## Docs (same change set as implementation)

- `src/README.md` �� expand the `plugin/` bullet to host + Registry + PluginHost + Python + store; point at this spec.
- Root `README.md` �� if the module / directory table still implies plugin is only leftover domain DLLs, add one clause; refresh **������** to 2026-09-13.
- `docs/README.md` �� index this spec and the implementation plan.
- `docs/superpowers/src-layout.md` �� plugin row: host source_set + domain children + widgets + python.

## Out of this cycle

- Real `--type=utility` child with a Mojo pipe for job bytes (stub only here).
- Retiring leftover `CDlg*` from the MFC `dll_stem` graphs (they stay compiling).
- Multi-interpreter Python, pip install into the embed, or third-party Python GIS stacks.
- Plugin sandbox (seccomp / job object) for native code.
- Renaming `content::MapSession` to `MapContents` globally (this spec adds `MapContents` as the plugin face only).

---

## ��smartgis.gis bindings �� phase 1��2026-09-28��

**Status:** active  
**Updated:** 2026-09-28  
**Plan:** [`../plans/2026-09-28-gis-python-spatial-analysis.md`](../archive/plans/2026-09-28-gis-python-spatial-analysis.md)  
**Shell contract:** [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md) ��GIS Python Console

Extends **Python runtime and bindings** above. End-state module tree mirrors `src/gis` (`model`, `datasource`, `kernel`, `vista`, `present`). Phase 1 only:

| Python | Role |
| --- | --- |
| `smartgis.gis.analysis.ops()` | Catalog of `native.*` processing ids |
| `smartgis.gis.analysis.run(id, **params)` | Write active �� `PluginHost::run_processing` �� load result �� refresh |
| `smartgis.gis.analysis.buffer/clip/��` | Thin wrappers over `run` |
| `PythonRuntime::eval` | Console / Agent in-process execution |
| `GisConsoleBridge` | Map document write/load/refresh callbacks (no GIS headers in Agent) |

Unbound submodules (`model`, `datasource`, ��) may be absent or raise a clear `NotImplementedError` until later phases. No PyQt. Bindings stay under `src/plugin/runtime/python/`.

---

## ��Python dual-runtime��embed + worker����2026-09-28��

**Status:** active  
**Updated:** 2026-09-28  
**Plan:** [`../plans/2026-09-28-gis-python-spatial-analysis.md`](../archive/plans/2026-09-28-gis-python-spatial-analysis.md)  
**Shell:** [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md) ��GIS Python Console / ��Diagnostic Tools  
**Algorithm:** [`2026-09-13-algorithm-layer-oss-design.md`](2026-09-13-algorithm-layer-oss-design.md) ��Python-facing analysis

### Locked (brainstorming)

| # | Choice |
| --- | --- |
| 1 | **Dual track (A):** in-process CPython embed **and** out-of-process Debug worker, in parallel �� not B/C single-track. |
| 2 | **One API surface:** package name `smartgis` + shared `.pyi`. Embed = native extension; worker = JSON-RPC thin client with the same method names where serializable. |
| 3 | **Embed owns:** `kind=python` plugins, `contribute_dock` / dialog / command / processing, live `PluginHost` / Views widgets, default Console `:py` / bare-line GIS (`PythonRuntime::eval`). |
| 4 | **Worker owns:** DAP / Pyright long `:run`, crash-isolated heavy scripts, optional scipy/networkx; talks to chrome only via `DebugAgent`. |
| 5 | **Heavy spatial analysis** (flood, least-cost path): Python **orchestrates**; kernels land in `gis/analysis` then `plugin::` / `contribute_processing` �� no Shapely / second GEOS. |
| 6 | **Debug profile:** `smartgis.debug` binds `base::trace::process_trace` / `base::trace::set_tracing_enabled` so plugins register spans visible in Diagnostic Tools CPU tab. |
| 7 | No second conda/venv per plugin; no Qt/PyQt; no per-plugin UI process. |

### Capability map (product goals �� seams)

| Goal | Embed | Worker | Notes |
| --- | --- | --- | --- |
| Analysis panel in UI | `contribute_dock` / `contribute_dialog` + sample plugin | �� | SpatialAnalysisPanel stays primary chrome; plugins add docks/dialogs |
| Console Python syntax | `eval` default | DAP / long file | Bare lines + `:py` prefer embed when `py_eval` bound |
| Pure-analysis scripting ceiling | `smartgis.gis.analysis` + `contribute_processing` | Same ids via Agent `cmd` / RPC | Catalog grows with `gis/analysis` |
| Plugin ? Debug full surface | `smartgis.debug.profile_*` | `py.register` + Agent methods | Same Diagnostic Tools dock |
| Flood / road optimal path | `analysis.run` / plugin processing | Heavy prep off chrome | Kernels later; API reserved |
| Scene GIS 2D/3D objects | `smartgis.gis.scene.*` via shell bridge to `MapScene` | RPC mirrors | Same document; present mode Map/Data/3D |
| Style / system config | `smartgis.gis.style.*` + `smartgis.ui.config` (ThemeService) | RPC mirrors | StyleDocument path + UI theme packs |

### Runtime ownership

```
PluginShell
  ������ plugin::PythonRuntime   (embed; one interpreter)
  ������ content::PluginHost
  ������ GisConsoleBridge �� Map document write/load/refresh

DebugAgent
  ������ host.py_eval �� PythonRuntime::eval   (preferred)
  ������ OOP spawn / py_sock worker           (fallback + DAP)
```

### Python host contributions (embed)

Beyond `contribute_command`, expose on `smartgis.Host`:

- `contribute_dock(plugin_id, id, title, area, factory_callable)`
- `contribute_dialog(plugin_id, id, title, factory_callable)`
- `contribute_processing(plugin_id, id, title, factory_callable)` �� factory `(host, args_json) -> bool`; must not touch Views

### `smartgis.debug` (both tracks; embed native, worker RPC)

| Symbol | Behavior |
| --- | --- |
| `debug.set_tracing(on: bool)` | `base::trace::set_tracing_enabled` |
| `debug.trace_event(name, cat)` | context manager �� `ScopedTraceEvent` |
| `debug.tracing_enabled()` | bool |

### Flood / path (landed kernels)

1. **API:** `smartgis.gis.analysis.run("native.cost_path" | "native.flood_fill", ��)` via catalog. Product UIs: `smartgis.traffic` / `smartgis.flood`.
2. Do **not** vendor a full hydrology engine or OSRM process; in-tree Dijkstra + GDAL DEM inundation (see ��traffic + flood).
3. Sample `kind=python` plugin remains educational; prefer builtin product packages for demos.

### Scene + style + system config��2026-09-28 expand��

**Status:** active  
**Shell wiring:** `GisConsoleBridge` extended; set from `BrowserView` when Diagnostic Tools / Python init.

Same `MapScene` backs Map (2D) and 3D tabs. Python does **not** include `MapScene` headers �� string/callback bridge only (Agent stays free of GIS types).

| Python | Behavior |
| --- | --- |
| `smartgis.gis.scene.layers()` | list of `{id,name,visible,active}` from `layer_descs` |
| `smartgis.gis.scene.select_layer(id)` | `MapScene::select_layer` + refresh |
| `smartgis.gis.scene.set_visible(id, on)` | `set_layer_visible` + refresh |
| `smartgis.gis.scene.extent()` | `{min_x,min_y,max_x,max_y}` or None |
| `smartgis.gis.scene.open(path)` / `write(path)` | `open_path` / `write_path` |
| `smartgis.gis.scene.feature_count()` | int |
| `smartgis.gis.scene.present_mode()` | `"map2d"` \| `"data"` \| `"scene3d"` |
| `smartgis.gis.scene.set_present_mode(mode)` | Map tab strip index 0/1/2 |
| `smartgis.gis.style.has_document()` | bool |
| `smartgis.gis.style.load(path)` | `load_style_path` (`.style.json`) |
| `smartgis.gis.style.clear()` | `clear_style_document` |
| `smartgis.gis.style.summary()` | `{name,version,layers:[ids��]}` or None |
| `smartgis.ui.config.themes()` | ThemeService packs `{id,label}` |
| `smartgis.ui.config.theme_id()` / `set_theme(id)` | active UI theme + persist |

**Non-goals this expand:** per-vertex 3D mesh editing API; full StyleDocument paint-key mutation from Python (load/replace file first); CEF/WinUI theme.

### Non-goals (this ��)

- Unify embed and worker into one process.  
- Full flood / network solver kernels.  
- Shell auto-layout of every `contribute_dock` (register + open path first; chrome mount can follow).

---

## ��product�CPython division��L1 substrate / L2 seams / L3 orchestration����2026-09-28��

**Status:** active  
**Updated:** 2026-09-28 �� P0�CP3 landed; P4 skeleton/policy + samples (industry_pack / product_orchestrate / analysis)  
**Locked choice:** approach **C** �� C++ capability substrate + Python product orchestration (not A: Python-only console; not B: rewrite all product business in Python).  
**Related:** ��Python dual-runtime above; [`../plans/2026-09-28-gis-python-spatial-analysis.md`](../archive/plans/2026-09-28-gis-python-spatial-analysis.md); product trees under `src/plugin/product/`; sample template [`../../../src/plugin/runtime/samples/industry_pack/`](../../../src/plugin/runtime/samples/industry_pack/).

### Goal

Raise the ceiling for extending `src/plugin/product` without claiming that every product behavior can be a pure-Python reimplementation. Stable kernels and host widgets stay C++; product flow (commands, dialogs, docks, industry variants) prefers Python once L2 bindings are complete. Builtin C++ remains the reference / fallback implementation.

### Three layers

```
L3  Product orchestration   �� Python preferred (kind=python); builtin C++ as reference
L2  Host seams              �� C++ PluginHost + thin smartgis.* bindings (must be complete)
L1  Capability substrate    �� always C++; grow via stable processing / tool / widget ids
```

**One-liner:** Python does not implement kernels; it calls them. C++ does not accumulate product workflows; it exposes composable capabilities.

### L1 �� must stay C++ (ceiling grows with the catalog)

| Capability | Contract shape | Notes |
| --- | --- | --- |
| TIN / heightmap / Delaunay | `dem.tin_from_xyz`, `dem.grid_from_heightmap` | Loaders + numeric mesh |
| Orthogrid Laplace | `baogrid.create_orth_grid` / `orthogrid.create_orth_grid` | Session + `orthogrid/boundary_solve`; solver in `gis/geo/grid` |
| Projection transforms | `proj.*` processing (when registered) | PROJ / GDAL stack |
| OGR / GEOS operators | `native.*` via `plugin::` �� `gis/analysis/ops` | Kernels in `gis.dll`; no second GEOS / Shapely |
| Surface / mesh write-back | `DemSurfaceWriter`-class host callbacks | Document consistency; no `Map*` in Python |
| 3D scene object writes | scene-device primitives | Point cloud / water / terrain / layer��3D |
| Print preview canvas | `MapPreviewView` (+ export kernels) | Views/Skia composite control |
| Boundary digitize | `tool` ids (e.g. `edit.append.linestring`) | Interaction state machine |

Ids are the ABI. Do not change semantics of a published processing / tool / dialog id; add a new id instead.

### L2 �� C++ owned, Python must call (binding completeness = ceiling)

Expose on embed `smartgis` (worker mirrors where serializable):

| Seam | Status intent |
| --- | --- |
| `Host.contribute_command` | Landed |
| `Host.contribute_dialog` / `contribute_dock` / `contribute_processing` | Required; designed in ��Python dual-runtime, bind next |
| `host.run_processing` / `open_dialog` / `tool.execute` | Required |
| `smartgis.ui` file_picker / message_box | Required for product-parity orchestration |
| `smartgis.gis.analysis` / `scene` / `style` | Phase-1 landed; grow catalog, not GIS headers |
| `smartgis.tool.activate(tool_id)` | Required for orthogrid digitize bridge |
| `smartgis.debug.profile_*` | Dual-runtime lock |

**Rule:** any side-effect API that product C++ uses today must land as processing, tool, dialog id, or Host/GisConsoleBridge callback. No Python-only bypass into legacy map/scene types.

### L3 �� Python preferred (product shape replaceable)

| Product module | Keep in C++ (L1 / widgets) | Move to Python when L2 ready |
| --- | --- | --- |
| `dem` | Loaders, processing, surface writer | Param dialogs, command wiring, validation copy |
| `proj` | Transform kernels | Wizard / tabs, batch lists, messaging |
| `print` | `MapPreviewView`, export kernels | Open preview, path pick, page options orchestration |
| `orthogrid` | Laplace; optional `.gridbnd` IO | Arm boundary, activate digitize tool, assemble processing args, industry grid policy |
| `model3d` | Scene-device write primitives | Menus, file pick, layer��3D step orchestration |

Builtin `kind=builtin` plugins remain the **reference and performance fallback**. Same contribution ids may be supplied by a trusted `kind=python` package for orchestration; **do not** let Python redefine L1 processing semantics. Prefer: builtin fallback + Python industry packs for L3 only.

### P4 path (pragmatic �� not full builtin deletion)

**Policy (landed 2026-09-28):** Industry packs are **`kind=python` L3** packages that orchestrate published L1 ids (`dem.*`, `native.*`, ��) via `Host.contribute_command` �� `host.run_processing`. Builtin C++ keeps **L1 kernels + reference dialogs/docks** until a surface is **explicitly withdrawn per plugin**. Full deletion of builtin UI is **out of scope**.

| Deliverable | Status |
| --- | --- |
| Document this P4 path on the living �� | **landed** 2026-09-28 |
| Template sample `src/plugin/runtime/samples/industry_pack/` | **skeleton landed** 2026-09-28 |
| Migrate all product modules to Python packs | **not started** (follow-on; per-plugin) |

Template: contribute industry commands only; call `dem.tin_from_xyz` / `native.buffer` (and peers); state in README that builtin remains reference fallback.

### Contract rules

1. One capability �� one stable id (`dem.tin_from_xyz` never changes meaning).
2. Processing factories (C++ or Python) must not touch Views / dialogs.
3. Document writes only through writer / EditSession seams �� Python never includes `MapScene` / leftover map headers.
4. Heavy work: L1 kernels; short scripts: embed; long / crash-prone: Debug worker (dual-runtime).
5. Industry depth = Python plugin packages under store; do not fork `src/plugin/product` trees per customer.

### Phased delivery

| Phase | Deliverable | Ceiling unlocked | Progress (2026-09-28) |
| --- | --- | --- | --- |
| P0 | Bind `contribute_dialog` / `dock` / `processing` + file_picker / message_box | Real `kind=python` plugins, not command-only | **landed** |
| P1 | dem / orthogrid kernels only via processing; one Python sample + keep builtin UI | Product orchestration convertible to Python | **landed** �� `samples/product_orchestrate` + analysis sample |
| P2 | `tool.activate` + baogrid digitize bridge | Interactive product flows in Python | **landed** �� `smartgis.tool.activate` + shell `ActivateTool` wire |
| P3 | Scene-device primitives + thin model3d bindings | 3D orchestration | **landed** �� `Model3dSceneWriter` + `model3d.*` processing |
| P4 | Builtin retreats to L1 + reference UI; industry packs default Python | Extension main path = Python | **skeleton / policy landed** �� not full migration; see **P4 path** + `samples/industry_pack/` |

### Non-goals (this ��)

- Rewriting TIN / Laplace / GEOS / PROJ kernels in Python.
- Per-vertex mesh editing as a first-class Python API (may follow later under render / scene umbrellas).
- Per-plugin conda / venv / pip into the embed.
- Plugin UI in a child process.
- Claiming `src/plugin/product` is fully replaceable by pure Python without L1/L2.
- **Deleting all builtin product UI in P4** �� reference dialogs stay until per-plugin withdrawal.

### Rejected alternatives

| Option | Why rejected |
| --- | --- |
| A �� product stays C++-only; Python = console only | Too low a ceiling for store / industry packs |
| B �� move all product business into Python | UI, digitize, 3D, and write-back seams explode; duplicates L1 |

---

## ��gis analysis ownership��2026-09-28��

**Status:** active  
**Updated:** 2026-09-28  
**Related:** [`2026-09-13-algorithm-layer-oss-design.md`](2026-09-13-algorithm-layer-oss-design.md) ��Python-facing analysis; [`../src-layout.md`](../src-layout.md)

### Locked

| # | Choice |
| --- | --- |
| 1 | Directory **`src/gis/analysis/`** (peer to `geo` / `model`). |
| 2 | Layout: `ops/` (native GeoJSON runners), `geometry/` / `raster/` (future typed objects; README skeleton this cycle). |
| 3 | **Public product API** remains `plugin::BuiltinOpDesc` / `builtin_op_catalog` / `run_builtin_op` (declared under `plugin/runtime/processing`). |
| 4 | Implementation is **`gis::detail`** in `gis.dll` (`GIS_EXPORT`); plugin `.cc` only forwards. |
| 5 | Core algorithms and analysis objects do **not** land under `src/plugin/` going forward. Domain product plugins may orchestrate via processing ids. |
| 6 | `gis/geo/ops` stays for low-level GEOS/OGR helpers; `analysis/ops` may call it. |

### Non-goals (this slice)

- Do not introduce a third public namespace for callers (`gis::analysis::*` as product API).
- Do not move dem / orthogrid / model3d product kernels in this change.
- Do not add a QGIS-style `ProcessingAlgorithm` base class yet.

---

## Leftover `legacy/plugin` role layout (2026-09-29)

**Status:** landed (layout only). **Plan:** [`../plans/2026-09-29-legacy-plugin-subdirectory-layout.md`](../plans/2026-09-29-legacy-plugin-subdirectory-layout.md).

Unfreezes the 2026-09-27 archive freeze (��no further nesting��). Stay under `src/legacy/plugin` �� do **not** move AuxModule / MFC shells into `src/plugin/{runtime,product}`. Mirror endgame buckets: leftover **`runtime/`** + **`product/`**; role names `shell` / `views` / `kernel`. Inside **`runtime/`**: **`auxmodule/`** (SmtAuxModule ABI �� `//src/legacy/plugin:plugin`) and **`bridge/`** (`*.am` scan TU + header-only `cmd.h` AM_MSG map �� `:bridge`). Do not name a directory `aux/` (Windows device name).

```
src/legacy/plugin/
  runtime/
    auxmodule/          # AuxModule DLL sources
    bridge/             # am.cc + cmd.h (header-only)
  product/<domain>/
    shell/
    views/
    kernel/             # orthogrid only
```

Scheme C includes (`legacy/plugin/runtime/auxmodule/��`, `legacy/plugin/runtime/bridge/��`, `legacy/plugin/product/<domain>/shell/��`); no shim. `dll_stem` / DEF / `Smt_*` unchanged.

---

## ��`runtime/host` role layout (2026-09-30)

**Status:** landed (layout only).

Nest `src/plugin/runtime/host/` by responsibility (Scheme C includes; **no** shim at old flat paths). Export macro stays at host root.

```
src/plugin/runtime/host/
  BUILD.gn
  plugin_host_export.h     # DLL export macro (widely included)
  registry/                # enable/disable / builtin hooks / state
  manifest/                # plugin.json parse
  store/                   # zip install + HTTP index
  signature/               # ed25519 + official / test keys
  processing/              # host ProcessingPool attach + operation_result
  resources/               # per-plugin resource roots (--plugins-dir)
  ui/                      # ManagerView
  tests/                   # host_test.cc
```

Includes: `"plugin/runtime/host/registry/registry.h"`, `"��/manifest/manifest.h"`, `"��/resources/resource_roots.h"`, �� �� path from `//src`. Namespace remains `plugin`.

---

## Folded topics (2026-09-28 merge B)

Former hot specs are under `archive/specs/` (`superseded`). **Revise this file** (append `��`) for new requirements in this topic. Do not create a new `YYYY-MM-DD-*-design.md`.

| Former hot spec | Section / note |
| --- | --- |
| [`../archive/specs/2026-09-14-plugin-full-upgrade-design.md`](../archive/specs/2026-09-14-plugin-full-upgrade-design.md) | ��Plugin full upgrade (folded) |
| [`../archive/specs/2026-09-14-plugin-subdir-layout-design.md`](../archive/specs/2026-09-14-plugin-subdir-layout-design.md) | ��Plugin subdirectory layout (folded); leftover half further nested 2026-09-29 �� �� Leftover `legacy/plugin` role layout |

---

## ��product sample + visualization��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **C** �� host seams + sample fixtures + per-plugin interact/suite (not harness-only, not interactive-only).  
**Plan:** [`../plans/2026-09-30-plugin-product-sample-viz.md`](../plans/2026-09-30-plugin-product-sample-viz.md)

### Goal

Every builtin under `src/plugin/product/` loads **shipped sample data** and produces a **visible** map/scene result through the same command/processing ids a user would run (menu / `host.run_processing` / interact DSL).

### Product set (after model3d merge)

| Tree | Plugin id | Primary viz |
| --- | --- | --- |
| `product/world3d` | `smartgis.world3d` | DEM TIN/grid → `MapScene::add_triangle_layer`; former model3d cmds → `World3dSceneWriter`; 2D `create_orth_grid` mesh; 3D `create_hex_grid` hex lattice |
| `product/traffic` | `smartgis.traffic` | Least-cost path GeoJSON �� `TrafficPathWriter` (map2d/scene3d progressive path) |
| `product/flood` | `smartgis.flood` | DEM inundation mask �� `FloodMaskWriter` (water-level frame animation) |
| `product/geochem` | `smartgis.geochem` | Graded sample points + full-extent IDW heat raster �� `GeochemWriter` |
| `product/print` | `smartgis.print` | china sample map loaded �� `print.preview` MapPreviewView |

`product/model3d` / `product/orthogrid` / `product/orthogrid3d` are **withdrawn** as separate builtins: sources/ids live under `world3d` (keep leftover `model3d.*` / `baogrid.*` / `orthogrid.*` / `orthogrid3d.*` command aliases). Do not leave dual `register_orthogrid` + `register_world3d` in `PluginShell`.

### Locked

| # | Choice |
| --- | --- |
| 1 | Sample fixtures live under **`testing/data/plugin/`** (GN copy to `out/data/plugin/`). Plugin-owned markup stays under `out/plugins/<package>/`. |
| 2 | Browser installs **all** writers in `Browser::init` (`set_world3d_surface_writer` + `set_world3d_scene_writer`). Unset �� structured `no_map_seam` / `no_scene_device` only. |
| 3 | Acceptance = interact suite + BMP/marks (same loop stack as `map2d.china` / `map2d.orthogrid`). Manual menu click is secondary. |
| 4 | Prefer existing samples: `china_dem.tif`, mini XYZ, `pointcloud_public_sample.txt`, china city vectors. Do not vendor a hydrology engine in this ��. |
| 5 | Processing factories stay Views-free; dialogs only assemble JSON and call `run_processing`. |

### Non-goals

- Flood / inundation kernels (follow-on under gis analysis).
- Deleting leftover `legacy/plugin/product/{dem,model3d}` `*.am` this cycle.
- Full leftover `SmtSceneMgr` parity for every model3d primitive in Views (P0: pointcloud + at least one primitive that paints; sphere/water may map to layered stand-ins until scene3d object API lands).

### Phased delivery

| Phase | Deliverable |
| --- | --- |
| P0 | Finish model3d �� world3d merge; wire scene writer; ship fixtures |
| P1 | `plugin.world3d` interact (tin + grid + pointcloud) green |
| P2 | `plugin.print` + orthogrid plugin path (not only showcase) green |

---

## ��world3d pointcloud LAS��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **2** �� shared `vista/component/world/pointcloud` module; `world3d` registers commands only.  
**Plan:** [`../plans/2026-09-30-world3d-pointcloud-las.md`](../plans/2026-09-30-world3d-pointcloud-las.md)  
**Render draw:** [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md) ��Point cloud GPU draw

### Goal

Load industry **LAS / LAZ** (and legacy sample `.txt`) into a shared point buffer, attach to `gis::World` `kPointCloud`, and show points in the new scene path (`GpuScene`) plus map footprint via `MapScene`.

### Locked

| # | Choice |
| --- | --- |
| 1 | Formats P0: uncompressed **LAS** 1.2/1.4 (ASPRS) + leftover RGB txt. **LAZ** via vendored **LASzip** `LASunzipper` (`third_party/.src/LASzip` + `//third_party:laszip`). |
| 2 | Reader stack: light **LASzip** first (default `load_point_cloud`); **PDAL** only via P3 processing (`has_pdal` when `third_party/.install` has PDAL). |
| 3 | Codec under `src/vista/assets/pointcloud/`; node buckets under `src/vista/component/world/pointcloud/`. Plugin does not embed parse. |
| 4 | Viz path: `World3dSceneWriter` �� shared loader �� `World` / `GpuScene` (not leftover `PointCloud3d` as default). Map2d may also show point features. |
| 5 | Scale: document P0�CP2 (full load �� chunk/thin �� octree/LOD); implement from P0. |
| 6 | Prefer sample fixtures under `testing/data/` (tiny `.las` + existing `pointcloud_public_sample.txt`). |

### Phased delivery

| Phase | Deliverable |
| --- | --- |
| P0 | Shared load + World payload + GpuScene draw + world3d picker (LAS/LAZ/txt) �� **landed** |
| P1 | Stride / chunk AABB + frustum cull (~1e6) �� **landed** (`chunk` + per-chunk GpuMesh) |
| P2 | Octree + LOD (~1e7 target; `third_party/octree`) �� **landed** (unibn select + >500k auto-thin) |
| P3 | PDAL processing ops (`world3d.pdal_read` / `world3d.pdal_pipeline`); GN auto-detect `.install` (**hard link when present**, stub `pdal_not_built` when absent); default `load_point_cloud` stays LASzip �� **stub landed**; live `build.bat t pdal` optional |

### Non-goals

- PDAL in the P0 link.
- Leftover `PointCloud3d` as the Views default path.
- Classification editing / full LiDAR analytics UI.

---

## §world3d True Earth（完整真3D）（2026-09-30）

**Status:** active  
**Updated:** 2026-10-02  
**Approach:** **A** — extend `smartgis.world3d` (do **not** open a parallel `earth3d` / `globe` package).  
**Plan:** [`../plans/2026-09-30-world3d-true-earth.md`](../plans/2026-09-30-world3d-true-earth.md)  
**Render stack:** [`2026-09-13-render-rhi-scene-design.md`](2026-09-13-render-rhi-scene-design.md) (Scene3d / atmosphere / tileset stream)

### Goal

Deliver a **Google-Earth-class product face** for true-3D browsing on the existing Views + Scene3d stack: globe / DEM terrain (China sample today; optional global GeoTIFF), atmosphere (sky / ocean / cloud / fog), satellite cloud cover (field ingest or procedural), orbit navigation + **fly-to**, optional city **3D Tiles** attach, and existing pointcloud / TIN / grid hooks — packaged under `src/plugin/product/world3d` with `out/plugins/world3d/` resources and `--plugin-showcase=world3d` capture.

### Locked

| # | Choice |
| --- | --- |
| 1 | Product id stays **`smartgis.world3d`**; no second builtin for “earth”. |
| 2 | Browser installs `World3dSceneWriter::{open_earth,fly_to,attach_tileset,load_global_dem,set_satellite_cloud,set_atmosphere}` next to existing scene writers. Unset → `no_scene_device`. |
| 3 | `open_earth` = Scene3D tab + `apply_china_scene3d_product_defaults` (DEM + procedural atmosphere + China orbit). |
| 4 | `load_global_dem` = optional GeoTIFF via `gis::set_sample_dem_path_override`; empty path resolves `out/data/global_dem.tif` / `out/plugins/world3d/data/` then **China stand-in** with structured JSON hint. |
| 5 | `set_satellite_cloud` = `AtmosphereSession::load_fields(path:cloud_cover)` when GeoTIFF present; else procedural cloud deck (`mode":"procedural"`). |
| 6 | `fly_to` = local lon/lat extent box + orbit distance (MVP; not spherical geodesic fly animation). |
| 7 | City tiles via `Scene3dGpuPresent::attach_tileset_json` + fixture `testing/data/m3_city_tileset.json` (or path arg). |
| 8 | Showcase path enables atmosphere (not land-only off) for Earth-class BMP. |

### Phased delivery

| Phase | Deliverable |
| --- | --- |
| P0 | Writer + commands + Browser + Earth showcase BMP — **landed** |
| P0b | Global DEM override + satellite cloud field / procedural + atmosphere toggles — **2026-10-02** |
| P1 | Interact suite `plugin.world3d.earth` (open_earth → fly_to → marks) |
| P2 | Spherical / clipmap globe mesh (gap pin; not Cesium Native) |

### Remaining vs Google Earth (honest)

- No full WGS84 **sphere** globe mesh / starfield / street-level Photorealistic 3D.
- No Google / Cesium Ion worldwide imagery streaming.
- Global DEM without a GeoTIFF stands in with China `china_dem.tif`.
- Satellite cloud without GeoTIFF uses procedural atmosphere clouds.
- City tiles: fixture + stream session; not production city coverage.
- Fly-to is extent reframe, not cinematic camera path.

### Non-goals

- Cesium Native vendor.
- New product package tree.
- Web GIS / mapd.

---


## ��traffic + flood analysis products��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **1** �� kernels in `gis/analysis`, two builtin product packages orchestrate UI / load / viz.  
**Plan:** [`../plans/2026-09-30-traffic-flood-analysis-plugins.md`](../archive/plans/2026-09-30-traffic-flood-analysis-plugins.md)

### Goal

Ship **`product/traffic`** (���н�ͨ���·��) and **`product/flood`** (DEM ��ˮ��û) with data load dialogs, real processing kernels, and map2d + scene3d animation seams.

### Locked

| # | Choice |
| --- | --- |
| 1 | Two packages: `smartgis.traffic`, `smartgis.flood` (not one umbrella). |
| 2 | Kernels: `gis/analysis/network` (`native.cost_path`) + `gis/analysis/raster/dem` (`native.flood_fill`); thin `plugin::` forward only. |
| 3 | Algorithms: in-tree Dijkstra on OGR line networks; GDAL DEM + priority-queue inundation (RichDEM-style; no full hydrology / OSRM vendor). |
| 4 | Viz: map2d result layers + scene3d writers (path progressive reveal; water-level frame sequence). Browser installs `set_traffic_path_writer` / `set_flood_mask_writer` in `Browser::init` (same pattern as world3d/orthogrid). Unset writers �� structured `no_*_seam` JSON. |
| 5 | Template matches `world3d`: `commands` + `manifest` + `processing` / `views` / `resources` / `tests`. |
| 6 | Register both in `PluginShell::start_builtins`. Sample Python stubs remain educational only. |

### Catalog args

- `native.cost_path`: `network`, `output`, `start_x/y`, `end_x/y`, optional `weight_field`, optional `frames` (path animation waypoints).
- `native.flood_fill`: `dem`, `output`, `seed_x/y`, `water_level` (absolute Z) or `water_depth`, optional `frames` + `frames_dir`.

### Non-goals

- Full 2D shallow-water / HEC-RAS.
- Full OSRM / Valhalla process stack.
- Withdrawing `samples/analysis` Python stub in this change.

---

## ��orthogrid adaptive boundary + heat viz��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **1** �� `GridSession` + `detail/` Laplace��optional Thompson��N + dual heat layers via `OrthogridMeshWriter`.

### Goal

Boundary-adapted orthogrid: digitize/replace four edges freely, auto-generate mutually orthogonal mesh (Laplace default, optional elliptic iters), map viz with mesh lines plus **cell-fill** and **axis-aligned raster** orthogonality heat layers for comparison.

### Locked

| # | Choice |
| --- | --- |
| 1 | Free adjust = re-digitize / replace any of flags 0..3 (vertex drag follow-on via MapScene). |
| 2 | Pipeline: Laplace �� `elliptic_iters`��`solve_elliptic_step` �� `|90?��|` node/cell fields (legacy `CalGridOrthogonality`). |
| 3 | Regenerate: auto on release/complete when `N==0` and `nx*ny �� 4096`; else `baogrid.generate` / processing. |
| 4 | Viz layers: `orthogrid_heat_raster` (under), `orthogrid_heat_cells`, `orthogrid` mesh lines; **interpolate** fill-color on numeric `heat` (= `|90?��|` ��), green��yellow��red. |
| 5 | Stay Views-free in processing; Browser installs richer mesh writer. No legacy `Orthogrid` class link. |
| 6 | Eigen A+B (2026-09-30): `GridField` owns row-major `ArrayXXd` `(ny,nx)`; Laplace `SimplicialLDLT` one-factor dual RHS; Thompson `SparseLU` + `analyzePattern` cache across `solve_elliptic_steps`; geometry via `Vector2d`. `BoundarySolve` still exports `vector<double>` xs/ys. |
| 7 | Sample `testing/data/plugin/orthogrid_sample.gridbnd`: **33��33 coastal bay** (embayments / cape / estuary / lagoon); showcase uses `elliptic_iters=4`. Regen: `py -3 testing/data/build_orthogrid_coast_sample.py`. |

### Non-goals

- Interactive vertex-drag sync in this drop (re-digitize is the free-adjust path).
- Real GeoTIFF basemap underlay (raster heat is axis-aligned polygon sampling).
- Changing `orthogrid3d` / legacy `Orthogrid::Orhogonal_SOR` in this Eigen pass.

---

## 「orthogrid3d HexLattice + VTK」（2026-09-30）

**Status:** active  
**Updated:** 2026-10-04  
**Approach:** product `plugin/product/world3d/hexgrid`; **do not** export `geo::HexGrid` from `gis.dll`. 2D mesh lives in `world3d/orthogrid`. Downstream GIS fluid / rigid-body analysis consume plugin lattice and/or `.vts`.

### Goal

True 3D single-block body-fitted structured hex mesh: 6-face Dirichlet + 7-point Laplace (M0), optional elliptic later; Scene3D commit hook; VTK StructuredGrid `.vts` export for external CFD.

### Locked

| # | Choice |
| --- | --- |
| 1 | Memory ABI: non-exported `HexLattice` in `plugin/product/world3d/hexgrid` (`nx,ny,nz`, row-major `k*ny*nx+j*nx+i`). Interchange / Feature identity: `OGRMultiPoint` XYZ + nx/ny/nz. **Not** `geo::HexGrid` in `gis/geo`. |
| 2 | Interchange M0: VTK XML StructuredGrid `.vts` writer (no VTK runtime dep). CGNS deferred. |
| 3 | M0 generate: 8-corner hex → face Dirichlet via bilinear + interior Laplace (`geo::solve_laplace` on `NodeField3d`); processing `orthogrid3d.create_hex_grid`. |
| 4 | Package id `smartgis.world3d` (command ids stay `orthogrid3d.*`). No separate `smartgis.orthogrid3d` / `smartgis.baogrid` product packages. |
| 5 | Fluid / rigid solvers are follow-on consumers (not in this drop). |
| 6 | Dev preview: in-repo VS Code/Cursor extension `mogu.vts-preview` (Three.js webview) under `testing/tools/harness/_shared/scripts/vscode/vscode-vts-preview/` — ASCII StructuredGrid only; not product Scene3D. |

### Non-goals

- Multi-block grids, tet meshes, 6-face interactive digitize UX (M1+).
- Shipping a VTK/CGNS SDK dependency.
- Changing 2D orthogrid Laplace / Map2D heat layers.
- Marketplace publish of the VTS preview extension (install via `install_vscode_vts_preview.bat`).
- Restoring `geo::HexGrid` / `SmtHexGrid` in `gis.dll` or `mesh/geometry.h`.

---

## §world3d command layers (2026-10-04)

**Status:** active  
**Updated:** 2026-10-04  
**Diagram:** [`../diagrams/plugin-product-world3d.html`](../diagrams/plugin-product-world3d.html)

### Goal

One package (`smartgis.world3d`) owns DEM, True-Earth scene, 2D orthogrid, and 3D hex **command contribution** as composed layers — not four leftover-shaped plugin modules.

### Locked

| # | Choice |
| --- | --- |
| 1 | Public façade is `plugin/product/world3d/commands.h`: `register_world3d` + `World3d*Writer` / `OrthogridMeshCommit` / `HexGridCommit` / boundary session. App shell and host_test include this header only. |
| 2 | `register_world3d` only wires `detail::register_world3d_{dem,scene,orthogrid,hexgrid}`. Domain `register.h` stays internal. |
| 3 | Identical leftover/product ids share one handler via `detail::contribute_command_aliases` / `contribute_prefixed_commands` (`baogrid.*` ≡ `orthogrid.*`; `model3d.add_pointcloud` ≡ `world3d.add_pointcloud`). |
| 4 | Command **ids** stay `baogrid.*` / `orthogrid.*` / `orthogrid3d.*` / `model3d.*` / `world3d.*` (AM, harness, host_test). No silent drop. |
| 5 | Solvers stay `gis/geo/grid`. Plugin I/O: `orthogrid/{session,boundary_solve}` and `hexgrid/{hex_lattice,vtk_structured,sample_volume}`. |

### Non-goals

- Deleting leftover `legacy/plugin/product/orthogrid` / `plugin_orthogrid` this change.
- Renaming AM command strings without a proven alias map in the same change.

---

## ��analysis session �� import / save / 2D��3D playback��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30 �� ResultPlayback inspector tab; orthogrid elliptic intermediate frames; pull_orbit_extent no longer calls MapContents::Extent
**Approach:** **B** �� chrome-owned `AnalysisPlayback` + `ResultPlayback`; four products fill strategy tables.  
**Plan:** [`../plans/2026-09-30-analysis-session-import-save-viz.md`](../plans/2026-09-30-analysis-session-import-save-viz.md)

### Goal

Unify **Import �� Run �� CommitLayer �� Export �� Playback** for `traffic` / `flood` / `orthogrid` / `orthogrid3d`, with controllable 2D/3D frame sync and BMP/PNG frame-sequence export (video encode deferred).

### Locked

| # | Choice |
| --- | --- |
| 1 | Session lives under `app/views/shell/runtime/analysis_session` (not under `plugin/`). |
| 2 | Save: **CommitLayer** to MapScene is primary; **Export** is explicit command / processing `output=`. |
| 3 | Writers (`TrafficPathWriter` / `FloodMaskWriter` / `OrthogridMeshWriter` / `HexGridWriter`) remain the seam; they also fill the session frame buffer. |
| 4 | Playback: shared `frame_index` drives map2d + scene3d stand-ins; CapabilityHost verbs `analysis_set_frame` / `analysis_export_frames`. |
| 5 | Frame export �� `out/<config>/captures/<run_id>/frame_XXXX.bmp` + `playback.json` (fps, product, params). |
| 6 | Dialogs default sample paths via `PLUGIN_SAMPLE_DIR` or `out/data/plugin/`. |
| 7 | Scope products: traffic, flood, orthogrid, orthogrid3d only (world3d/print later). |

### Pipeline

```
Import �� run_processing �� ResultArtifact �� CommitLayer(MapScene)
                                        �� Export(file)
                                        �� ResultPlayback(2D+3D frames)
```

### Non-goals

- Video container encode; OSRM / full hydrology; Model Builder in this drop.
- Moving kernels into `plugin/`.

---

## ��mine / stratum����ɽ��ά�ز㣩��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **1** �� dedicated product package `smartgis.mine` at `src/plugin/product/mine/` (do **not** fold into `world3d`); kernels in `gis/analysis/geology`.  
**Plan:** [`../plans/2026-09-30-mine-stratum-earthwork.md`](../archive/plans/2026-09-30-mine-stratum-earthwork.md)

### Goal

Ship a mine / stratum product: load borehole contacts, interpolate stratum TIN surfaces, compute a coarse single-layer prism earthwork volume, and visualize borehole sticks + TIN first (closed solid mesh as a follow-on command).

### Locked

| # | Choice |
| --- | --- |
| 1 | Product package **`smartgis.mine`** under `src/plugin/product/mine/` �� Approach **1**; **not** folded into `world3d`. |
| 2 | Data model: **borehole contacts CSV �� interpolate stratum TIN** (not a multi-DEM stack, not voxel MVP). |
| 3 | Earthwork MVP: **single-layer coarse prism volume**; finer cut/fill + reserve later. |
| 4 | Viz MVP: **borehole sticks + TIN surfaces** first; closed solid mesh as a follow-on command. |
| 5 | Kernels in `src/gis/analysis/geology/`: `load_boreholes_csv`, `interpolate_stratum_tin`, `prism_volume_between`. Catalog ids: `native.stratum_interpolate`, `native.stratum_prism_volume`. |
| 6 | Commands: `mine.load_boreholes`, `mine.interpolate_stratum`, `mine.prism_volume`. |
| 7 | Browser installs `set_mine_stratum_writer` in `Browser::init` (same pattern as traffic/flood/world3d). Sample: `testing/data/plugin/mine_boreholes.csv`. Harness: `plugin.mine`. |
| 8 | No algorithms under `src/plugin/`; no Qt; Views + Skia only. Template matches traffic/flood: `commands` + `manifest` + `processing` / `views` / `resources` / `tests`. Register in `PluginShell::start_builtins`. |

### Catalog args (MVP)

- `native.stratum_interpolate`: borehole contacts path / in-memory contacts �� stratum TIN(s) (layer id / contact elevations).
- `native.stratum_prism_volume`: upper/lower TIN (or single layer vs reference) �� coarse prism volume scalar (+ optional export).

### Non-goals

- Multi-DEM stack or voxel / block-model MVP.
- Fine cut/fill, reserve estimation, or closed solid mesh in the first drop (follow-on).
- Folding mine into `smartgis.world3d` or putting geology kernels under `src/plugin/`.

---

## ��stormsurge 3D disaster��P1�CP3���� 2026-09-30

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **1** �� dedicated product package `smartgis.stormsurge` at `src/plugin/product/stormsurge/` (do **not** fold into `flood`); coastal / storm-surge kernels in `gis/analysis`; product only loads, orchestrates, and writes viz.  
**Plan:** [`../plans/2026-09-30-stormsurge-3d-disaster.md`](../plans/2026-09-30-stormsurge-3d-disaster.md)

### Goal

Ship a **��ά�籩��** disaster product: DEM + coast + tide (or water-level) series �� inundation mask / depth / polygons �� map2d + scene3d visualization. Full roadmap is **P1 / P2 / P3** below; **P2 stats** (area / depth-class / buffer-overlap) are in scope for the current code cycle.

### Locked

| # | Choice |
| --- | --- |
| 1 | Product package **`smartgis.stormsurge`** under `src/plugin/product/stormsurge/` �� Approach **1**; **not** folded into `smartgis.flood`. |
| 2 | Kernel ownership: `src/gis/analysis/raster/dem/` �� mask/depth via `storm_surge`; **P2** stats via `storm_surge_stats`. Catalog ids: **`native.storm_surge`**, **`native.storm_surge_stats`**. Thin `plugin::` forward only; no algorithms under `src/plugin/`. |
| 3 | P1 pipeline: **DEM + coast polyline/polygon + tide (water-level) time series** �� raster mask + optional depth raster + optional inundation polygons �� product writers. |
| 4 | Viz P1 order: **mask-on-DEM first**, then **water-surface mesh** (same run may emit both; mesh is still P1, not deferred to P2). Browser installs `set_stormsurge_writer` (map2d + scene3d) in `Browser::init` (same pattern as traffic/flood/world3d). Unset writers �� structured `no_stormsurge_seam` JSON. |
| 5 | Samples: **synthetic fixtures** under `testing/data/plugin/` for harness (tiny DEM + coast + tide + optional impact GeoJSON); **optional** China coastal showcase path when real coastal DEM/coastline assets exist under `out/data/`. |
| 6 | Template matches traffic/flood: `commands` + `manifest` + `processing` / `views` / `resources` / `tests`. Register in `PluginShell::start_builtins`. |
| 7 | Product commands: `stormsurge.run`, `stormsurge.export`, `stormsurge.load_coast`, `stormsurge.stats`, `stormsurge.about`. |

### Phased delivery

| Phase | Deliverable | Code cycle |
| --- | --- | --- |
| **P1** | Kernel `native.storm_surge` + product package + mask-on-DEM then water-surface mesh writers + synthetic harness (+ optional China coastal showcase) | Landed / in flight |
| **P2** | Area / depth-class summary; buffer & overlap stats vs assets / admin polygons (`native.storm_surge_stats`, `stormsurge.stats`) | **Implement now** |
| **P3** | NetCDF / grid water-surface import; optional simplified tide/wind drivers; optional `DomainKind::kStormSurge` domain hook (vista/domain sibling to atmosphere �� Effect later) | Follow-on |

### Catalog args

- `native.storm_surge`: `dem`, `coast` (polyline/polygon path), `tide` (series path or scalar `water_level` / `water_depth`), `output` (mask and/or depth and/or polygons), optional `frames` + `frames_dir` for time-step animation.
- `native.storm_surge_stats`: `mask` (Byte wet GeoTIFF), optional `depth` (Float32), optional `impact` (buildings/roads GeoJSON), optional `buffer_distance`, optional `depth_breaks` (default `[0.5,1,2]` �� classes 0�C0.5 / 0.5�C1 / 1�C2 / >2), `output` (stats JSON); optional `class_mask_output`, `buffer_output`, `overlap_output`.

### Product command sketch

| Command | Role |
| --- | --- |
| `stormsurge.load_coast` | Import / register coast geometry (and optional default sample path). |
| `stormsurge.run` | Run `native.storm_surge` with dialog / CLI args; commit via writer. Optional `stats_output` / `impact` / `depth_output` triggers P2 stats in-process. |
| `stormsurge.export` | Explicit export of mask / depth / polygons / frame sequence. |
| `stormsurge.stats` | Run `native.storm_surge_stats` on last mask (or explicit paths); write report JSON + optional class mask / buffer / overlap layers. |
| `stormsurge.about` | Package id, version, kernel catalog ids, non-goals note. |

### Non-goals

- Full hydrodynamic / 2D shallow-water / ADCIRC-class engine in P1/P2.
- Folding storm surge into `smartgis.flood` or putting coastal kernels under `src/plugin/`.
- Implementing P3 NetCDF / `DomainKind::kStormSurge` Effect in the P2 code drop (document only until that phase opens).

---

## ��geochem ����ѧ������2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **A** �� copy flood/traffic/mine skeleton; kernels in `src/gis/analysis/geochem/` (not inside plugin).  
**Plan:** [`../plans/2026-09-30-geochem-analysis-plugin.md`](../archive/plans/2026-09-30-geochem-analysis-plugin.md)

### Goal

Ship **`smartgis.geochem`** (����ѧ����): sample-point graded colors + legend overlay, **full-extent IDW heat interpolation raster** over the padded sample bbox (plus optional anomaly mask GeoTIFF), and a stats panel (histogram / correlation / background�Cthreshold).

### Locked

| # | Choice |
| --- | --- |
| 1 | Product package **`smartgis.geochem`** under `src/plugin/product/geochem/` �� Architecture **A**; **not** folded into mine/flood. |
| 2 | Kernels in `src/gis/analysis/geochem/`: CSV/OGR load, stats, IDW, grade legend, heat normalize (`geochem_heat_score`). Catalog ids: **`native.geochem_stats`**, **`native.geochem_idw`**. |
| 3 | Data input **both**: selected Point layer in map session (`use_active_layer` + `GeochemLayerReader`) **and** CSV import (lon/lat + multi-element). Optional OGR vector path. |
| 4 | Viz: graded sample circles + **full-extent** `geochem_heat_raster` fill (`heat` 0..100 interpolate ramp, orthogrid-style) via `GeochemWriter` in `Browser::init`. Unset �� `file_only` / structured seam error. |
| 5 | Sample: `testing/data/plugin/geochem/geochem_samples.csv` (~120 pts). Harness: `plugin.geochem`. |
| 6 | Template matches traffic/flood/mine: `commands` + `manifest` + `views` / `resources` / `tests`. Register in `PluginShell::start_builtins`. |
| 7 | No algorithms under `src/plugin/`; no Qt; Views + Skia only. |

### Catalog args (MVP)

- `native.geochem_stats`: `input` (CSV), `element`, optional `bins`, `k_sigma`, `correlate`, `output` (JSON).
- `native.geochem_idw`: `input`, `element`, `output` (Float32 GeoTIFF full-extent heat surface); optional `cells`, `power`, `threshold`, `k_sigma`, `mask_output` (Byte anomaly).

### Product commands

| Command | Role |
| --- | --- |
| `geochem.load` | Load CSV / vector / active layer �� graded samples. |
| `geochem.stats` | Histogram + background/threshold (+ optional Pearson `r`). |
| `geochem.analyze` | Stats + grade + IDW heat raster; commit via writer. |
| `geochem.style_apply` | Re-apply grade legend to last samples. |
| `geochem.about` | Package / catalog ids. |

### Non-goals

- 3D geochem volumes, full multivariate suite, LIMS.
- Merging with mine / stormsurge products.
- Native DEM-style GeoTIFF underlay paint (heat is axis-aligned polygon cells, same as orthogrid).

---

## ��report browser capability��2026-09-30��

**Status:** active  
**Updated:** 2026-09-30  
**Approach:** **1** �� shell-owned `ReportBrowser` + Host API; plugins emit static HTML packs.  
**Plan:** [`../plans/2026-09-30-plugin-report-browser.md`](../plans/2026-09-30-plugin-report-browser.md)  
**Shell UI:** [`2026-09-27-views-desktop-shell-design.md`](2026-09-27-views-desktop-shell-design.md) ��Report dock

### Goal

Analysis / system plugins generate **local HTML+JS** reports (vendored chart libs) and show them in a built-in report browser. Default **no network**. Not a CEF product chrome (archived CEF shell stays rejected).

### Locked

| # | Choice |
| --- | --- |
| 1 | Purpose: **local reports only** (no address bar; block `http`/`https` navigation). |
| 2 | Abstraction: `plugin::ReportBrowser` (`navigate(dir)`, `post_json`, `close`). |
| 3 | v1 backend: **WebView2**; CEF backend optional later behind the same interface. |
| 4 | UI: dedicated **Report** inspector dock (shell-owned). |
| 5 | Content: static pack (`index.html` + libs + `data.json`) under allowed roots; optional `post_to_report(json)`. |
| 6 | Host API (append on `content::PluginHost`): `open_report` / `post_to_report` / `close_report` + `set_report_bridge` installed by Views shell. Python: `smartgis.Host` same names. |
| 7 | Path allowlist: plugin resource roots + session report dirs; reject arbitrary paths. |
| 8 | Missing WebView2 Runtime: soft-fail (status text / false); harness may SKIP. |

### Non-goals

- Reopening CEF / WinUI product chrome.
- Per-plugin WebView instances.
- CDN / online chart libraries by default.
- Replacing map2d/scene3d Writers or AnalysisPlayback playback.

---
