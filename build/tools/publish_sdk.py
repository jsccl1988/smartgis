# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Publish Debug and Release SDKs directly from a flat gen root.

Compile output stays in out/Debug and out/Release (exe, dll, import lib, and
PDB beside each other). This script is the only step that builds:

  out/sdk/<Config>/{include,lib,bin,pdb}
  out/sdk/<Config>/third_party/{include,lib,bin,pdb}

  python build/tools/publish_sdk.py --out Debug
  python build/tools/publish_sdk.py --out Release
"""

from __future__ import annotations

import argparse
import re
import shutil
import struct
import sys
from pathlib import Path

# Product images a consumer runs. Test and bench exes stay in the gen root.
PRODUCT_EXES = (
    "SmartGIS.exe",
    "SmartGisRender.exe",
    "UiDesigner.exe",
)

# Header trees copied under include/ with paths relative to src/.
HEADER_TREES = (
    "base",
    "content",
    "gis",
    "gpu",
    "net",
    "plugin",
    "render",
    "scenic",
    "tool",
    "ui",
    "vista",
)

HEADER_SUFFIXES = {".h", ".hh", ".hpp", ".hxx", ".inl", ".inc"}
SKIP_DIR_NAMES = {"testing", "test", "tests"}
SKIP_TP_INCLUDE_DIRS = {"benchmark", "gmock", "gtest"}
SKIP_TP_LIB_MARKERS = ("benchmark", "gmock", "gtest")

# Product runtime copied under bin/. gdalplugins is third-party.
PRODUCT_RUNTIME_DIRS = ("plugins",)
# Gen-root DLLs that are not built by SmartGIS and are not in .install/bin.
REDIST_DLLS = {
    "cximage.dll",
    "d3dcompiler_47.dll",
    "dxcompiler.dll",
    "dxil.dll",
    "python3.dll",
    "python312.dll",
    "webview2loader.dll",
}


def repo_root_from_here() -> Path:
    return Path(__file__).resolve().parents[2]


def is_skipped_header(path: Path) -> bool:
    if path.suffix.lower() not in HEADER_SUFFIXES:
        return True
    if any(part.lower() in SKIP_DIR_NAMES for part in path.parts):
        return True
    stem = path.name.lower()
    return stem.endswith("_test.h") or stem.endswith("_unittest.h") or stem.endswith(
        "_unittests.h"
    )


def copy_file(src: Path, dest: Path) -> None:
    dest.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(src, dest)


def copy_tree_filtered(src_root: Path, dest_root: Path, skip) -> int:
    count = 0
    if not src_root.is_dir():
        return 0
    for path in src_root.rglob("*"):
        if not path.is_file() or skip(path):
            continue
        rel = path.relative_to(src_root)
        copy_file(path, dest_root / rel)
        count += 1
    return count


def publish_include(root: Path, include_dir: Path) -> int:
    count = 0
    src = root / "src"
    for tree in HEADER_TREES:
        count += copy_tree_filtered(
            src / tree, include_dir / tree, is_skipped_header
        )
    return count


def publish_third_party_include(root: Path, include_dir: Path) -> int:
    count = 0
    tp_include = root / "third_party" / ".install" / "include"
    if not tp_include.is_dir():
        return 0
    for path in tp_include.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(tp_include)
        if rel.parts and rel.parts[0].lower() in SKIP_TP_INCLUDE_DIRS:
            continue
        copy_file(path, include_dir / rel)
        count += 1
    return count


def skip_tp_lib(path: Path) -> bool:
    name = path.name.lower()
    if path.suffix.lower() not in {".lib"}:
        return True
    return any(marker in name for marker in SKIP_TP_LIB_MARKERS)


def third_party_bin_names(root: Path) -> set[str]:
    """DLL/EXE names shipped by third_party/.install/bin, plus known redistributables."""
    names = set(REDIST_DLLS)
    bin_dir = root / "third_party" / ".install" / "bin"
    if bin_dir.is_dir():
        for path in bin_dir.iterdir():
            if path.is_file() and path.suffix.lower() in {".dll", ".exe"}:
                names.add(path.name.lower())
    return names


def is_third_party_image(filename: str, tp_names: set[str]) -> bool:
    if filename.lower() in tp_names or filename.lower() in REDIST_DLLS:
        return True
    stem = module_stem(filename)
    suffix = filename[len(stem) :]
    marker = _strip_debug_marker(stem)
    if marker and (marker[0] + suffix).lower() in tp_names:
        return True
    # Debug twin of a release name that lives in the install prefix.
    for candidate in (stem + "_d", stem + "-d", stem + "d"):
        if (candidate + suffix).lower() in tp_names:
            return True
    return False


_MODULE_SUFFIXES = (
    ".dll.lib",
    ".dll.pdb",
    ".exe.pdb",
    ".dll",
    ".exe",
    ".lib",
    ".pdb",
)


def module_stem(filename: str) -> str:
    """``base_d.dll.lib`` and ``gdald.lib`` both reduce to a pairable stem."""
    lower = filename.lower()
    for suffix in _MODULE_SUFFIXES:
        if lower.endswith(suffix):
            return filename[: -len(suffix)]
    return Path(filename).stem


def _strip_debug_marker(stem: str) -> tuple[str, str] | None:
    """Return ``(release_stem, how)``. ``how`` is ``explicit`` for ``_d`` / ``-d``.

    ``explicit`` matches ``base_d``, ``proj_9_4_d``, ``libcurl-d``, ``libcurl-d_imp``,
    ``geotiff_d_i``. ``trailing`` matches ``geosd``, ``gdald``, ``zlibstaticd``,
    ``zlibd1`` and is only a debug marker when that release stem is present
    (so ``world3d`` stays neutral).
    """
    stripped = re.sub(r"([_-])d(?=_|$)", "", stem, count=1, flags=re.IGNORECASE)
    if stripped != stem:
        return stripped, "explicit"
    numbered = re.fullmatch(r"(.*)d(\d+)", stem, re.IGNORECASE)
    if numbered:
        return numbered.group(1) + numbered.group(2), "trailing"
    if len(stem) > 1 and stem.lower().endswith("d"):
        return stem[:-1], "trailing"
    return None


def variant_kind(filename: str, sibling_names: set[str]) -> str:
    """Return ``debug``, ``release``, or ``neutral`` for one file in a directory.

    ``sibling_names`` is the set of filenames in that directory. Debug/Release
    SDKs each keep one half of a pair; files with no twin are ``neutral``.
    """
    stem = module_stem(filename)
    folded = stem.lower()
    suffix = filename[len(stem) :]
    stems = {module_stem(name).lower(): name for name in sibling_names}

    def has(other_stem: str) -> bool:
        return other_stem.lower() in stems and (other_stem + suffix).casefold() in {
            name.casefold() for name in sibling_names
        }

    marker = _strip_debug_marker(stem)
    if marker:
        release_stem, how = marker
        if how == "explicit" or has(release_stem):
            return "debug"
    for other in sibling_names:
        other_stem = module_stem(other)
        if other_stem.lower() == folded:
            continue
        other_marker = _strip_debug_marker(other_stem)
        if other_marker and other_marker[0].lower() == folded:
            if other_marker[1] == "explicit" or has(other_stem):
                return "release"
    return "neutral"


def keep_for_config(filename: str, sibling_names: set[str], config: str) -> bool:
    kind = variant_kind(filename, sibling_names)
    if kind == "neutral":
        return True
    return (config == "Debug" and kind == "debug") or (
        config == "Release" and kind == "release"
    )


def pdb_recorded_in_image(image: Path) -> str | None:
    """Return the CodeView PDB path stored in a PE, if one is present."""
    try:
        data = image.read_bytes()
    except OSError:
        return None
    needle = b"RSDS"
    start = 0
    found: str | None = None
    while True:
        idx = data.find(needle, start)
        if idx < 0:
            break
        path_at = idx + 4 + 16 + 4
        if path_at >= len(data):
            break
        end = data.find(b"\x00", path_at)
        if end < 0:
            break
        raw = data[path_at:end]
        start = end + 1
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            text = raw.decode("mbcs", errors="replace")
        if text.lower().endswith(".pdb"):
            found = text
            break
    return found


def _copy_image(out_dir: Path, image: Path, bin_dir: Path, lib_dir: Path, pdb_dir: Path, counts: dict[str, int], rel: Path | None = None) -> None:
    dest_name = rel if rel is not None else Path(image.name)
    copy_file(image, bin_dir / dest_name)
    counts["bin"] += 1
    import_lib = out_dir / f"{image.name}.lib"
    if import_lib.is_file() and rel is None:
        copy_file(import_lib, lib_dir / import_lib.name)
        counts["lib"] += 1
    pdb_dest = pdb_dir / f"{dest_name}.pdb"
    _copy_linker_pdb(out_dir, image, pdb_dest)
    if pdb_dest.is_file():
        counts["pdb"] += 1


def pe_imported_dlls(path: Path) -> list[str]:
    """DLL names in the PE import directory. Empty when the image is not a PE."""
    data = path.read_bytes()
    if len(data) < 64 or data[:2] != b"MZ":
        return []
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if e_lfanew + 24 > len(data) or data[e_lfanew : e_lfanew + 4] != b"PE\0\0":
        return []
    _machine, nsections, _ts, _sym, _nsym, opt_size, _chars = struct.unpack_from(
        "<HHIIIHH", data, e_lfanew + 4
    )
    opt = e_lfanew + 24
    if opt + 2 > len(data):
        return []
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic == 0x20B:
        dd_off = opt + 112
    elif magic == 0x10B:
        dd_off = opt + 96
    else:
        return []
    if dd_off + 16 > len(data):
        return []
    import_rva, import_size = struct.unpack_from("<II", data, dd_off + 8)
    if import_rva == 0 or import_size == 0:
        return []
    sec_off = opt + opt_size
    sections: list[tuple[int, int, int]] = []
    for index in range(nsections):
        off = sec_off + index * 40
        if off + 24 > len(data):
            break
        vsz, va, rsz, raw = struct.unpack_from("<IIII", data, off + 8)
        sections.append((va, max(vsz, rsz), raw))

    def rva_to_off(rva: int) -> int | None:
        for va, span, raw in sections:
            if va <= rva < va + span:
                return raw + (rva - va)
        return None

    cursor = rva_to_off(import_rva)
    if cursor is None:
        return []
    names: list[str] = []
    while cursor + 20 <= len(data):
        _ilt, _time, _fwd, name_rva, _iat = struct.unpack_from("<IIIII", data, cursor)
        if name_rva == 0:
            break
        name_off = rva_to_off(name_rva)
        if name_off is None:
            break
        end = data.find(b"\x00", name_off)
        if end < 0:
            break
        names.append(data[name_off:end].decode("ascii", errors="replace"))
        cursor += 20
    return names


def close_third_party_imports(root: Path, tp_bin: Path) -> int:
    """Copy install-prefix DLLs that a shipped third-party image imports.

    Debug ``gdald.dll`` imports ``geos_c.dll`` (not ``geos_cd.dll``). The
    config classifier would drop that release-looking name, so follow the
    import directory instead of the filename pair.
    """
    install = root / "third_party" / ".install" / "bin"
    if not install.is_dir() or not tp_bin.is_dir():
        return 0
    available = {
        path.name.lower(): path
        for path in install.iterdir()
        if path.is_file() and path.suffix.lower() == ".dll"
    }
    copied = 0
    queue = [path for path in tp_bin.glob("*.dll") if path.is_file()]
    seen: set[str] = set()
    while queue:
        image = queue.pop()
        key = image.name.lower()
        if key in seen:
            continue
        seen.add(key)
        for imported in pe_imported_dlls(image):
            dest = tp_bin / imported
            if dest.is_file():
                if dest.name.lower() not in seen:
                    queue.append(dest)
                continue
            src = available.get(imported.lower())
            if src is None:
                continue
            copy_file(src, dest)
            copied += 1
            queue.append(dest)
    return copied


def publish_bin_lib_pdb(root: Path, out_dir: Path, sdk: Path, config: str) -> dict[str, int]:
    product = {"bin": 0, "lib": 0, "pdb": 0}
    third = {"bin": 0, "lib": 0, "pdb": 0}
    tp_names = third_party_bin_names(root)
    tp_root = sdk / "third_party"

    dlls = sorted(out_dir.glob("*.dll"))
    dll_names = {path.name for path in dlls}
    kept = [path for path in dlls if keep_for_config(path.name, dll_names, config)]
    for image in kept:
        if is_third_party_image(image.name, tp_names):
            _copy_image(out_dir, image, tp_root / "bin", tp_root / "lib", tp_root / "pdb", third)
        else:
            _copy_image(out_dir, image, sdk / "bin", sdk / "lib", sdk / "pdb", product)
    for name in PRODUCT_EXES:
        exe = out_dir / name
        if exe.is_file():
            _copy_image(out_dir, exe, sdk / "bin", sdk / "lib", sdk / "pdb", product)

    _copy_runtime_tree(out_dir, out_dir / "plugins", sdk / "bin", sdk / "pdb", product, config)
    _copy_runtime_tree(
        out_dir, out_dir / "gdalplugins", tp_root / "bin", tp_root / "pdb", third, config
    )
    third["bin"] += close_third_party_imports(root, tp_root / "bin")

    tp_lib = root / "third_party" / ".install" / "lib"
    if tp_lib.is_dir():
        libs = [path for path in tp_lib.iterdir() if path.is_file() and not skip_tp_lib(path)]
        lib_names = {path.name for path in libs}
        for path in libs:
            if not keep_for_config(path.name, lib_names, config):
                continue
            dest = tp_root / "lib" / path.name
            if dest.exists():
                continue
            copy_file(path, dest)
            third["lib"] += 1
    return {"product": product, "third_party": third}


def _copy_runtime_tree(
    out_dir: Path,
    src: Path,
    bin_dir: Path,
    pdb_dir: Path,
    counts: dict[str, int],
    config: str,
) -> None:
    if not src.is_dir():
        return
    by_dir: dict[Path, list[Path]] = {}
    for path in src.rglob("*"):
        if path.is_file():
            by_dir.setdefault(path.parent, []).append(path)
    for files in by_dir.values():
        names = {path.name for path in files}
        for path in files:
            if path.suffix.lower() in {".pdb", ".ilk", ".exp", ".lib"}:
                continue
            if path.suffix.lower() in {".dll", ".exe"} and not keep_for_config(
                path.name, names, config
            ):
                continue
            rel = path.relative_to(src.parent)
            copy_file(path, bin_dir / rel)
            counts["bin"] += 1
            if path.suffix.lower() in {".dll", ".exe"}:
                pdb_rel = Path(str(rel) + ".pdb")
                _copy_linker_pdb(out_dir, path, pdb_dir / pdb_rel)
                if (pdb_dir / pdb_rel).is_file():
                    counts["pdb"] += 1


def _copy_linker_pdb(out_dir: Path, image: Path, dest: Path) -> None:
    """Copy the linker PDB for image. Prefer the pdb/ dir the toolchain writes."""
    rel = image.relative_to(out_dir)
    candidates = [
        out_dir / "pdb" / f"{rel}.pdb",
        out_dir / "pdb" / f"{image.name}.pdb",
        image.with_name(image.name + ".pdb"),
        image.with_suffix(image.suffix + ".pdb"),
    ]
    recorded = pdb_recorded_in_image(image)
    if recorded:
        recorded_path = Path(recorded)
        if not recorded_path.is_absolute():
            recorded_path = (out_dir / recorded_path).resolve()
        candidates.insert(0, recorded_path)
    seen: set[Path] = set()
    for candidate in candidates:
        try:
            key = candidate.resolve()
        except OSError:
            continue
        if key in seen:
            continue
        seen.add(key)
        if candidate.is_file():
            copy_file(candidate, dest)
            return


SKIP_RUNTIME_DATA_DIRS = {"cache"}


def copy_tree(src: Path, dest: Path, skip_dirs: set[str] | None = None) -> int:
    """Copy files under src into dest. Skip directory names in skip_dirs."""
    if not src.is_dir():
        return 0
    skip = {name.lower() for name in (skip_dirs or set())}
    count = 0
    for path in src.rglob("*"):
        if not path.is_file():
            continue
        rel = path.relative_to(src)
        if any(part.lower() in skip for part in rel.parts):
            continue
        copy_file(path, dest / rel)
        count += 1
    return count


def publish_runtime_data(root: Path, sdk: Path) -> dict[str, int]:
    """Sample maps, shell markup, and GDAL/PROJ data the exe resolves at startup.

    SmartGIS.exe lives in sdk/bin. Seed and markup search <exe>/../data and
    <exe>/../ui. PROJ/GDAL search <exe>/../third_party/share.
    """
    share = root / "third_party" / ".install" / "share"
    return {
        "data": copy_tree(root / "out" / "data", sdk / "data", SKIP_RUNTIME_DATA_DIRS),
        "ui": copy_tree(root / "out" / "ui", sdk / "ui"),
        "proj": copy_tree(share / "proj", sdk / "third_party" / "share" / "proj"),
        "gdal": copy_tree(share / "gdal", sdk / "third_party" / "share" / "gdal"),
    }


def publish(root: Path, config: str) -> Path:
    """Assemble out/sdk/<Config> from the flat gen root and source trees."""
    out_dir = root / "out" / config
    if not out_dir.is_dir():
        raise SystemExit(f"ERROR: missing gen root {out_dir}")
    sdk = root / "out" / "sdk" / config
    if sdk.exists():
        shutil.rmtree(sdk)
    sdk.mkdir(parents=True)
    (sdk / "SDK_CONFIG.txt").write_text(
        f"{config}\nthird_party=third_party\n", encoding="utf-8"
    )
    include_n = publish_include(root, sdk / "include")
    tp_include_n = publish_third_party_include(root, sdk / "third_party" / "include")
    counts = publish_bin_lib_pdb(root, out_dir, sdk, config)
    product = counts["product"]
    third = counts["third_party"]
    runtime = publish_runtime_data(root, sdk)
    print(
        f"publish_sdk: config={config} {sdk} "
        f"include={include_n} lib={product['lib']} bin={product['bin']} pdb={product['pdb']} "
        f"third_party include={tp_include_n} lib={third['lib']} bin={third['bin']} pdb={third['pdb']} "
        f"data={runtime['data']} ui={runtime['ui']} proj={runtime['proj']} gdal={runtime['gdal']}"
    )
    return sdk


def _self_check() -> None:
    debug_bin = {
        "base_d.dll",
        "geos.dll",
        "geosd.dll",
        "geos_c.dll",
        "geos_cd.dll",
        "libcurl.dll",
        "libcurl-d.dll",
        "proj_9_4_d.dll",
        "zlibd1.dll",
        "flood.dll",
        "d3dcompiler_47.dll",
    }
    assert variant_kind("base_d.dll", debug_bin) == "debug"
    assert variant_kind("geos.dll", debug_bin) == "release"
    assert variant_kind("geosd.dll", debug_bin) == "debug"
    assert variant_kind("geos_cd.dll", debug_bin) == "debug"
    assert variant_kind("libcurl-d.dll", debug_bin) == "debug"
    assert variant_kind("libcurl.dll", debug_bin) == "release"
    assert variant_kind("proj_9_4_d.dll", debug_bin) == "debug"
    assert variant_kind("zlibd1.dll", {"zlibd1.dll", "zlib1.dll"}) == "debug"
    assert variant_kind("zlib1.dll", {"zlibd1.dll", "zlib1.dll"}) == "release"
    assert variant_kind("flood.dll", debug_bin) == "neutral"
    assert variant_kind("d3dcompiler_47.dll", debug_bin) == "neutral"
    assert keep_for_config("geos.dll", debug_bin, "Debug") is False
    assert keep_for_config("geosd.dll", debug_bin, "Debug") is True
    assert keep_for_config("geos.dll", {"geos.dll"}, "Release") is True
    assert keep_for_config("gdald.lib", {"gdal.lib", "gdald.lib"}, "Release") is False
    assert keep_for_config("gdal.lib", {"gdal.lib", "gdald.lib"}, "Release") is True
    assert module_stem("base_d.dll.lib") == "base_d"
    curl_libs = {"libcurl-d_imp.lib", "libcurl_imp.lib", "libcurl.lib", "curl.lib"}
    assert variant_kind("libcurl-d_imp.lib", curl_libs) == "debug"
    assert variant_kind("libcurl_imp.lib", curl_libs) == "release"
    assert variant_kind("libcurl.lib", curl_libs) == "neutral"
    assert variant_kind("world3d.dll", {"world3d.dll"}) == "neutral"
    geotiff = {"geotiff_d_i.lib", "geotiff_i.lib", "geotiff_d.lib", "geotiff.lib"}
    assert variant_kind("geotiff_d_i.lib", geotiff) == "debug"
    assert variant_kind("geotiff_i.lib", geotiff) == "release"
    tp_names = {"gdald.dll", "geos.dll", "geosd.dll", "libcurl-d.dll"}
    assert is_third_party_image("gdald.dll", tp_names)
    assert is_third_party_image("geosd.dll", tp_names)
    assert is_third_party_image("d3dcompiler_47.dll", tp_names)
    assert not is_third_party_image("base_d.dll", tp_names)
    assert not is_third_party_image("flood.dll", tp_names)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--check",
        action="store_true",
        help="Run the Debug/Release pair classifier self-check and exit",
    )
    parser.add_argument(
        "--out",
        choices=("Debug", "Release"),
        help="Gen-root config to publish (required unless --check)",
    )
    parser.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Repo root (default: inferred from this file)",
    )
    args = parser.parse_args(argv)
    if args.check:
        _self_check()
        print("publish_sdk: self-check ok")
        return 0
    if not args.out:
        parser.error("--out is required")
    root = args.root.resolve() if args.root else repo_root_from_here()
    publish(root, args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
