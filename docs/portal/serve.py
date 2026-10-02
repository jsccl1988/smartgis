# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""Local docs portal: auto-scan docs/ for .md + .html, serve preview UI.

Usage (repo root or docs/portal):
  python docs/portal/serve.py
  python docs/portal/serve.py --port 8765 --no-browser
"""

from __future__ import annotations

import argparse
import json
import mimetypes
import os
import socket
import sys
import threading
import time
import webbrowser
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, unquote, urlparse

DOCS_ROOT = Path(__file__).resolve().parents[1]
PORTAL_DIR = Path(__file__).resolve().parent
SKIP_DIR_NAMES = {".git", "__pycache__", "node_modules", ".vs"}
SCAN_SUFFIXES = {".md", ".html", ".htm"}


def _is_under(path: Path, root: Path) -> bool:
    try:
        path.resolve().relative_to(root.resolve())
        return True
    except ValueError:
        return False


def build_catalog(docs_root: Path) -> dict:
    """Walk docs/ and return a nested tree of markdown + HTML pages."""
    files: list[dict] = []
    for dirpath, dirnames, filenames in os.walk(docs_root):
        dirnames[:] = sorted(
            d for d in dirnames if d not in SKIP_DIR_NAMES and not d.startswith(".")
        )
        rel_dir = Path(dirpath).resolve().relative_to(docs_root.resolve())
        for name in sorted(filenames):
            suffix = Path(name).suffix.lower()
            if suffix not in SCAN_SUFFIXES:
                continue
            # Portal shell itself is the host UI, not a catalog entry.
            if rel_dir.parts[:1] == ("portal",) and name in {
                "index.html",
                "serve.py",
                "open.bat",
            }:
                continue
            rel = (rel_dir / name).as_posix()
            kind = "html" if suffix in {".html", ".htm"} else "md"
            files.append(
                {
                    "path": rel,
                    "name": name,
                    "kind": kind,
                    "dir": "" if str(rel_dir) == "." else rel_dir.as_posix(),
                }
            )

    def nest(entries: list[dict]) -> list[dict]:
        root_nodes: dict[str, dict] = {}

        def ensure_dir(parts: list[str]) -> dict:
            cursor_map = root_nodes
            node = None
            trail: list[str] = []
            for part in parts:
                trail.append(part)
                key = "/".join(trail)
                if key not in cursor_map:
                    cursor_map[key] = {
                        "type": "dir",
                        "name": part,
                        "path": key,
                        "children": {},
                    }
                node = cursor_map[key]
                cursor_map = node["children"]
            return node if node is not None else {"children": root_nodes}

        for entry in entries:
            parts = [p for p in entry["dir"].split("/") if p] if entry["dir"] else []
            parent = ensure_dir(parts) if parts else {"children": root_nodes}
            parent["children"][entry["path"]] = {
                "type": "file",
                "name": entry["name"],
                "path": entry["path"],
                "kind": entry["kind"],
            }

        def to_list(children: dict) -> list[dict]:
            dirs = []
            files_out = []
            for node in children.values():
                if node["type"] == "dir":
                    dirs.append(
                        {
                            "type": "dir",
                            "name": node["name"],
                            "path": node["path"],
                            "children": to_list(node["children"]),
                        }
                    )
                else:
                    files_out.append(
                        {
                            "type": "file",
                            "name": node["name"],
                            "path": node["path"],
                            "kind": node["kind"],
                        }
                    )
            dirs.sort(key=lambda n: n["name"].lower())
            files_out.sort(key=lambda n: n["name"].lower())
            return dirs + files_out

        return to_list(root_nodes)

    html_n = sum(1 for f in files if f["kind"] == "html")
    md_n = sum(1 for f in files if f["kind"] == "md")
    return {
        "root": "docs",
        "generated_at": int(time.time()),
        "counts": {"all": len(files), "md": md_n, "html": html_n},
        "files": files,
        "tree": nest(files),
    }


class DocsPortalHandler(BaseHTTPRequestHandler):
    server_version = "SmartGisDocsPortal/1.0"

    def log_message(self, fmt: str, *args) -> None:
        sys.stderr.write("%s - %s\n" % (self.address_string(), fmt % args))

    def _send(self, code: int, body: bytes, content_type: str, cache: bool = False) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        if not cache:
            self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _send_json(self, obj: object, code: int = 200) -> None:
        data = json.dumps(obj, ensure_ascii=False, indent=2).encode("utf-8")
        self._send(code, data, "application/json; charset=utf-8")

    def _safe_docs_path(self, rel: str) -> Path | None:
        rel = unquote(rel).lstrip("/").replace("\\", "/")
        if not rel or ".." in Path(rel).parts:
            return None
        target = (DOCS_ROOT / rel).resolve()
        if not _is_under(target, DOCS_ROOT):
            return None
        return target

    def do_GET(self) -> None:  # noqa: N802 — stdlib handler name
        parsed = urlparse(self.path)
        path = unquote(parsed.path)

        if path in {"/", "/portal", "/portal/"}:
            index = PORTAL_DIR / "index.html"
            body = index.read_bytes()
            self._send(200, body, "text/html; charset=utf-8")
            return

        if path == "/api/catalog":
            self._send_json(build_catalog(DOCS_ROOT))
            return

        if path == "/api/raw":
            qs = parse_qs(parsed.query)
            rel = (qs.get("path") or [""])[0]
            target = self._safe_docs_path(rel)
            if target is None or not target.is_file():
                self._send_json({"error": "not found", "path": rel}, 404)
                return
            data = target.read_bytes()
            ctype = "text/plain; charset=utf-8"
            if target.suffix.lower() in {".html", ".htm"}:
                ctype = "text/html; charset=utf-8"
            elif target.suffix.lower() == ".md":
                ctype = "text/markdown; charset=utf-8"
            self._send(200, data, ctype)
            return

        # Static file under docs/ (HTML diagrams, images, portal assets).
        rel = path.lstrip("/")
        if rel.startswith("portal/"):
            target = (PORTAL_DIR / rel[len("portal/") :]).resolve()
            if not _is_under(target, PORTAL_DIR) or not target.is_file():
                self._send(404, b"not found", "text/plain; charset=utf-8")
                return
        else:
            target = self._safe_docs_path(rel)
            if target is None or not target.is_file():
                self._send(404, b"not found", "text/plain; charset=utf-8")
                return

        ctype, _ = mimetypes.guess_type(str(target))
        if not ctype:
            ctype = "application/octet-stream"
        if target.suffix.lower() in {".md", ".html", ".htm", ".css", ".js", ".svg"}:
            if "charset" not in ctype and ctype.startswith("text/"):
                ctype = f"{ctype}; charset=utf-8"
            if target.suffix.lower() == ".md":
                ctype = "text/markdown; charset=utf-8"
            if target.suffix.lower() in {".html", ".htm"}:
                ctype = "text/html; charset=utf-8"
        self._send(200, target.read_bytes(), ctype, cache=False)


def _pick_port(preferred: int) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        try:
            s.bind(("127.0.0.1", preferred))
            return preferred
        except OSError:
            s.bind(("127.0.0.1", 0))
            return int(s.getsockname()[1])


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description="SmartGIS docs portal (MD + HTML)")
    parser.add_argument("--port", type=int, default=8765, help="preferred port")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--no-browser", action="store_true")
    args = parser.parse_args(argv)

    if not DOCS_ROOT.is_dir():
        print(f"docs root missing: {DOCS_ROOT}", file=sys.stderr)
        return 2

    port = _pick_port(args.port)
    httpd = ThreadingHTTPServer((args.host, port), DocsPortalHandler)
    url = f"http://{args.host}:{port}/"
    catalog = build_catalog(DOCS_ROOT)
    def _log(msg: str) -> None:
        print(msg, flush=True)

    _log("SmartGIS docs portal")
    _log(f"  root     {DOCS_ROOT}")
    _log(f"  url      {url}")
    _log(
        f"  catalog  {catalog['counts']['all']} files "
        f"({catalog['counts']['md']} md, {catalog['counts']['html']} html)"
    )
    _log("  Ctrl+C to stop")

    if not args.no_browser:
        threading.Timer(0.4, lambda: webbrowser.open(url)).start()

    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped")
    finally:
        httpd.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
