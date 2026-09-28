# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Scriptable mock DebugAgent for tools/debug pytest."""

from __future__ import annotations

import json
import socket
import threading
from dataclasses import dataclass, field
from typing import Any, Callable, Optional


Handler = Callable[[dict[str, Any]], Optional[dict[str, Any]]]


@dataclass
class MockStep:
    """One expected inbound request and the response / push to send next."""

    expect_method: Optional[str] = None
    response: Optional[dict[str, Any]] = None
    # If set, send these lines after handling the request (e.g. push events).
    extra_lines: list[dict[str, Any]] = field(default_factory=list)
    # Custom handler overrides response when provided.
    handler: Optional[Handler] = None


class MockAgentServer:
    """Bind 127.0.0.1:0 and serve a scripted NDJSON session on one connection."""

    def __init__(self, steps: Optional[list[MockStep]] = None) -> None:
        self._steps = list(steps or [])
        self._listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self._listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self._listener.bind(("127.0.0.1", 0))
        self._listener.listen(1)
        self.port = self._listener.getsockname()[1]
        self.url = f"tcp://127.0.0.1:{self.port}"
        self.requests: list[dict[str, Any]] = []
        self.error: Optional[BaseException] = None
        self._thread: Optional[threading.Thread] = None
        self._stop = threading.Event()

    def start(self) -> "MockAgentServer":
        self._thread = threading.Thread(target=self._serve, daemon=True)
        self._thread.start()
        return self

    def close(self) -> None:
        self._stop.set()
        try:
            self._listener.close()
        except OSError:
            pass
        if self._thread is not None:
            self._thread.join(timeout=5)

    def __enter__(self) -> "MockAgentServer":
        return self.start()

    def __exit__(self, *args: object) -> None:
        self.close()

    def _serve(self) -> None:
        try:
            self._listener.settimeout(5.0)
            conn, _ = self._listener.accept()
        except OSError as exc:
            self.error = exc
            return
        with conn:
            file = conn.makefile("rwb", buffering=0)
            step_i = 0
            while not self._stop.is_set():
                try:
                    line = file.readline()
                except OSError as exc:
                    self.error = exc
                    return
                if not line:
                    return
                try:
                    req = json.loads(line.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as exc:
                    self.error = exc
                    return
                self.requests.append(req)
                if step_i >= len(self._steps):
                    # Default echo-ok for unscripted calls.
                    resp = {
                        "id": req.get("id"),
                        "ok": True,
                        "result": {"echo_method": req.get("method")},
                    }
                    file.write((json.dumps(resp) + "\n").encode("utf-8"))
                    file.flush()
                    continue
                step = self._steps[step_i]
                step_i += 1
                if step.expect_method is not None:
                    assert req.get("method") == step.expect_method, (
                        f"expected {step.expect_method!r}, got {req.get('method')!r}"
                    )
                if step.handler is not None:
                    resp = step.handler(req)
                elif step.response is not None:
                    resp = dict(step.response)
                    if "id" not in resp and "id" in req:
                        resp["id"] = req["id"]
                else:
                    resp = {"id": req.get("id"), "ok": True, "result": {}}
                if resp is not None:
                    file.write((json.dumps(resp) + "\n").encode("utf-8"))
                    file.flush()
                for extra in step.extra_lines:
                    file.write((json.dumps(extra) + "\n").encode("utf-8"))
                    file.flush()


def serve_method_table(
    table: dict[str, Callable[[dict[str, Any]], dict[str, Any]]],
    *,
    max_calls: int = 32,
) -> MockAgentServer:
    """Mock that dispatches by method name until the client disconnects."""

    steps: list[MockStep] = []

    def make_handler(name: str) -> Handler:
        def _handler(req: dict[str, Any]) -> dict[str, Any]:
            fn = table[name]
            result = fn(req)
            if "ok" in result:
                return result
            return {"id": req.get("id"), "ok": True, "result": result}

        return _handler

    for _ in range(max_calls):
        # Dynamic: expect any method, dispatch in handler via last request.
        def handler(req: dict[str, Any], _table: dict = table) -> dict[str, Any]:
            method = str(req.get("method") or "")
            if method not in _table:
                return {
                    "id": req.get("id"),
                    "ok": False,
                    "error": f"unknown method: {method}",
                }
            out = _table[method](req)
            if "ok" in out:
                if "id" not in out:
                    out = {**out, "id": req.get("id")}
                return out
            return {"id": req.get("id"), "ok": True, "result": out}

        steps.append(MockStep(handler=handler))
    return MockAgentServer(steps)
