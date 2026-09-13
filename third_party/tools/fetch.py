#!/usr/bin/env python3
# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Fetch manifest packages into third_party/.src/.

Git packages land in `.src/<name>`. HTTP archives (`http_url`) extract to
`.src/<src_dir_name or name>/`. Offline: reuse an existing extract tree, or a
tarball/zip already under `.src/` (decoded or URL-encoded basename). For the
pinned protobuf-cpp-3.6.1 asset, `MOGU_PROTOBUF_CPP_ARCHIVE` may point at the
tar instead of downloading.

GitHub fetches: `MOGU_GITHUB_MIRROR` (e.g. https://ghproxy.net) prefixes
https://github.com/... git and HTTP URLs. GitHub tag/branch packages prefer a
resumable source archive (`.../archive/refs/tags/<ref>.tar.gz`) over `git
clone`, then fall back to clone. Clone retries use
`MOGU_GIT_CLONE_MAX_ATTEMPTS` (default 5) and `MOGU_GIT_CLONE_RETRY_BASE_SEC`
(default 8). Git HTTP is forced to HTTP/1.1 with a larger `http.postBuffer` to
avoid Debian/Ubuntu git+GnuTLS pack resets (`GnuTLS recv error (-110)` / `-9`).

After checkout/extract, applies optional unified diffs from
third_party/patches/<package>/*.patch (sorted by name) via `git apply`.

Packages with `"skip_fetch_all": true` are omitted from `--all` (Chromium
sparse pin). Fetch them with `--package <name>`.

Thin `third_party/<name>/BUILD.gn` wrappers are not source trees; clones
land in `.src/<name>`. An existing `.src` dir (including a junction to a
local checkout) is reused and never `rmtree`'d. `MGIS_TP_FORCE_FETCH=1`
unlinks a junction then reclones. In-tree vendored skip only when
`third_party/<name>/CMakeLists.txt` exists.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import gzip
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import tarfile
import time
import urllib.request
import zipfile
from pathlib import Path
from urllib.parse import unquote


class FetchError(Exception):
    """Raised when a git or HTTP fetch for one package fails."""

    pass


def _tp_root() -> Path:
    return Path(__file__).resolve().parent.parent


def _load_manifest(path: Path) -> dict:
    with path.open(encoding="utf-8") as f:
        return json.load(f)


def _github_url_with_optional_mirror(url: str) -> str:
    """Prefix github.com URLs when MOGU_GITHUB_MIRROR is set."""
    prefix = (os.environ.get("MOGU_GITHUB_MIRROR") or "").strip().rstrip("/")
    if not prefix:
        return url
    u = (url or "").strip()
    if u.startswith("https://github.com/") or u.startswith("http://github.com/"):
        return prefix + "/" + u
    return url


_GITHUB_MIRROR_FALLBACKS = ("https://ghproxy.net",)


def _github_url_candidates(url: str) -> list[str]:
    """Direct GitHub URL plus optional env mirror and documented fallbacks."""
    u = (url or "").strip()
    out: list[str] = []
    explicit = _github_url_with_optional_mirror(u)
    if explicit != u:
        out.append(explicit)
    if u:
        out.append(u)
    if u.startswith("https://github.com/") or u.startswith("http://github.com/"):
        for prefix in _GITHUB_MIRROR_FALLBACKS:
            cand = prefix.rstrip("/") + "/" + u
            if cand not in out:
                out.append(cand)
    seen: set[str] = set()
    uniq: list[str] = []
    for item in out:
        if item not in seen:
            seen.add(item)
            uniq.append(item)
    return uniq


def _git_http_config_args() -> list[str]:
    """Harden git HTTPS against Debian/Ubuntu git+GnuTLS resets on large packs."""
    return [
        "-c",
        "http.version=HTTP/1.1",
        "-c",
        "http.postBuffer=524288000",
        "-c",
        "core.compression=0",
        "-c",
        "core.longpaths=true",
    ]


def _git_clone_max_attempts() -> int:
    try:
        n = int(os.environ.get("MOGU_GIT_CLONE_MAX_ATTEMPTS") or "5")
    except ValueError:
        n = 5
    return max(1, n)


def _git_clone_retry_base_sec() -> float:
    try:
        n = float(os.environ.get("MOGU_GIT_CLONE_RETRY_BASE_SEC") or "8")
    except ValueError:
        n = 8.0
    return n


def _is_reparse_point(path: Path) -> bool:
    """True for symlinks and Windows junctions (do not follow into the target)."""
    try:
        if path.is_symlink():
            return True
        is_j = getattr(path, "is_junction", None)
        if callable(is_j) and is_j():
            return True
    except OSError:
        return False
    return False


def _rmtree_if_exists(path: Path) -> None:
    """Remove a dest dir. Junctions/symlinks are unlinked; targets are not deleted."""
    if _is_reparse_point(path):
        path.rmdir()
        return
    if path.exists():
        shutil.rmtree(path)


def _archive_complete(path: Path) -> bool:
    """True when dest looks like a finished .tar.gz (gzip stream not truncated)."""
    if not path.is_file() or path.stat().st_size <= 0:
        return False
    name = path.name.lower()
    if name.endswith(".tar.gz") or name.endswith(".tgz"):
        try:
            with gzip.open(path, "rb") as gz:
                while gz.read(1024 * 1024):
                    pass
            return True
        except (OSError, EOFError, gzip.BadGzipFile):
            return False
    return True


def _github_archive_url(git_url: str, ref: str) -> str | None:
    """GitHub auto-generated source tarball for a tag/branch (resumable HTTP)."""
    u = (git_url or "").strip().rstrip("/")
    if u.endswith(".git"):
        u = u[:-4]
    if not (u.startswith("https://github.com/") or u.startswith("http://github.com/")):
        return None
    ref = (ref or "").strip()
    if not ref:
        return None
    if _looks_like_commit_sha(ref):
        return f"{u}/archive/{ref}.tar.gz"
    return f"{u}/archive/refs/tags/{ref}.tar.gz"


def _download_url(url: str, dest: Path) -> None:
    """Download URL to dest; prefer curl so large GitHub archives can resume."""
    dest.parent.mkdir(parents=True, exist_ok=True)
    last_error: FetchError | None = None
    for cand in _github_url_candidates(url):
        for attempt in range(1, 8):
            try:
                _download_url_once(cand, dest)
            except FetchError as e:
                last_error = e
                print(f"[fetch] download failed: {e}")
                if dest.is_file() and dest.stat().st_size > 0:
                    continue
                break
            if _archive_complete(dest):
                return
            size = dest.stat().st_size if dest.is_file() else 0
            print(f"[fetch] truncated archive ({size} bytes); resume {attempt}/7")
            last_error = FetchError(f"truncated archive {dest}")
            time.sleep(min(30.0, 4.0 * attempt))
    if last_error is not None:
        raise last_error
    raise FetchError(f"incomplete download {dest}")


def _download_url_once(url: str, dest: Path, *, allow_full_restart: bool = True) -> None:
    curl = shutil.which("curl")
    if curl:
        resume = dest.is_file() and dest.stat().st_size > 0
        print(f"[fetch] curl {url} -> {dest}")
        cmd = [
            curl,
            "-fL",
            "--connect-timeout",
            "20",
            "--retry",
            "3",
            "--retry-delay",
            "8",
            "-o",
            str(dest),
            url,
        ]
        if resume:
            cmd[1:1] = ["-C", "-"]
        r = subprocess.run(cmd, check=False)
        if r.returncode == 33:
            if _archive_complete(dest):
                return
            if not allow_full_restart:
                raise FetchError(f"curl resume rejected (exit 33): {url}")
            print("[fetch] server rejects byte ranges; restarting full download")
            dest.unlink(missing_ok=True)
            _download_url_once(url, dest, allow_full_restart=False)
            return
        if r.returncode != 0:
            raise FetchError(f"curl download failed (exit {r.returncode}): {url}")
        return
    print(f"[fetch] download {url} -> {dest}")
    urllib.request.urlretrieve(url, dest)


def _fetch_github_archive(url: str, dest_root: Path, target: Path) -> None:
    cache = dest_root / _archive_basenames(url)[0]
    _download_url(url, cache)
    print(f"[fetch] extract {cache} -> {target}")
    tmp = dest_root / f".extract_{target.name}"
    _rmtree_if_exists(tmp)
    tmp.mkdir(parents=True, exist_ok=True)
    try:
        with tarfile.open(cache) as tf:
            tf.extractall(tmp)
        _place_http_extract(tmp, target)
    except (tarfile.TarError, EOFError, OSError) as e:
        raise FetchError(f"extract failed for {cache}: {e}") from e
    finally:
        _rmtree_if_exists(tmp)
    if not target.is_dir():
        raise FetchError(f"expected extract dir {target}")
    if (target / ".gitmodules").is_file() and not (target / ".git").exists():
        _run_git(["init"], cwd=target)


def _run_git(args: list[str], cwd: Path | None = None) -> None:
    r = subprocess.run(
        ["git", *_git_http_config_args(), *args], cwd=cwd, check=False
    )
    if r.returncode != 0:
        cmd = " ".join(args)
        raise FetchError(f"git {cmd!r} failed (exit {r.returncode})")


def _git_clone_with_retry(
    clone_args: list[str],
    target: Path,
    max_attempts: int | None = None,
) -> None:
    """Run `git clone ...` with backoff; wipe a partial dest between attempts."""
    if max_attempts is None:
        max_attempts = _git_clone_max_attempts()
    max_attempts = max(1, max_attempts)
    base_sleep = _git_clone_retry_base_sec()
    last_error: FetchError | None = None
    for attempt in range(1, max_attempts + 1):
        if attempt > 1:
            delay = min(120.0, base_sleep * (2 ** (attempt - 2)))
            print(f"[fetch] retry clone {attempt}/{max_attempts} after {delay:.0f}s")
            time.sleep(delay)
            _rmtree_if_exists(target)
        try:
            _run_git(clone_args)
            return
        except FetchError as e:
            last_error = e
            print(f"[fetch] clone attempt {attempt}/{max_attempts} failed: {e}")
            _rmtree_if_exists(target)
    assert last_error is not None
    raise last_error


def _looks_like_commit_sha(ref: str) -> bool:
    return bool(re.fullmatch(r"[0-9a-fA-F]{7,40}", ref))


def _checkout_git_ref(target: Path, ref: str, shallow: bool) -> None:
    depth = ["--depth", "1"] if shallow else []
    if _looks_like_commit_sha(ref):
        _run_git(["fetch", *depth, "origin", ref], cwd=target)
        _run_git(["checkout", "-f", ref], cwd=target)
        return
    _run_git(["fetch", "--tags", *depth, "origin"], cwd=target)
    _run_git(["checkout", "-f", ref], cwd=target)

def _maybe_submodules(pkg: dict, repo: Path) -> None:
    if not pkg.get("git_submodules"):
        return
    print(f"[fetch] submodules {pkg['name']}")
    _run_git(["submodule", "update", "--init", "--recursive"], cwd=repo)


def _apply_third_party_patches(tp_root: Path, package_name: str, repo: Path) -> None:
    patch_dir = tp_root / "patches" / package_name
    if not patch_dir.is_dir():
        return
    patches = sorted(p for p in patch_dir.iterdir() if p.suffix == ".patch" and p.is_file())
    if not patches:
        return
    print(f"[fetch] apply {len(patches)} patch file(s) to {package_name}")
    for pf in patches:
        rel = pf.relative_to(tp_root)
        already = subprocess.run(
            [
                "git",
                "apply",
                "--reverse",
                "--check",
                "--whitespace=nowarn",
                str(pf.resolve()),
            ],
            cwd=repo,
            check=False,
            capture_output=True,
        )
        if already.returncode == 0:
            print(f"[fetch]   {rel} (already applied)")
            continue
        print(f"[fetch]   {rel}")
        r = subprocess.run(
            ["git", "apply", "--whitespace=nowarn", str(pf.resolve())],
            cwd=repo,
            check=False,
        )
        if r.returncode != 0:
            raise FetchError(f"git apply failed for {pf.name} (repo={repo})")


def _archive_basenames(url: str) -> tuple[str, str]:
    """Return (decoded, raw) archive filenames from an http_url path."""
    raw = url.rsplit("/", 1)[-1].split("?")[0]
    return unquote(raw), raw


def _resolve_http_cache(pkg: dict, dest_root: Path) -> Path:
    """Pick a local archive: env override, existing .src file, or download."""
    url = pkg["http_url"]
    decoded, raw = _archive_basenames(url)
    proto_override = (os.environ.get("MOGU_PROTOBUF_CPP_ARCHIVE") or "").strip()
    if proto_override and "protobuf-cpp-3.6.1.tar.gz" in url:
        override = Path(proto_override)
        if not override.is_file():
            raise FetchError(
                f"MOGU_PROTOBUF_CPP_ARCHIVE is set but not a file: {proto_override}"
            )
        resolved = override.resolve()
        print(f"[fetch] using MOGU_PROTOBUF_CPP_ARCHIVE={resolved}")
        return resolved
    # Prefer decoded name (libtorch …2.1.2+cpu.zip); also accept a URL-encoded
    # file already sitting in .src/ from an earlier urlretrieve without unquote.
    for name in (decoded, raw):
        cand = dest_root / name
        if cand.is_file() and cand.stat().st_size > 0:
            print(f"[fetch] using pre-downloaded archive {cand}")
            return cand
    cache = dest_root / decoded
    dest_root.mkdir(parents=True, exist_ok=True)
    _download_url(url, cache)
    return cache


def _place_http_extract(extract_root: Path, target: Path) -> None:
    """Move an extracted zip tree onto dest_root/<src_dir_name>."""
    nested = extract_root / target.name
    if nested.is_dir():
        if target.exists():
            shutil.rmtree(target)
        shutil.move(str(nested), str(target))
        return
    kids = [
        p
        for p in extract_root.iterdir()
        if p.name not in (".", "..") and not p.name.startswith(".")
    ]
    if len(kids) == 1 and kids[0].is_dir():
        if target.exists():
            shutil.rmtree(target)
        shutil.move(str(kids[0]), str(target))
        return
    # Flat zip (antlr4-cpp-runtime-*-source.zip): CMakeLists.txt at archive root.
    if (extract_root / "CMakeLists.txt").is_file():
        target.mkdir(parents=True, exist_ok=True)
        for child in extract_root.iterdir():
            dest = target / child.name
            if dest.exists():
                if dest.is_dir():
                    shutil.rmtree(dest)
                else:
                    dest.unlink()
            shutil.move(str(child), str(dest))
        return
    raise FetchError(f"expected extract dir {target}")


def _extract_http_archive(cache: Path, dest_root: Path, target: Path) -> None:
    basename = cache.name
    lower = basename.lower()
    target.parent.mkdir(parents=True, exist_ok=True)
    if lower.endswith(".zip"):
        tmp = dest_root / f".extract_{target.name}"
        if tmp.exists():
            shutil.rmtree(tmp)
        tmp.mkdir(parents=True, exist_ok=True)
        try:
            with zipfile.ZipFile(cache) as zf:
                zf.extractall(tmp)
            _place_http_extract(tmp, target)
        finally:
            if tmp.exists():
                shutil.rmtree(tmp)
        return
    if ".tar." in lower or lower.endswith(".tgz"):
        with tarfile.open(cache) as tf:
            tf.extractall(dest_root)
        if not target.is_dir():
            raise FetchError(f"expected extract dir {target}")
        return
    raise FetchError(f"unsupported archive {basename}")


def _fetch_http(pkg: dict, dest_root: Path, tp_root: Path) -> None:
    name = pkg["name"]
    src_dir_name = pkg.get("src_dir_name") or name
    target = dest_root / src_dir_name
    if target.is_dir() and any(target.iterdir()):
        print(f"[fetch] http skip existing {target}")
        _apply_third_party_patches(tp_root, name, target)
        return

    cache = _resolve_http_cache(pkg, dest_root)
    if pkg.get("sha256"):
        digest = hashlib.sha256(cache.read_bytes()).hexdigest()
        if digest != pkg["sha256"]:
            raise FetchError(f"sha256 mismatch for {cache.name}: {digest}")

    print(f"[fetch] extract {cache} -> {target}")
    _extract_http_archive(cache, dest_root, target)
    if not target.is_dir():
        raise FetchError(f"expected extract dir {target}")
    _apply_third_party_patches(tp_root, name, target)


def _src_tree_ready(target: Path) -> bool:
    """True when `.src/<name>` already has sources (clone, extract, or junction)."""
    if not target.is_dir():
        return False
    try:
        if (target / ".git").exists():
            return True
        if (target / "CMakeLists.txt").is_file():
            return True
        for marker in (
            "include",
            "asio",
            "Eigen",
            "GL",
            "KHR",
            "CxImage",
            "httplib.h",
            "ed25519.c",
            "atlapp.h",
            "tiny_gltf.h",
            "include/assimp/Importer.hpp",
        ):
            if (target / marker).exists():
                return True
    except OSError:
        return False
    return False


def _package_src_dest(dest_root: Path, pkg: dict) -> Path:
    name = pkg.get("name") or ""
    src_dir_name = pkg.get("src_dir_name") or name
    aliased = dest_root / src_dir_name
    named = dest_root / name
    if aliased.is_dir():
        return aliased
    return named


def _vendored_dir(tp_root: Path, pkg: dict) -> Path | None:
    """Return third_party/<local_src or name> only for a full in-tree CMake checkout.

    Thin wrappers (`BUILD.gn` / README only) must still fetch into `.src/`.
    """
    name = pkg.get("local_src") or pkg.get("name") or ""
    skip = {
        "tools",
        "gn",
        "scripts",
        "patches",
        "doc",
        "bazel",
        "sysinc",
        "test",
        ".src",
        ".build",
        ".install",
    }
    if not name or name in skip:
        return None
    local = tp_root / name
    try:
        if not local.is_dir():
            return None
        if (local / "CMakeLists.txt").is_file():
            return local
    except OSError:
        return None
    return None


def fetch_one(tp_root: Path, dest_root: Path, pkg: dict, shallow: bool) -> None:
    force = bool((os.environ.get("MGIS_TP_FORCE_FETCH") or "").strip())
    dest = _package_src_dest(dest_root, pkg)
    if _src_tree_ready(dest) and not force:
        print(f"[fetch] reuse {pkg.get('name')}: {dest}")
        return
    if _vendored_dir(tp_root, pkg) is not None and not force:
        local = _vendored_dir(tp_root, pkg)
        print(f"[fetch] vendored skip {pkg.get('name')}: {local}")
        return
    if force and dest.exists():
        print(f"[fetch] force-clear {dest}")
        _rmtree_if_exists(dest)
    if (pkg.get("http_url") or "").strip():
        _fetch_http(pkg, dest_root, tp_root)
        return
    clone_one(tp_root, dest_root, pkg, shallow)


def _git_clone_urls(pkg: dict) -> list[str]:
    """Clone URL order: `git_url` first (Gitea pin), then `git_url_fallbacks`.

    GitHub pins still expand through `_github_url_candidates` (env mirror +
    ghproxy). Put a working host in `git_url` when the Gitea repo is empty.
    """
    primary = (pkg.get("git_url") or "").strip()
    out: list[str] = []
    if primary:
        for cand in _github_url_candidates(primary):
            if cand not in out:
                out.append(cand)
    for extra in pkg.get("git_url_fallbacks") or []:
        u = (extra or "").strip()
        if not u:
            continue
        for cand in _github_url_candidates(u):
            if cand not in out:
                out.append(cand)
    return out


def _github_archive_url_for_pkg(pkg: dict) -> str | None:
    ref = (pkg.get("git_ref") or "").strip()
    archive = _github_archive_url(pkg.get("git_url") or "", ref)
    if archive:
        return archive
    for extra in pkg.get("git_url_fallbacks") or []:
        archive = _github_archive_url(extra, ref)
        if archive:
            return archive
    return None


def clone_one(tp_root: Path, dest_root: Path, pkg: dict, shallow: bool) -> None:
    name = pkg["name"]
    ref = pkg["git_ref"]
    target = dest_root / name
    if _src_tree_ready(target) and not (target / ".git").exists():
        print(f"[fetch] reuse existing {target}")
        return
    if target.is_dir() and (target / ".git").exists():
        print(f"[fetch] update {name} at {target}")
        _checkout_git_ref(target, ref, shallow=True)
        _maybe_sparse_checkout(pkg, target)
        _maybe_submodules(pkg, target)
        _apply_third_party_patches(tp_root, name, target)
        return
    if target.exists():
        print(f"[fetch] removing incomplete {target}")
        _rmtree_if_exists(target)
    target.parent.mkdir(parents=True, exist_ok=True)
    depth = ["--depth", "1"] if shallow else []
    sparse_paths = pkg.get("git_sparse_paths")
    if sparse_paths:
        git_filter = (pkg.get("git_filter") or "blob:none").strip()
        print(f"[fetch] sparse clone {name} ({ref}) -> {target}")
        last_clone: FetchError | None = None
        for cand in _git_clone_urls(pkg):
            try:
                print(f"[fetch] sparse clone from {cand}")
                _git_clone_with_retry(
                    [
                        "clone",
                        *depth,
                        "--filter",
                        git_filter,
                        "--sparse",
                        cand,
                        str(target),
                    ],
                    target,
                    max_attempts=1,
                )
                last_clone = None
                break
            except FetchError as e:
                last_clone = e
                print(f"[fetch] sparse clone failed for {cand}: {e}")
                _rmtree_if_exists(target)
        if last_clone is not None:
            raise last_clone
        _checkout_git_ref(target, ref, shallow=shallow)
        _maybe_sparse_checkout(pkg, target)
    else:
        print(f"[fetch] clone {name} ({ref}) -> {target}")
        archive = _github_archive_url_for_pkg(pkg)
        cloned = False
        if archive:
            try:
                print(f"[fetch] GitHub archive {name} ({ref}) -> {target}")
                _fetch_github_archive(archive, dest_root, target)
                cloned = True
            except FetchError as archive_err:
                print(
                    f"[fetch] archive failed ({archive_err}); falling back to git clone"
                )
                _rmtree_if_exists(target)
        if not cloned:
            last_clone: FetchError | None = None
            for cand in _git_clone_urls(pkg):
                try:
                    print(f"[fetch] git clone from {cand}")
                    _git_clone_with_retry(
                        ["clone", *depth, "--branch", ref, cand, str(target)],
                        target,
                        max_attempts=1,
                    )
                    last_clone = None
                    cloned = True
                    break
                except FetchError as e:
                    last_clone = e
                    print(f"[fetch] git clone failed for {cand}: {e}")
                    _rmtree_if_exists(target)
                    try:
                        print(f"[fetch] retry clone without --branch {ref}")
                        _git_clone_with_retry(
                            ["clone", *depth, cand, str(target)], target, max_attempts=1
                        )
                        _checkout_git_ref(target, ref, shallow=shallow)
                        last_clone = None
                        cloned = True
                        break
                    except FetchError as e2:
                        last_clone = e2
                        print(f"[fetch] git clone failed for {cand}: {e2}")
                        _rmtree_if_exists(target)
            if not cloned and last_clone is not None:
                raise last_clone
    _maybe_submodules(pkg, target)
    _apply_third_party_patches(tp_root, name, target)


def _maybe_sparse_checkout(pkg: dict, repo: Path) -> None:
    paths = pkg.get("git_sparse_paths")
    if not paths:
        return
    print(f"[fetch] sparse-checkout {pkg['name']}: {paths}")
    _run_git(["sparse-checkout", "init", "--cone"], cwd=repo)
    _run_git(["sparse-checkout", "set", *paths], cwd=repo)


def main() -> None:
    root = _tp_root()
    default_manifest = root / "manifest.json"
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=default_manifest)
    parser.add_argument(
        "--dest",
        type=Path,
        default=None,
        help="destination root (default: <third_party>/.src)",
    )
    parser.add_argument("--package", action="append", default=[], help="only these names (repeatable)")
    parser.add_argument("--all", action="store_true", help="fetch all packages from manifest")
    parser.add_argument("--no-shallow", action="store_true", help="full git history")
    parser.add_argument(
        "-j",
        "--jobs",
        type=int,
        default=max(1, min(8, os.cpu_count() or 4)),
        metavar="N",
        help="parallel clone/update workers (default: min(8, CPU count), at least 1)",
    )
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be >= 1")

    data = _load_manifest(args.manifest)
    packages = data.get("packages") or []
    sub = data.get("default_src_subdir") or ".src"
    dest_root = args.dest if args.dest is not None else root / sub

    if not args.all and not args.package:
        parser.error("specify --all or one or more --package <name>")

    want = {p.lower() for p in args.package} if args.package else None
    shallow = not args.no_shallow

    to_fetch: list[dict] = []
    for pkg in packages:
        name = pkg.get("name")
        has_http = bool((pkg.get("http_url") or "").strip())
        has_git = bool(pkg.get("git_url")) and bool(pkg.get("git_ref"))
        if not name or (not has_http and not has_git):
            print(f"[fetch] skip invalid entry: {pkg!r}", file=sys.stderr)
            continue
        if want is not None and name.lower() not in want:
            continue
        if want is None and pkg.get("skip_fetch_all"):
            print(
                f"[fetch] skip {name} (skip_fetch_all; use --package {name})"
            )
            continue
        to_fetch.append(pkg)

    failures = 0
    if not to_fetch:
        print(f"[fetch] nothing to fetch. Sources under {dest_root}")
        return

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as ex:
        futures = [ex.submit(fetch_one, root, dest_root, pkg, shallow) for pkg in to_fetch]
        for fut in concurrent.futures.as_completed(futures):
            try:
                fut.result()
            except FetchError as e:
                print(f"[fetch] ERROR: {e}", file=sys.stderr)
                failures += 1
            except Exception as e:
                print(f"[fetch] ERROR: unexpected: {e}", file=sys.stderr)
                failures += 1

    if failures:
        print(f"[fetch] failed: {failures} package(s)", file=sys.stderr)
        sys.exit(1)

    print(f"[fetch] done. Sources under {dest_root}")


if __name__ == "__main__":
    main()
