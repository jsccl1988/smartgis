# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""SmartGIS debug worker: connect to Agent, register, run py.job requests."""

from __future__ import annotations

import argparse
import io
import sys
import traceback
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from typing import Any

# Allow ``python -m smartgis_debug.worker`` with PYTHONPATH=python.
_PACKAGE_ROOT = Path(__file__).resolve().parent
_PYTHON_ROOT = _PACKAGE_ROOT.parent
if str(_PYTHON_ROOT) not in sys.path:
    sys.path.insert(0, str(_PYTHON_ROOT))
# Prefer package-local ``smartgis`` for stubs / client.
_LOCAL_SMARTGIS = str(_PACKAGE_ROOT)
if _LOCAL_SMARTGIS not in sys.path:
    sys.path.insert(0, _LOCAL_SMARTGIS)

from smartgis.client import AgentClient, AgentError  # noqa: E402
import smartgis as smartgis_api  # noqa: E402


def _maybe_start_debugpy(port: int | None, wait: bool) -> None:
    if port is None:
        return
    try:
        import debugpy
    except ImportError as exc:
        raise SystemExit(
            "debugpy is not installed; pip install debugpy or omit --debugpy"
        ) from exc
    debugpy.listen(("127.0.0.1", port))
    print(f"smartgis_debug: debugpy listening on 127.0.0.1:{port}", file=sys.stderr)
    if wait:
        print("smartgis_debug: waiting for debugger attach…", file=sys.stderr)
        debugpy.wait_for_client()


def _run_code(code: str, filename: str = "<py.job>") -> dict[str, Any]:
    stdout_buf = io.StringIO()
    stderr_buf = io.StringIO()
    value: Any = None
    ok = True
    error: str | None = None
    globals_dict: dict[str, Any] = {"__name__": "__main__", "smartgis": smartgis_api}
    try:
        with redirect_stdout(stdout_buf), redirect_stderr(stderr_buf):
            compiled = compile(code, filename, "exec")
            exec(compiled, globals_dict, globals_dict)
            if "_result" in globals_dict:
                value = globals_dict["_result"]
    except Exception:
        ok = False
        error = traceback.format_exc()
        stderr_buf.write(error)
    result: dict[str, Any] = {
        "stdout": stdout_buf.getvalue(),
        "stderr": stderr_buf.getvalue(),
    }
    if value is not None:
        result["value"] = repr(value)
    if not ok:
        result["error"] = error or "execution failed"
    return result


def _handle_job(params: dict[str, Any]) -> dict[str, Any]:
    job_id = params.get("id")
    code = params.get("code")
    path = params.get("path")
    if code is not None and path is not None:
        return {
            "id": job_id,
            "ok": False,
            "error": "provide either code or path, not both",
        }
    if code is None and path is None:
        return {"id": job_id, "ok": False, "error": "missing code or path"}
    if path is not None:
        try:
            text = Path(str(path)).read_text(encoding="utf-8")
        except OSError as exc:
            return {"id": job_id, "ok": False, "error": f"cannot read path: {exc}"}
        outcome = _run_code(text, filename=str(path))
    else:
        outcome = _run_code(str(code))
    if "error" in outcome:
        return {
            "id": job_id,
            "ok": False,
            "error": outcome.get("error", "execution failed"),
            "result": outcome,
        }
    return {"id": job_id, "ok": True, "result": outcome}


def run_worker(agent_url: str, debugpy_port: int | None = None, wait: bool = False) -> int:
    """Connect, register, and process py.job messages until the socket closes."""
    _maybe_start_debugpy(debugpy_port, wait=wait)
    client = AgentClient.connect(agent_url)
    smartgis_api.bind_client(client)
    try:
        client.call("py.register", {})
    except AgentError as exc:
        print(f"smartgis_debug: py.register failed: {exc}", file=sys.stderr)
        client.close()
        return 1

    print(f"smartgis_debug: registered with agent at {agent_url}", file=sys.stderr)

    try:
        while True:
            msg = client.read_raw()
            method = msg.get("method")
            if method == "py.job":
                params = msg.get("params") or {}
                if not isinstance(params, dict):
                    params = {}
                reply = _handle_job(params)
                # Correlate with the incoming request id when present.
                if "id" in msg:
                    reply_out = {
                        "id": msg["id"],
                        "ok": reply.get("ok", False),
                    }
                    if reply.get("ok"):
                        reply_out["result"] = {
                            "job_id": reply.get("id"),
                            **(reply.get("result") or {}),
                        }
                    else:
                        reply_out["error"] = reply.get("error", "job failed")
                        if reply.get("result") is not None:
                            reply_out["result"] = reply["result"]
                    client.send_raw(reply_out)
                else:
                    client.send_raw(
                        {
                            "event": "py.job.result",
                            **reply,
                        }
                    )
            elif method == "shutdown":
                req_id = msg.get("id")
                if req_id is not None:
                    client.send_raw({"id": req_id, "ok": True, "result": {}})
                break
            elif "event" in msg:
                continue
            elif "id" in msg and "ok" in msg:
                # Spurious response; ignore.
                continue
            else:
                req_id = msg.get("id")
                if req_id is not None:
                    client.send_raw(
                        {
                            "id": req_id,
                            "ok": False,
                            "error": f"unknown method: {method!r}",
                        }
                    )
    except AgentError as exc:
        print(f"smartgis_debug: connection ended: {exc}", file=sys.stderr)
    finally:
        client.close()
    return 0


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        prog="smartgis_debug.worker",
        description="Out-of-process SmartGIS debug Python worker",
    )
    parser.add_argument(
        "--agent",
        required=True,
        help="Agent URL, e.g. tcp://127.0.0.1:PORT",
    )
    parser.add_argument(
        "--debugpy",
        type=int,
        default=None,
        metavar="PORT",
        help="Listen for debugpy attach on 127.0.0.1:PORT",
    )
    parser.add_argument(
        "--wait-for-client",
        action="store_true",
        help="With --debugpy, block until a debugger attaches",
    )
    args = parser.parse_args(argv)
    return run_worker(args.agent, debugpy_port=args.debugpy, wait=args.wait_for_client)


if __name__ == "__main__":
    raise SystemExit(main())
