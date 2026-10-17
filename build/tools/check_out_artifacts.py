# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Pre/post-compile gate: PE writability + ninja closure + PE import consistency.

Catches stale or half-linked products (e.g. LNK1168 left new DLLs + old
SmartGIS.exe) before runners start, and after every compile.

Layers:
  L0 writable — exclusive open on existing closure PEs; list lock holders
  L1 ninja    — ``ninja -n`` on SmartGIS + import DLLs (or test_all)
  L2 pe       — PE import table vs ninja closure + DLL-newer-than-exe skew

Usage:
  python build/tools/check_out_artifacts.py --out Debug --mode writable
  python build/tools/check_out_artifacts.py --out Debug --mode product
  python build/tools/check_out_artifacts.py --out Debug --mode harness
  python build/tools/check_out_artifacts.py --out Debug --mode te
"""

from __future__ import annotations

import argparse
import re
import struct
import subprocess
import sys
from pathlib import Path

# Keep in sync with build.bat te runner list. Names that are not ninja targets
# are treated as optional leftovers (warn only).
TE_EXES: tuple[str, ...] = (
    "rhi_test.exe",
    "model_test.exe",
    "scene_test.exe",
    "scene_gpu_test.exe",
    "unified_draw_test.exe",
    "leftover_mesh_test.exe",
    "ogr_text_encoding_test.exe",
    "sdbd_client_test.exe",
    "sdbd_live_test.exe",
    "sde_gdal_test.exe",
    "geo_ogr_test.exe",
    "proj_test.exe",
    "stat_expr_test.exe",
    "tin_delaunay_test.exe",
    "orthogrid_laplace_test.exe",
    "net_test.exe",
    "tool_dispatch_test.exe",
    "draft_test.exe",
    "camera_nav_test.exe",
    "content_tool_session_test.exe",
    "feature_attrs_test.exe",
    "content_catalog_layers_test.exe",
    "land_mask_test.exe",
    "views_unittests.exe",
    "markup_unittests.exe",
    "views_pixel_tests.exe",
    "ipc_test.exe",
    "render_backend_test.exe",
    "tile_test.exe",
    "style_test.exe",
    "map2d_test.exe",
    "map2d_pass_test.exe",
    "gis_scene_test.exe",
    "scene3d_presenter_test.exe",
    "dem_raster_test.exe",
    "scenic_gdi_map_paint_test.exe",
    "scenic_map_carto2d_test.exe",
    "scenic_gl_map_paint_test.exe",
    "scenic_dem_stereo_test.exe",
    "menu_test.exe",
    "select_query_test.exe",
    "plugin_host_test.exe",
    "processing_ops_test.exe",
    "plugin_builtins_test.exe",
    "views_launch_options_test.exe",
)

_PRODUCT_EXE = "SmartGIS.exe"
_WORK_LINE = re.compile(r"^\[\d+/\d+\]\s+")
# System / CRT DLLs that are not part of the out/ ninja closure.
_SYSTEM_DLL_PREFIXES: tuple[str, ...] = (
    "api-ms-",
    "ext-ms-",
    "vcruntime",
    "msvcp",
    "msvcr",
    "ucrtbase",
    "concrt",
    "vcomp",
    "kernel32",
    "kernelbase",
    "ntdll",
    "user32",
    "gdi32",
    "advapi32",
    "shell32",
    "ole32",
    "oleaut32",
    "combase",
    "rpcrt4",
    "ws2_32",
    "winmm",
    "imm32",
    "setupapi",
    "version",
    "shlwapi",
    "crypt32",
    "bcrypt",
    "sechost",
    "clbcatq",
    "comdlg32",
    "winspool",
    "opengl32",
    "glu32",
    "d3d11",
    "d3d12",
    "dxgi",
    "d3dcompiler",
    "dwmapi",
    "uxtheme",
    "dbghelp",
    "psapi",
    "iphlpapi",
    "secur32",
    "netapi32",
    "userenv",
    "wtsapi32",
    "mfplat",
    "mf",
    "mfreadwrite",
    "dxva2",
)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _find_ninja(root: Path) -> Path | None:
    cand = root / "build" / "bin" / "ninja.exe"
    if cand.is_file():
        return cand
    from shutil import which

    w = which("ninja")
    return Path(w) if w else None


def _run(
    ninja: Path, out_dir: Path, args: list[str], *, timeout: float = 120.0
) -> subprocess.CompletedProcess[str]:
    return subprocess.run(
        [str(ninja), "-C", str(out_dir), *args],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
        timeout=timeout,
        check=False,
    )


def _is_ninja_target(ninja: Path, out_dir: Path, name: str) -> bool:
    proc = _run(ninja, out_dir, ["-t", "query", name])
    if proc.returncode != 0:
        return False
    head = (proc.stdout or "").lstrip()
    return head.startswith(name + ":") or head.startswith(name + "\n")


def _query_import_dlls(ninja: Path, out_dir: Path, exe_name: str) -> list[str]:
    proc = _run(ninja, out_dir, ["-t", "query", exe_name])
    if proc.returncode != 0:
        return []
    dlls: list[str] = []
    for line in (proc.stdout or "").splitlines():
        tok = line.strip()
        if tok.startswith("|"):
            tok = tok[1:].strip()
        # External import libs (WebView2Loader.dll.lib under third_party/) are
        # not gen-root outputs. Keep only the filename of product *.dll.lib.
        name = Path(tok).name
        if name.endswith(".dll.lib"):
            dlls.append(name[: -len(".lib")])
        elif name.endswith(".dll") and ("/" not in tok and "\\" not in tok):
            dlls.append(name)
    seen: set[str] = set()
    out: list[str] = []
    for d in dlls:
        if d not in seen:
            seen.add(d)
            out.append(d)
    return out


def _product_closure(ninja: Path, out_dir: Path) -> list[str]:
    closure = [_PRODUCT_EXE]
    closure.extend(_query_import_dlls(ninja, out_dir, _PRODUCT_EXE))
    return closure


def _ninja_dry_run_pending(
    ninja: Path, out_dir: Path, targets: list[str]
) -> list[str]:
    if not targets:
        return []
    proc = _run(ninja, out_dir, ["-n", *targets])
    text = (proc.stdout or "") + (proc.stderr or "")
    if proc.returncode != 0:
        return [f"ninja -n exit={proc.returncode}: {text.strip()[:400]}"]
    pending = [ln.strip() for ln in text.splitlines() if _WORK_LINE.match(ln)]
    return pending


def _require_file(path: Path, errors: list[str]) -> None:
    if not path.is_file():
        errors.append(f"missing: {path}")
        return
    if path.stat().st_size <= 0:
        errors.append(f"empty: {path}")


def _rva_to_offset(sections: list[tuple[int, int, int]], rva: int) -> int | None:
    for va, raw_ptr, raw_size in sections:
        if va <= rva < va + max(raw_size, 1):
            return raw_ptr + (rva - va)
    return None


def _pe_import_dlls(path: Path) -> list[str]:
    """Return DLL names from the PE import directory (no pefile dependency)."""
    data = path.read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError("not a PE image")
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    if e_lfanew + 24 >= len(data) or data[e_lfanew : e_lfanew + 4] != b"PE\0\0":
        raise ValueError("bad PE signature")
    coff = e_lfanew + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    if opt + opt_size > len(data):
        raise ValueError("truncated optional header")
    magic = struct.unpack_from("<H", data, opt)[0]
    if magic == 0x20B:  # PE32+
        dd_off = opt + 112
    elif magic == 0x10B:  # PE32
        dd_off = opt + 96
    else:
        raise ValueError(f"unsupported optional magic 0x{magic:x}")
    if dd_off + 16 > opt + opt_size:
        raise ValueError("no import data directory")
    import_rva = struct.unpack_from("<I", data, dd_off + 8)[0]
    if import_rva == 0:
        return []

    sec_off = opt + opt_size
    sections: list[tuple[int, int, int]] = []
    for i in range(num_sections):
        so = sec_off + i * 40
        if so + 40 > len(data):
            break
        va = struct.unpack_from("<I", data, so + 12)[0]
        raw_size = struct.unpack_from("<I", data, so + 16)[0]
        raw_ptr = struct.unpack_from("<I", data, so + 20)[0]
        sections.append((va, raw_ptr, raw_size))

    off = _rva_to_offset(sections, import_rva)
    if off is None:
        raise ValueError("import RVA not mapped")

    names: list[str] = []
    while off + 20 <= len(data):
        (
            _ilt,
            _stamp,
            _forwarder,
            name_rva,
            _iat,
        ) = struct.unpack_from("<5I", data, off)
        if name_rva == 0:
            break
        name_off = _rva_to_offset(sections, name_rva)
        if name_off is None or name_off >= len(data):
            break
        end = data.find(b"\0", name_off)
        if end < 0:
            break
        raw = data[name_off:end]
        try:
            names.append(raw.decode("ascii"))
        except UnicodeDecodeError:
            names.append(raw.decode("utf-8", errors="replace"))
        off += 20
    return names


def _is_system_dll(name: str) -> bool:
    low = name.lower()
    if not low.endswith(".dll"):
        return True
    stem = low[: -len(".dll")]
    for p in _SYSTEM_DLL_PREFIXES:
        if stem.startswith(p) or stem == p.rstrip("-"):
            return True
    # Bare Win32 names without path.
    if "\\" not in low and "/" not in low and stem in {
        "kernel32",
        "user32",
        "gdi32",
        "advapi32",
        "shell32",
        "ole32",
        "oleaut32",
        "combase",
        "ws2_32",
        "winmm",
        "opengl32",
        "d3d11",
        "d3d12",
        "dxgi",
        "dbghelp",
        "version",
        "shlwapi",
        "crypt32",
        "bcrypt",
        "setupapi",
        "imm32",
        "dwmapi",
        "uxtheme",
        "psapi",
        "iphlpapi",
        "secur32",
        "netapi32",
        "userenv",
        "wtsapi32",
        "ntdll",
        "rpcrt4",
        "sechost",
        "clbcatq",
        "comdlg32",
        "winspool",
        "glu32",
        "d3dcompiler_47",
        "mfplat",
        "mf",
        "mfreadwrite",
        "dxva2",
        "msacm32",
        "winhttp",
        "urlmon",
        "dnsapi",
        "mpr",
        "wldap32",
        "normaliz",
        "comctl32",
        "msimg32",
        "gdiplus",
        "shcore",
        "propsys",
        "wininet",
    }:
        return True
    return False


def _lock_holders(path: Path) -> list[str]:
    """Best-effort: Win32 processes whose ExecutablePath matches path."""
    try:
        ps = (
            f"$p = (Resolve-Path -LiteralPath '{path}').Path; "
            "Get-CimInstance Win32_Process | "
            "Where-Object { $_.ExecutablePath -and "
            "($_.ExecutablePath -ieq $p) } | "
            "ForEach-Object { '{0} {1}' -f $_.ProcessId, $_.Name }"
        )
        proc = subprocess.run(
            [
                "powershell",
                "-NoProfile",
                "-ExecutionPolicy",
                "Bypass",
                "-Command",
                ps,
            ],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=30,
            check=False,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return [f"(lock query failed: {exc})"]
    lines = [ln.strip() for ln in (proc.stdout or "").splitlines() if ln.strip()]
    return lines


def _check_writable(out_dir: Path, names: list[str], errors: list[str]) -> None:
    for name in names:
        path = out_dir / name
        if not path.is_file():
            continue
        try:
            with open(path, "r+b"):
                pass
        except OSError as exc:
            holders = _lock_holders(path)
            detail = (
                f" holders: {', '.join(holders)}"
                if holders
                else " (no matching Win32_Process ExecutablePath; close IDE/debugger?)"
            )
            errors.append(f"not writable (LNK1168 risk): {path}: {exc}{detail}")


def _check_pe_imports(
    out_dir: Path,
    exe_name: str,
    ninja_dlls: list[str],
    errors: list[str],
    warnings: list[str],
) -> None:
    exe = out_dir / exe_name
    if not exe.is_file():
        return
    try:
        imported = _pe_import_dlls(exe)
    except ValueError as exc:
        errors.append(f"PE parse failed for {exe_name}: {exc}")
        return

    ninja_set = {Path(d).name.lower() for d in ninja_dlls}
    out_dlls = {p.name.lower() for p in out_dir.glob("*.dll")}
    missing_local: list[str] = []
    for dll in imported:
        low = dll.lower()
        if _is_system_dll(low):
            continue
        # Product / third_party DLLs that ship beside the exe must exist.
        if low in out_dlls or (out_dir / dll).is_file():
            if low.endswith(".dll") and low not in ninja_set:
                # Present in out/ but not in ninja import graph — warn only
                # (plugins / delay-load / third_party copy).
                warnings.append(
                    f"PE imports {dll} (present in out/) but not in ninja "
                    f"closure of {exe_name}"
                )
            continue
        # Name looks like a project DLL (*_d.dll / known stems) → hard error.
        if low.endswith("_d.dll") or low in ninja_set:
            missing_local.append(dll)
        else:
            warnings.append(
                f"PE imports {dll} not found next to {exe_name} "
                f"(system/PATH?)"
            )
    if missing_local:
        errors.append(
            f"PE import missing beside {exe_name}: {', '.join(missing_local)}"
        )

    # L2 skew: any ninja import DLL newer than exe ⇒ half-link / LNK1168 residue.
    try:
        exe_mtime = exe.stat().st_mtime
    except OSError:
        return
    skew: list[str] = []
    for dll in ninja_dlls:
        dp = out_dir / dll
        if not dp.is_file():
            continue
        try:
            if dp.stat().st_mtime > exe_mtime + 1.0:
                skew.append(dll)
        except OSError:
            continue
    if skew:
        errors.append(
            f"DLL newer than {exe_name} (ABI/link skew): {', '.join(skew)}. "
            f"Kill lock holders if any, then: build.bat "
            f"{'debug' if 'Debug' in out_dir.parts else 'release'} {exe_name}"
        )


def check(
    root: Path,
    config: str,
    mode: str,
    *,
    skip_pe: bool = False,
    allow_missing_product: bool = False,
) -> int:
    out_dir = root / "out" / config
    errors: list[str] = []
    warnings: list[str] = []

    if not (out_dir / "build.ninja").is_file():
        print(
            f"ERROR: {out_dir / 'build.ninja'} missing -- run build.bat "
            f"{config.lower()} first",
            file=sys.stderr,
        )
        return 2

    ninja = _find_ninja(root)
    if ninja is None:
        print("ERROR: ninja.exe not found (build\\bin or PATH)", file=sys.stderr)
        return 2

    product = out_dir / _PRODUCT_EXE

    if mode == "writable":
        # L0 only — pre-ninja. Missing product is OK (first link).
        closure = _product_closure(ninja, out_dir)
        _check_writable(out_dir, closure, errors)
        for w in warnings:
            print(f"warn: {w}")
        if errors:
            print(
                f"ERROR: check_out_artifacts --out {config} --mode writable failed:"
            )
            for e in errors:
                print(f"  - {e}")
            print(
                "  Tip: stop SmartGIS.exe / harness, then rebuild. "
                "LNK1168 leaves new DLLs + old exe (ABI skew)."
            )
            return 1
        print(
            f"check_out_artifacts: OK out/{config} mode=writable "
            f"checked={sum(1 for n in closure if (out_dir / n).is_file())} "
            f"closure_names={len(closure)}"
        )
        return 0

    if mode in ("harness", "product", "te"):
        if not product.is_file():
            if allow_missing_product and mode == "product":
                print(
                    f"warn: {product} missing — skip product gate "
                    f"(partial target build)"
                )
                return 0
            errors.append(f"missing: {product}")

    closure: list[str] = []
    if mode in ("harness", "product"):
        closure = _product_closure(ninja, out_dir)
        for dll in closure[1:]:
            _require_file(out_dir / dll, errors)
        plugins = out_dir / "plugins"
        if not plugins.is_dir():
            errors.append(f"missing plugins dir: {plugins}")
        elif not any(plugins.iterdir()):
            errors.append(f"empty plugins dir: {plugins}")

    if mode == "te":
        closure = ["test_all"]

    # L1
    pending = _ninja_dry_run_pending(ninja, out_dir, closure)
    if pending:
        preview = "; ".join(pending[:8])
        more = "" if len(pending) <= 8 else f" (+{len(pending) - 8} more)"
        tip = "te" if mode == "te" else (
            "SmartGIS.exe" if mode == "product" else "harness"
        )
        errors.append(
            f"ninja closure stale for [{', '.join(closure)}]: "
            f"{len(pending)} pending -- {preview}{more}. "
            f"Rebuild: build.bat {config.lower()} {tip}"
        )

    if mode == "te" and not pending:
        required_exes: list[str] = []
        for name in TE_EXES:
            if not _is_ninja_target(ninja, out_dir, name):
                warnings.append(f"optional (not a ninja target): {name}")
                continue
            alone_pending = _ninja_dry_run_pending(ninja, out_dir, [name])
            if alone_pending:
                warnings.append(
                    f"optional (not in test_all closure): {name}"
                )
                continue
            required_exes.append(name)
            _require_file(out_dir / name, errors)
        if not required_exes:
            errors.append("no TE_EXES resolved inside test_all closure")

    # L2 — product / harness (skip when ninja already reported stale)
    if mode in ("harness", "product") and not skip_pe and not pending:
        ninja_dlls = closure[1:] if closure else []
        _check_pe_imports(out_dir, _PRODUCT_EXE, ninja_dlls, errors, warnings)

    for w in warnings:
        print(f"warn: {w}")
    if errors:
        print(f"ERROR: check_out_artifacts --out {config} --mode {mode} failed:")
        for e in errors:
            print(f"  - {e}")
        return 1

    dll_n = max(0, len(closure) - (1 if mode != "te" else 0))
    pe_note = ""
    if mode in ("harness", "product") and not skip_pe:
        pe_note = " pe=ok"
    print(
        f"check_out_artifacts: OK out/{config} mode={mode} "
        f"closure={','.join(closure[:6])}"
        f"{'...' if len(closure) > 6 else ''} "
        f"({dll_n} import dlls / dry-run clean){pe_note}"
    )
    return 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument(
        "--out",
        default="Debug",
        choices=("Debug", "Release"),
        help="Gen-root config under out/",
    )
    ap.add_argument(
        "--mode",
        required=True,
        choices=("writable", "harness", "te", "product"),
        help=(
            "writable: L0 pre-ninja; product/harness: L1+L2 SmartGIS closure; "
            "te: test_all + TE exes"
        ),
    )
    ap.add_argument(
        "--root",
        type=Path,
        default=None,
        help="Repo root (default: inferred from this script)",
    )
    ap.add_argument(
        "--skip-pe",
        action="store_true",
        help="Skip L2 PE import / mtime skew checks",
    )
    ap.add_argument(
        "--allow-missing-product",
        action="store_true",
        help="For --mode product: exit 0 if SmartGIS.exe is absent",
    )
    args = ap.parse_args(argv)
    root = (args.root or _repo_root()).resolve()
    return check(
        root,
        args.out,
        args.mode,
        skip_pe=args.skip_pe,
        allow_missing_product=args.allow_missing_product,
    )


if __name__ == "__main__":
    raise SystemExit(main())
