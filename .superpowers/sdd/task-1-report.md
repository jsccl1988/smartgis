# Task 1 Report: Providers and GDAL open targets

**Status:** DONE  
**Commit:** `c224770` — Add OGR DB provider ids and GDAL open-target helper. (was `6cb270b` before message rewrite)

## Summary

Added three OGR DB provider enum values (`PROVIDER_POSTGRES`, `PROVIDER_GPKG`, `PROVIDER_SPATIALITE`) to `eSmtDBProvider`, implemented `db_provider_traits<Provider>` (C++17, MSVC-safe) with thin runtime free-function dispatch, wired `ogr_codec` / `sde_gdal` / `sde_gdal_test` in GN, and registered the test on `//:test_all`.

## TDD Evidence

### RED (Step 2)

Created test + GN wiring without `ogr_connect.h` / `ogr_connect.cc`:

```
ninja: error: '../src/sdb/datasource/gdal/ogr_connect.cc', needed by
'obj/src/sdb/datasource/gdal/ogr_codec/ogr_connect.obj', missing and no known rule to make it
Exit code: 1
```

### GREEN (Step 4)

After implementation (including `db_provider_traits` refactor):

```
> out\sde_gdal_test.exe
sde_gdal_test connect checks ok

> build.bat te
Exit code: 0
exe_smoke: 5 passed, 0 failed, 0 skipped
```

All brief assertions pass: provider support flags, GDAL driver names, GPKG file path, PostgreSQL `PG:` connection string, ACCESS empty target.

## Implementation

### Provider enum (`src/sdb/layer/layer.h`)

Appended after `PROVIDER_MYSQL` (no reorder):

- `PROVIDER_POSTGRES` — PostgreSQL / PostGIS
- `PROVIDER_GPKG` — GeoPackage file
- `PROVIDER_SPATIALITE` — SpatiaLite file

### `db_provider_traits<Provider>` (`ogr_connect.h` / `.cc`)

Primary template:

```cpp
template <uint Provider>
struct db_provider_traits {
  static constexpr bool supported = false;
  static constexpr const char* driver_name = nullptr;
  static std::string open_target(const Smt_GIS::SmtDataSourceInfo& info);
};
```

Full specializations for `PROVIDER_GPKG`, `PROVIDER_POSTGRES`, `PROVIDER_SPATIALITE` hold `supported = true`, `driver_name`, and `open_target()` (defined out-of-line in `.cc` for MSVC).

Shared helpers in anonymous namespace:

- `make_file_open_target` — GPKG / SpatiaLite path join
- `make_postgres_open_target` — `PG:host=… port=… dbname=… user=… password=…`

### Free-function dispatch (brief API unchanged)

| Function | Dispatch |
| --- | --- |
| `is_db_provider_supported(uint)` | `switch` → `db_provider_traits<P>::supported` |
| `gdal_driver_name(uint)` | `switch` → `db_provider_traits<P>::driver_name` |
| `make_gdal_open_target(info)` | `switch` → `db_provider_traits<P>::open_target(info)` |

`sde_gdal_test.cc` is verbatim from the brief; no test changes required.

### GN wiring

- `ogr_codec` source_set — `ogr_connect.h/.cc`
- `sde_gdal` shared library — `gdal_driver.cc/.h` (still returns `false`)
- `sde_gdal_test` executable
- `//:test_all` includes `//src/sdb/datasource/gdal:sde_gdal_test`

**Build fixes beyond brief text (required for green build on MSVC /C++17):**

1. `ogr_codec`: `configs += [ "//build:legacy" ]` + `public_configs` so `layer.h` transitive includes resolve for the test target.
2. Extra link deps on `ogr_codec` / `sde_gdal_test`: `//src/algorithm/geo:geo`, `//src/base:base` (inline methods in `layer.h` pull geometry symbols).
3. `snprintf` instead of `std::snprintf` (MSVC `/std:c++17` does not expose `std::snprintf`).
4. Parent deps: `src/sdb/BUILD.gn` and `src/sdb/datasource/BUILD.gn` reference `sde_gdal` instead of removed `gdal_seam`.

## Self-Review

| Check | Result |
| --- | --- |
| Brief test API unchanged | Yes — `sde_gdal_test.cc` matches brief exactly |
| No OgrDataSource / feature codec | Yes — only connect helpers |
| `register_gdal_driver()` still false | Yes |
| C++17 only (no concepts/NTTP/C++23) | Yes |
| Namespaces / snake_case on new free functions | Yes — `sdb::datasource` |
| Copyright headers on new files | Yes — 2026 Mogu Authors |
| `build.bat te` green | Yes |
| No second GDAL / SOCI / Qt | Yes |

**Concerns:**

- Commit includes `BUILD.gn`, `src/sdb/BUILD.gn`, `src/sdb/datasource/BUILD.gn`, and `gdal_driver.*` in addition to brief `git add` list — required for GN graph and DLL stub to compile.
- `layer.h` committed as a new file (promoted layout); enum append is the functional change for this task.
- Initial mistaken commit including unrelated staged renames was reset; final commit `6cb270b` contains only 10 task-scoped files.

