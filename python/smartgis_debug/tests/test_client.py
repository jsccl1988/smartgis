# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tests for smartgis AgentClient NDJSON framing and call() against a mock agent."""

from __future__ import annotations

import json
import socket
import sys
import threading
import unittest
from pathlib import Path
from typing import Any

# Mirror conftest path setup when running via unittest discover.
_ROOT = Path(__file__).resolve().parents[1]
_PYTHON = _ROOT.parent
for path in (str(_PYTHON), str(_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from smartgis.client import (  # noqa: E402
    AgentClient,
    AgentError,
    decode_line,
    encode_request,
    parse_agent_url,
)


class ParseAndFramingTests(unittest.TestCase):
    def test_parse_agent_url(self) -> None:
        self.assertEqual(parse_agent_url("tcp://127.0.0.1:9999"), ("127.0.0.1", 9999))
        self.assertEqual(parse_agent_url("127.0.0.1:8888"), ("127.0.0.1", 8888))
        with self.assertRaises(ValueError):
            parse_agent_url("tcp://127.0.0.1")

    def test_encode_decode_roundtrip(self) -> None:
        raw = encode_request(7, "ping", {})
        self.assertTrue(raw.endswith(b"\n"))
        obj = decode_line(raw)
        self.assertEqual(obj, {"id": 7, "method": "ping", "params": {}})


def _serve_one_call(
    listener: socket.socket,
    expected_method: str,
    result: dict[str, Any],
    ok: bool = True,
) -> None:
    conn, _ = listener.accept()
    with conn:
        file = conn.makefile("rwb", buffering=0)
        line = file.readline()
        req = json.loads(line.decode("utf-8"))
        assert req["method"] == expected_method
        if ok:
            resp = {"id": req["id"], "ok": True, "result": result}
        else:
            resp = {"id": req["id"], "ok": False, "error": "boom"}
        file.write((json.dumps(resp) + "\n").encode("utf-8"))
        file.flush()


class AgentClientTests(unittest.TestCase):
    def test_agent_client_call_ok(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        thread = threading.Thread(
            target=_serve_one_call,
            args=(listener, "ping", {"pong": True}),
            daemon=True,
        )
        thread.start()
        try:
            with AgentClient.connect(f"tcp://127.0.0.1:{port}") as client:
                out = client.call("ping")
                self.assertEqual(out, {"pong": True})
        finally:
            thread.join(timeout=2)
            listener.close()

    def test_agent_client_call_error(self) -> None:
        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        thread = threading.Thread(
            target=_serve_one_call,
            args=(listener, "cmd.exec", {}, False),
            daemon=True,
        )
        thread.start()
        try:
            with AgentClient.connect(f"tcp://127.0.0.1:{port}") as client:
                with self.assertRaisesRegex(AgentError, "boom"):
                    client.call("cmd.exec", {"line": ":help"})
        finally:
            thread.join(timeout=2)
            listener.close()


def _serve_register_and_job(listener: socket.socket) -> None:
    """Mock Agent: answer py.register, then send one py.job and expect a reply."""
    conn, _ = listener.accept()
    with conn:
        file = conn.makefile("rwb", buffering=0)
        reg = json.loads(file.readline().decode("utf-8"))
        assert reg["method"] == "py.register"
        file.write(
            (json.dumps({"id": reg["id"], "ok": True, "result": {}}) + "\n").encode(
                "utf-8"
            )
        )
        file.flush()
        job = {
            "id": 42,
            "method": "py.job",
            "params": {"id": "j1", "code": "print('hi')\n_result = 1 + 1\n"},
        }
        file.write((json.dumps(job) + "\n").encode("utf-8"))
        file.flush()
        reply = json.loads(file.readline().decode("utf-8"))
        assert reply["id"] == 42
        assert reply["ok"] is True
        assert "hi" in reply["result"]["stdout"]


class WorkerTests(unittest.TestCase):
    def test_worker_register_and_job(self) -> None:
        from smartgis_debug.worker import run_worker

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        thread = threading.Thread(
            target=_serve_register_and_job, args=(listener,), daemon=True
        )
        thread.start()
        try:
            rc = run_worker(f"tcp://127.0.0.1:{port}")
            self.assertEqual(rc, 0)
        finally:
            thread.join(timeout=5)
            listener.close()


if __name__ == "__main__":
    unittest.main()
