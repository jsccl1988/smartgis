# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""SmartGIS debug worker: connect to Agent, register, run py.job requests."""

from __future__ import annotations

import argparse
import io
import sys
import traceback
from concurrent.futures import ThreadPoolExecutor, TimeoutError as FuturesTimeout
from contextlib import redirect_stderr, redirect_stdout
from pathlib import Path
from typing import Any, Optional

# Allow ``python -m debug.worker`` with PYTHONPATH=tools.
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

# Default wall-clock limit per py.job (seconds). 0 / None disables.
DEFAULT_JOB_TIMEOUT_S = 60.0


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
    print(f"debug: debugpy listening on 127.0.0.1:{port}", file=sys.stderr)
    if wait:
        print("debug: waiting for debugger attach…", file=sys.stderr)
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


def _run_code_with_timeout(
    code: str,
    filename: str,
    timeout_s: Optional[float],
) -> dict[str, Any]:
    if timeout_s is None or timeout_s <= 0:
        return _run_code(code, filename=filename)
    with ThreadPoolExecutor(max_workers=1) as pool:
        future = pool.submit(_run_code, code, filename)
        try:
            return future.result(timeout=timeout_s)
        except FuturesTimeout:
            return {
                "stdout": "",
                "stderr": f"job timed out after {timeout_s}s\n",
                "error": f"job timed out after {timeout_s}s",
            }


def _resolve_job_path(path: str) -> Path | dict[str, Any]:
    """Return a readable file Path, or an error dict for the job reply."""
    try:
        resolved = Path(path).expanduser().resolve(strict=False)
    except OSError as exc:
        return {"ok": False, "error": f"cannot resolve path: {exc}"}
    if not resolved.exists():
        return {"ok": False, "error": f"path does not exist: {resolved}"}
    if not resolved.is_file():
        return {"ok": False, "error": f"path is not a file: {resolved}"}
    return resolved


def handle_job(
    params: dict[str, Any],
    *,
    timeout_s: Optional[float] = DEFAULT_JOB_TIMEOUT_S,
) -> dict[str, Any]:
    """Execute one ``py.job`` params object; returns worker-local reply dict."""
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
        resolved = _resolve_job_path(str(path))
        if isinstance(resolved, dict):
            return {"id": job_id, **resolved}
        try:
            text = resolved.read_text(encoding="utf-8")
        except OSError as exc:
            return {"id": job_id, "ok": False, "error": f"cannot read path: {exc}"}
        outcome = _run_code_with_timeout(text, filename=str(resolved), timeout_s=timeout_s)
    else:
        outcome = _run_code_with_timeout(str(code), filename="<py.job>", timeout_s=timeout_s)
    if "error" in outcome:
        return {
            "id": job_id,
            "ok": False,
            "error": outcome.get("error", "execution failed"),
            "result": outcome,
        }
    return {"id": job_id, "ok": True, "result": outcome}


# Back-compat alias used by older tests / imports.
_handle_job = handle_job


def run_worker(
    agent_url: str,
    debugpy_port: int | None = None,
    wait: bool = False,
    *,
    job_timeout_s: Optional[float] = DEFAULT_JOB_TIMEOUT_S,
) -> int:
    """Connect, register, and process py.job messages until the socket closes."""
    _maybe_start_debugpy(debugpy_port, wait=wait)
    try:
        client = AgentClient.connect(agent_url)
    except AgentError as exc:
        print(f"debug: connect failed: {exc}", file=sys.stderr)
        return 1
    smartgis_api.bind_client(client)
    try:
        client.call("py.register", {})
    except AgentError as exc:
        print(f"debug: py.register failed: {exc}", file=sys.stderr)
        client.close()
        return 1

    print(f"debug: registered with agent at {agent_url}", file=sys.stderr)

    try:
        while True:
            msg = client.read_raw()
            method = msg.get("method")
            if method == "py.job":
                params = msg.get("params") or {}
                if not isinstance(params, dict):
                    params = {}
                reply = handle_job(params, timeout_s=job_timeout_s)
                # Correlate with the incoming request id when present.
                if "id" in msg:
                    reply_out: dict[str, Any] = {
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
        print(f"debug: connection ended: {exc}", file=sys.stderr)
    finally:
        client.close()
        smartgis_api.unbind_client()
    return 0


def build_arg_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        prog="debug.worker",
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
    parser.add_argument(
        "--job-timeout",
        type=float,
        default=DEFAULT_JOB_TIMEOUT_S,
        metavar="SEC",
        help="Wall-clock timeout per py.job (0 disables; default %(default)s)",
    )
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_arg_parser()
    args = parser.parse_args(argv)
    timeout = args.job_timeout
    if timeout is not None and timeout <= 0:
        timeout = None
    return run_worker(
        args.agent,
        debugpy_port=args.debugpy,
        wait=args.wait_for_client,
        job_timeout_s=timeout,
    )


if __name__ == "__main__":
    raise SystemExit(main())