## Files Changed

| File | Action |
| --- | --- |
| `src/sdb/layer/layer.h` | Modified — provider enum values |
| `src/sdb/datasource/gdal/ogr_connect.h` | Created — traits + free fn declarations |
| `src/sdb/datasource/gdal/ogr_connect.cc` | Created — trait bodies + dispatch |
| `src/sdb/datasource/gdal/sde_gdal_test.cc` | Created — connect checks test |
| `src/sdb/datasource/gdal/BUILD.gn` | Created — ogr_codec, sde_gdal, test |
| `src/sdb/datasource/gdal/gdal_driver.h/.cc` | Created — stub (pre-existing seam) |
| `BUILD.gn` | Created — test_all wiring |
| `src/sdb/BUILD.gn` | Created — sde_gdal dep |
| `src/sdb/datasource/BUILD.gn` | Created — sde_gdal dep |

## Co-author trailer fix (review finding)

**Status:** BLOCKED — `6cb270b` is not HEAD; cannot amend without history rewrite.

### Pre-check

```
git log -1 --format="%H%n%an %ae%n%s%n%b"
cbf3dc233acb2a462c70104238d598d37c4361f3
jsccl1988 jsccl1988@.com
Document C++23 Mojo IPC and MapContents design.
Lock Chromium-style invitation, mojom v1, traits, and repo-wide C++23 before implementation.

Co-authored-by: Cursor <cursoragent@cursor.com>

git status -sb
## master...origin/master [ahead 3]
```

Target commit (not HEAD):

```
git log -1 --format="%H%n%an %ae%n%s%n%b" 6cb270b
6cb270ba8141b7abf47240c9cb79e69cac262a7a
jsccl1988 jsccl1988@.com
Add OGR DB provider ids and GDAL open-target helper.
Co-authored-by: Cursor <cursoragent@cursor.com>
```

Recent log: `cbf3dc2` (HEAD) → `6cb270b` (OGR task) → `d84cc83` → …

Branch is **ahead 3, not pushed**. Amending `6cb270b` would require rebasing `cbf3dc2` on top of a rewritten parent; interactive rebase is disallowed and `git commit --amend` only applies to HEAD.

### Command used

None — no safe amend path for a non-HEAD commit under current constraints.

### New HEAD SHA

Unchanged: `cbf3dc233acb2a462c70104238d598d37c4361f3` (OGR commit remains `6cb270ba8141b7abf47240c9cb79e69cac262a7a` with Cursor trailer).

### Proof (6cb270b message still has trailer)

```
git log -1 --format="%B" 6cb270b
Add OGR DB provider ids and GDAL open-target helper.
Co-authored-by: Cursor <cursoragent@cursor.com>
```

### Test

```
out\sde_gdal_test.exe
sde_gdal_test connect checks ok
```

## Co-author trailer fix (message-only rewrite)

**Status:** DONE — `git filter-branch --msg-filter` on `origin/master..HEAD` (2026-09-13).

### Method

```
git stash push -m "temp-before-msg-rewrite"
FILTER_BRANCH_SQUELCH_WARNING=1 git filter-branch -f \
  --msg-filter "py -3 C:/Dev/src/gis/smartgis/.superpowers/sdd/strip_cursor_trailer.py" \
  origin/master..HEAD
git stash pop
```

Strips `Co-authored-by:` lines naming Cursor / Cursor Agent / `cursoragent@cursor.com`; tidies trailing blank lines. No file-tree changes; no force-push.

### Old → new SHAs

| Commit | Old | New |
| --- | --- | --- |
| Promote product sources to src/ | `d84cc83` | `04d0125` |
| Add OGR DB provider ids (task commit) | `6cb270b` | `c224770` |
| Document C++23 Mojo IPC design | `cbf3dc2` | `09fb26d` |
| Raise product C++ standard to C++20 | `bcc4b26` | `daf75ca` |
| Specify map UI event forwarding (HEAD) | `2efcb0b` | `db012c9` |

### Proof (no Cursor trailers)

```
git log origin/master..HEAD --format="%h %s%n%b%n---"
db012c9 Specify Chromium-style map UI event forwarding.
MapWidgetHostView takes native input; GPU only sees DispatchPointer and IME commit.

---
daf75ca Raise product C++ standard to C++20 and lock it in Cursor rules.

---
09fb26d Document C++23 Mojo IPC and MapContents design.
Lock Chromium-style invitation, mojom v1, traits, and repo-wide C++23 before implementation.

---
c224770 Add OGR DB provider ids and GDAL open-target helper.

---
04d0125 Promote product sources to src/ and drop leftover branches/ and vs2008/.
GN forced-include now lives at build/smt_compat.h so the build no longer depends on the old sln tree.

---
```

```
git status -sb
## master...origin/master [ahead 5]
```

### Test (post-rewrite)

```
out\sde_gdal_test.exe
sde_gdal_test connect checks ok
```
