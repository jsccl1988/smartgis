# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Tests for debug.worker job handling and session loop."""

from __future__ import annotations

import json
import socket
import sys
import tempfile
import threading
import unittest
from pathlib import Path

_ROOT = Path(__file__).resolve().parents[1]
_PYTHON = _ROOT.parent
for path in (str(_PYTHON), str(_ROOT)):
    if path not in sys.path:
        sys.path.insert(0, path)

from debug.worker import (  # noqa: E402
    build_arg_parser,
    handle_job,
    main,
    run_worker,
)
from tests.mock_agent import MockAgentServer, MockStep  # noqa: E402


class HandleJobUnitTests(unittest.TestCase):
    def test_code_ok(self) -> None:
        out = handle_job({"id": "j1", "code": "print('hi')\n_result = 2\n"}, timeout_s=5)
        self.assertTrue(out["ok"])
        self.assertIn("hi", out["result"]["stdout"])
        self.assertIn("2", out["result"]["value"])

    def test_code_and_path_conflict(self) -> None:
        out = handle_job({"id": "j", "code": "1", "path": "x.py"}, timeout_s=1)
        self.assertFalse(out["ok"])
        self.assertIn("either code or path", out["error"])

    def test_missing_payload(self) -> None:
        out = handle_job({"id": "j"}, timeout_s=1)
        self.assertFalse(out["ok"])
        self.assertIn("missing", out["error"])

    def test_path_missing(self) -> None:
        out = handle_job({"id": "j", "path": str(Path("no-such-file-xyz.py"))}, timeout_s=1)
        self.assertFalse(out["ok"])
        self.assertIn("does not exist", out["error"])

    def test_path_is_directory(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            out = handle_job({"id": "j", "path": tmp}, timeout_s=1)
            self.assertFalse(out["ok"])
            self.assertIn("not a file", out["error"])

    def test_path_file_ok(self) -> None:
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "job.py"
            path.write_text("print('from-file')\n", encoding="utf-8")
            out = handle_job({"id": "j", "path": str(path)}, timeout_s=5)
            self.assertTrue(out["ok"])
            self.assertIn("from-file", out["result"]["stdout"])

    def test_exec_error(self) -> None:
        out = handle_job({"id": "j", "code": "raise RuntimeError('x')\n"}, timeout_s=5)
        self.assertFalse(out["ok"])
        self.assertIn("RuntimeError", out["error"])

    def test_timeout(self) -> None:
        code = "import time\ntime.sleep(2)\n"
        out = handle_job({"id": "j", "code": code}, timeout_s=0.3)
        self.assertFalse(out["ok"])
        self.assertIn("timed out", out["error"])


class WorkerSessionTests(unittest.TestCase):
    def test_register_job_shutdown(self) -> None:
        def serve(listener: socket.socket) -> None:
            conn, _ = listener.accept()
            with conn:
                f = conn.makefile("rwb", buffering=0)
                reg = json.loads(f.readline().decode("utf-8"))
                self.assertEqual(reg["method"], "py.register")
                f.write(
                    (json.dumps({"id": reg["id"], "ok": True, "result": {}}) + "\n").encode()
                )
                f.flush()
                job = {
                    "id": 42,
                    "method": "py.job",
                    "params": {"id": "j1", "code": "print('hi')\n_result = 1 + 1\n"},
                }
                f.write((json.dumps(job) + "\n").encode())
                f.flush()
                reply = json.loads(f.readline().decode("utf-8"))
                self.assertEqual(reply["id"], 42)
                self.assertTrue(reply["ok"])
                self.assertIn("hi", reply["result"]["stdout"])
                f.write((json.dumps({"id": 99, "method": "shutdown", "params": {}}) + "\n").encode())
                f.flush()
                bye = json.loads(f.readline().decode("utf-8"))
                self.assertEqual(bye["id"], 99)
                self.assertTrue(bye["ok"])

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        thread = threading.Thread(target=serve, args=(listener,), daemon=True)
        thread.start()
        try:
            rc = run_worker(f"tcp://127.0.0.1:{port}", job_timeout_s=5)
            self.assertEqual(rc, 0)
        finally:
            thread.join(timeout=5)
            listener.close()

    def test_unknown_method(self) -> None:
        def serve(listener: socket.socket) -> None:
            conn, _ = listener.accept()
            with conn:
                f = conn.makefile("rwb", buffering=0)
                reg = json.loads(f.readline().decode("utf-8"))
                f.write(
                    (json.dumps({"id": reg["id"], "ok": True, "result": {}}) + "\n").encode()
                )
                f.flush()
                f.write(
                    (
                        json.dumps({"id": 7, "method": "nope", "params": {}}) + "\n"
                    ).encode()
                )
                f.flush()
                reply = json.loads(f.readline().decode("utf-8"))
                self.assertFalse(reply["ok"])
                self.assertIn("unknown method", reply["error"])
                f.write(
                    (json.dumps({"id": 8, "method": "shutdown", "params": {}}) + "\n").encode()
                )
                f.flush()
                f.readline()

        listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 0))
        listener.listen(1)
        port = listener.getsockname()[1]
        thread = threading.Thread(target=serve, args=(listener,), daemon=True)
        thread.start()
        try:
            self.assertEqual(run_worker(f"tcp://127.0.0.1:{port}", job_timeout_s=5), 0)
        finally:
            thread.join(timeout=5)
            listener.close()

    def test_register_failure(self) -> None:
        with MockAgentServer(
            [MockStep(expect_method="py.register", response={"ok": False, "error": "no"})]
        ) as server:
            self.assertEqual(run_worker(server.url, job_timeout_s=1), 1)

    def test_connect_failure(self) -> None:
        self.assertEqual(run_worker("tcp://127.0.0.1:1", job_timeout_s=1), 1)

    def test_arg_parser_requires_agent(self) -> None:
        parser = build_arg_parser()
        with self.assertRaises(SystemExit):
            parser.parse_args([])

    def test_main_missing_agent_exits(self) -> None:
        with self.assertRaises(SystemExit):
            main([])


if __name__ == "__main__":
    unittest.main()
