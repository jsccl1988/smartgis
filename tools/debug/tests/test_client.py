# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tests for AgentClient NDJSON framing, events, and URL parsing."""

from __future__ import annotations

import json
import sys
import threading
import unittest
from pathlib import Path

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
from tests.mock_agent import MockAgentServer, MockStep  # noqa: E402

class ParseAndFramingTests(unittest.TestCase):
    def test_parse_agent_url(self) -> None:
        self.assertEqual(parse_agent_url("tcp://127.0.0.1:9999"), ("127.0.0.1", 9999))
        self.assertEqual(parse_agent_url("127.0.0.1:8888"), ("127.0.0.1", 8888))
        with self.assertRaises(ValueError):
            parse_agent_url("tcp://127.0.0.1")
        with self.assertRaises(ValueError):
            parse_agent_url("")
        with self.assertRaises(ValueError):
            parse_agent_url("http://127.0.0.1:9")

    def test_encode_decode_roundtrip(self) -> None:
        raw = encode_request(7, "ping", {})
        self.assertTrue(raw.endswith(b"\n"))
        obj = decode_line(raw)
        self.assertEqual(obj, {"id": 7, "method": "ping", "params": {}})

    def test_decode_rejects_empty_and_non_object(self) -> None:
        with self.assertRaises(ValueError):
            decode_line("   ")
        with self.assertRaises(ValueError):
            decode_line("[1,2]")


class AgentClientTests(unittest.TestCase):
    def test_agent_client_call_ok(self) -> None:
        with MockAgentServer(
            [MockStep(expect_method="ping", response={"ok": True, "result": {"pong": True}})]
        ) as server:
            with AgentClient.connect(server.url) as client:
                out = client.call("ping")
                self.assertEqual(out, {"pong": True})

    def test_agent_client_call_error(self) -> None:
        with MockAgentServer(
            [
                MockStep(
                    expect_method="cmd.exec",
                    response={"ok": False, "error": "boom"},
                )
            ]
        ) as server:
            with AgentClient.connect(server.url) as client:
                with self.assertRaisesRegex(AgentError, "boom"):
                    client.call("cmd.exec", {"line": ":help"})

    def test_call_skips_push_events(self) -> None:
        import socket

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        t = threading.Thread(target=_serve_event_then_ok, args=(listener,), daemon=True)
        t.start()
        try:
            with AgentClient.connect(f"tcp://127.0.0.1:{port}") as client:
                seen: list[dict] = []
                client.set_event_handler(seen.append)
                out = client.call("log.tail", {"n": 1})
                self.assertEqual(out, {"entries": []})
                self.assertEqual(len(seen), 1)
                self.assertEqual(seen[0]["event"], "log")
        finally:
            t.join(timeout=2)
            listener.close()

    def test_non_dict_result_wrapped(self) -> None:
        with MockAgentServer(
            [MockStep(expect_method="ping", response={"ok": True, "result": 42})]
        ) as server:
            with AgentClient.connect(server.url) as client:
                self.assertEqual(client.call("ping"), {"value": 42})

    def test_connect_failure(self) -> None:
        with self.assertRaises(AgentError):
            AgentClient.connect("tcp://127.0.0.1:1", timeout=0.2)

    def test_drain_events_without_handler(self) -> None:
        import socket

        def serve(listener: socket.socket) -> None:
            conn, _ = listener.accept()
            with conn:
                f = conn.makefile("rwb", buffering=0)
                line = f.readline()
                req = json.loads(line.decode("utf-8"))
                f.write((json.dumps({"event": "log", "entry": {"m": 1}}) + "\n").encode())
                f.flush()
                f.write(
                    (json.dumps({"id": req["id"], "ok": True, "result": {}}) + "\n").encode()
                )
                f.flush()

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        t = threading.Thread(target=serve, args=(listener,), daemon=True)
        t.start()
        try:
            with AgentClient.connect(f"tcp://127.0.0.1:{port}") as client:
                client.call("ping")
                events = client.drain_events()
                self.assertEqual(len(events), 1)
                self.assertEqual(events[0]["event"], "log")
        finally:
            t.join(timeout=2)
            listener.close()


def _serve_event_then_ok(listener: object) -> None:
    import socket

    assert isinstance(listener, socket.socket)
    conn, _ = listener.accept()
    with conn:
        f = conn.makefile("rwb", buffering=0)
        line = f.readline()
        req = json.loads(line.decode("utf-8"))
        f.write((json.dumps({"event": "log", "entry": {"message": "x"}}) + "\n").encode())
        f.flush()
        f.write(
            (json.dumps({"id": req["id"], "ok": True, "result": {"entries": []}}) + "\n").encode()
        )
        f.flush()


if __name__ == "__main__":
    unittest.main()
