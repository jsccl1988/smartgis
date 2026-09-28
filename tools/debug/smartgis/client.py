# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Thin NDJSON TCP client for the SmartGIS Debug Agent."""

from __future__ import annotations

import json
import select
import socket
from typing import Any, Callable, Mapping, Optional
from urllib.parse import urlparse


class AgentError(RuntimeError):
    """Raised when the Agent returns ok=false or the transport fails."""


EventHandler = Callable[[dict[str, Any]], None]


class AgentClient:
    """Newline-delimited JSON client talking to DebugAgent over loopback TCP.

    Request:  {"id": N, "method": "...", "params": {...}}
    Response: {"id": N, "ok": true, "result": {...}}
           or {"id": N, "ok": false, "error": "..."}
    Event:    {"event": "log", "entry": {...}}
    """

    def __init__(self, sock: socket.socket) -> None:
        self._sock = sock
        self._file = sock.makefile("rwb", buffering=0)
        self._next_id = 1
        self._event_handler: Optional[EventHandler] = None
        self._pending_events: list[dict[str, Any]] = []

    @classmethod
    def connect(cls, agent_url: str, timeout: float = 10.0) -> "AgentClient":
        """Connect to an Agent URL like ``tcp://127.0.0.1:PORT``."""
        host, port = parse_agent_url(agent_url)
        try:
            sock = socket.create_connection((host, port), timeout=timeout)
        except OSError as exc:
            raise AgentError(f"connect failed: {host}:{port}: {exc}") from exc
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

    def set_event_handler(self, handler: Optional[EventHandler]) -> None:
        """Optional callback invoked for server-push events during call/read."""
        self._event_handler = handler

    def send_raw(self, message: Mapping[str, Any]) -> None:
        """Write one NDJSON object (no response expected)."""
        line = json.dumps(message, ensure_ascii=False, separators=(",", ":"))
        try:
            self._file.write((line + "\n").encode("utf-8"))
            self._file.flush()
        except OSError as exc:
            raise AgentError(f"send failed: {exc}") from exc

    def read_raw(self) -> dict[str, Any]:
        """Read one NDJSON object from the socket."""
        try:
            raw = self._file.readline()
        except OSError as exc:
            raise AgentError(f"read failed: {exc}") from exc
        if not raw:
            raise AgentError("connection closed by peer")
        try:
            obj = json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise AgentError(f"malformed JSON line: {exc}") from exc
        if not isinstance(obj, dict):
            raise AgentError("expected JSON object")
        return obj

    def _dispatch_event(self, msg: dict[str, Any]) -> None:
        if self._event_handler is not None:
            self._event_handler(msg)
        else:
            self._pending_events.append(msg)

    def drain_events(self) -> list[dict[str, Any]]:
        """Return and clear buffered push events (when no handler is set)."""
        events = list(self._pending_events)
        self._pending_events.clear()
        return events

    def poll_event(self, timeout: float = 0.0) -> Optional[dict[str, Any]]:
        """Wait up to ``timeout`` seconds for one push event or return None."""
        if self._pending_events:
            return self._pending_events.pop(0)
        if timeout < 0:
            timeout = 0.0
        ready, _, _ = select.select([self._sock], [], [], timeout)
        if not ready:
            return None
        msg = self.read_raw()
        if "event" in msg and "id" not in msg:
            if self._event_handler is not None:
                self._event_handler(msg)
            return msg
        # Unexpected response / request — stash as synthetic pending for caller.
        self._pending_events.append(msg)
        return None

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
                self._dispatch_event(msg)
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

    def reconnect(self, agent_url: str, timeout: float = 10.0) -> "AgentClient":
        """Close this client and return a new connection to ``agent_url``."""
        self.close()
        return AgentClient.connect(agent_url, timeout=timeout)


def parse_agent_url(agent_url: str) -> tuple[str, int]:
    """Parse ``tcp://host:port`` into ``(host, port)``."""
    text = agent_url.strip()
    if not text:
        raise ValueError("empty agent URL")
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
