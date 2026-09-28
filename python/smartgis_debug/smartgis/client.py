# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Thin NDJSON TCP client for the SmartGIS Debug Agent."""

from __future__ import annotations

import json
import socket
from typing import Any, Mapping, Optional
from urllib.parse import urlparse


class AgentError(RuntimeError):
    """Raised when the Agent returns ok=false or the transport fails."""


class AgentClient:
    """Newline-delimited JSON client talking to DebugAgent over loopback TCP.

    Request:  {"id": N, "method": "...", "params": {...}}
    Response: {"id": N, "ok": true, "result": {...}}
           or {"id": N, "ok": false, "error": "..."}
    """

    def __init__(self, sock: socket.socket) -> None:
        self._sock = sock
        self._file = sock.makefile("rwb", buffering=0)
        self._next_id = 1

    @classmethod
    def connect(cls, agent_url: str, timeout: float = 10.0) -> "AgentClient":
        """Connect to an Agent URL like ``tcp://127.0.0.1:PORT``."""
        host, port = parse_agent_url(agent_url)
        sock = socket.create_connection((host, port), timeout=timeout)
        sock.settimeout(timeout)
        return cls(sock)

    def close(self) -> None:
        try:
            self._file.close()
        except OSError:
            pass
        try:
            self._sock.close()
        except OSError:
            pass

    def __enter__(self) -> "AgentClient":
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def send_raw(self, message: Mapping[str, Any]) -> None:
        """Write one NDJSON object (no response expected)."""
        line = json.dumps(message, ensure_ascii=False, separators=(",", ":"))
        self._file.write((line + "\n").encode("utf-8"))
        self._file.flush()

    def read_raw(self) -> dict[str, Any]:
        """Read one NDJSON object from the socket."""
        raw = self._file.readline()
        if not raw:
            raise AgentError("connection closed by peer")
        try:
            obj = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise AgentError(f"malformed JSON line: {exc}") from exc
        if not isinstance(obj, dict):
            raise AgentError("expected JSON object")
        return obj

    def call(
        self,
        method: str,
        params: Optional[Mapping[str, Any]] = None,
    ) -> dict[str, Any]:
        """Send a request and return the ``result`` dict on success."""
        req_id = self._next_id
        self._next_id += 1
        self.send_raw(
            {
                "id": req_id,
                "method": method,
                "params": dict(params) if params else {},
            }
        )
        while True:
            msg = self.read_raw()
            # Skip server push events (no matching id).
            if "event" in msg and "id" not in msg:
                continue
            if msg.get("id") != req_id:
                continue
            if not msg.get("ok", False):
                raise AgentError(str(msg.get("error", "unknown error")))
            result = msg.get("result", {})
            if result is None:
                return {}
            if not isinstance(result, dict):
                return {"value": result}
            return result


def parse_agent_url(agent_url: str) -> tuple[str, int]:
    """Parse ``tcp://host:port`` into ``(host, port)``."""
    text = agent_url.strip()
    if "://" not in text:
        text = "tcp://" + text
    parsed = urlparse(text)
    if parsed.scheme not in ("tcp", ""):
        raise ValueError(f"unsupported agent URL scheme: {parsed.scheme!r}")
    host = parsed.hostname or "127.0.0.1"
    if parsed.port is None:
        raise ValueError("agent URL must include a port")
    return host, int(parsed.port)


def encode_request(
    req_id: int,
    method: str,
    params: Optional[Mapping[str, Any]] = None,
) -> bytes:
    """Encode one NDJSON request line (for tests / framing helpers)."""
    payload = {
        "id": req_id,
        "method": method,
        "params": dict(params) if params else {},
    }
    return (json.dumps(payload, ensure_ascii=False, separators=(",", ":")) + "\n").encode(
        "utf-8"
    )


def decode_line(raw: bytes | str) -> dict[str, Any]:
    """Decode one NDJSON line into a dict."""
    if isinstance(raw, bytes):
        text = raw.decode("utf-8")
    else:
        text = raw
    text = text.strip()
    if not text:
        raise ValueError("empty line")
    obj = json.loads(text)
    if not isinstance(obj, dict):
        raise ValueError("expected JSON object")
    return obj
