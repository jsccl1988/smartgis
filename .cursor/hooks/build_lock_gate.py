# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""beforeShellExecution: forbid enabling compile lock; block bare gn/ninja if lock is on."""

from __future__ import annotations

import json
import os
import re
import sys
from datetime import datetime
from pathlib import Path

_BARE_NINJA = re.compile(
    r"\bninja(?:\.exe)?\b[^&\n|]*?"
    r"-C\s+[\"']?(?:\.[\\/])?out[\\/](?:Debug|Release)\b",
    re.IGNORECASE,
)
_BARE_GN_GEN = re.compile(
    r"\bgn(?:\.exe)?\b[^&\n|]*?\bgen\b[^&\n|]*?\bout[\\/](?:Debug|Release)\b",
    re.IGNORECASE,
)
_LOCK_OFF_ENV = re.compile(
    r"(?:"
    r"\$env:SMARTGIS_BUILD_LOCK\s*=\s*['\"]?(?:0|off|false|no)\b"
    r"|set(?:x)?\s+SMARTGIS_BUILD_LOCK\s*=\s*['\"]?(?:0|off|false|no)\b"
    r")",
    re.IGNORECASE,
)
_LOCK_ON_ENV = re.compile(
    r"(?:"
    r"\$env:SMARTGIS_BUILD_LOCK\s*=\s*['\"]?(?:1|on|true|yes)\b"
    r"|set(?:x)?\s+SMARTGIS_BUILD_LOCK\s*=\s*['\"]?(?:1|on|true|yes)\b"
    r")",
    re.IGNORECASE,
)
_CREATE_LOCK_ON = re.compile(
    r"(?:"
    r"New-Item[^\n;|&]{0,200}?\.build\.lock\.on"
    r"|Set-Content[^\n;|&]{0,200}?\.build\.lock\.on"
    r"|Out-File[^\n;|&]{0,200}?\.build\.lock\.on"
    r"|Add-Content[^\n;|&]{0,200}?\.build\.lock\.on"
    r"|echo\.?\s*>\s*[^\n;|&]*\.build\.lock\.on"
    r"|>>?\s*[\"']?[^\s;|&]*\.build\.lock\.on"
    r")",
    re.IGNORECASE,
)
_DELETE_LOCK_ON = re.compile(
    r"(?:"
    r"Remove-Item[^\n;|&]{0,200}?\.build\.lock\.on"
    r"|\bdel\b[^\n;|&]{0,120}?\.build\.lock\.on"
    r"|\berm\b[^\n;|&]{0,120}?\.build\.lock\.on"
    r")",
    re.IGNORECASE,
)
_MANUAL_LOCK_MUTEX = re.compile(
    r"(?:"
    r"\[(?:System\.)?IO\.File\]::Open[^\n;|&]{0,160}?\.build\.lock\.(?:debug|release|shared)\b"
    r"|New-Item[^\n;|&]{0,200}?\.build\.lock\.(?:debug|release|shared)\b"
    r"|Set-Content[^\n;|&]{0,200}?\.build\.lock\.(?:debug|release|shared)\b"
    r"|Out-File[^\n;|&]{0,200}?\.build\.lock\.(?:debug|release|shared)\b"
    r")",
    re.IGNORECASE,
)

_DENY_ENABLE_MSG = (
    "Compile lock enable is forbidden. Do not create out/.build.lock.on, "
    "do not set SMARTGIS_BUILD_LOCK=1/on/true/yes, and do not hand-hold "
    "out/.build.lock.debug|release|shared. Use direct build.bat; partition "
    "edits across non-overlapping paths."
)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _audit(decision: str, cmd: str, extra: str = "") -> None:
    try:
        log_dir = _repo_root() / "out" / "scratch"
        log_dir.mkdir(parents=True, exist_ok=True)
        line = (
            f"{datetime.now().isoformat(timespec='seconds')}\t{decision}\t"
            f"{cmd.replace(chr(10), ' ').replace(chr(13), ' ')[:240]}"
            f"{('\t' + extra) if extra else ''}\n"
        )
        with (log_dir / "build_lock_gate.log").open("a", encoding="utf-8") as f:
            f.write(line)
    except Exception:
        pass


def _load_payload() -> tuple[dict, bytes]:
    raw = sys.stdin.buffer.read()
    if not raw:
        return {}, raw
    text = None
    for enc in ("utf-8-sig", "utf-8", "utf-16", "utf-16-le", "cp936"):
        try:
            text = raw.decode(enc)
            break
        except Exception:
            continue
    if text is None:
        return {}, raw
    text = text.strip()
    if not text:
        return {}, raw
    try:
        data = json.loads(text)
        if isinstance(data, dict):
            return data, raw
    except Exception:
        pass
    # Best-effort: extract "command" field if present
    m = re.search(r'"command"\s*:\s*"((?:\\.|[^"\\])*)"', text)
    if m:
        cmd = bytes(m.group(1), "utf-8").decode("unicode_escape")
        return {"command": cmd}, raw
    return {}, raw


def _cwd_from(data: dict) -> Path | None:
    for key in ("working_directory", "cwd", "workdir"):
        raw = data.get(key)
        if isinstance(raw, str) and raw.strip():
            return Path(raw)
    return None


def _command_from(data: dict) -> str:
    cmd = data.get("command")
    if isinstance(cmd, str) and cmd:
        return cmd
    # Some builds nest under tool_input
    tool_input = data.get("tool_input")
    if isinstance(tool_input, dict):
        nested = tool_input.get("command")
        if isinstance(nested, str) and nested:
            return nested
    return ""


def _lock_enabled(cmd: str, data: dict) -> bool:
    if _LOCK_OFF_ENV.search(cmd):
        return False
    if _LOCK_ON_ENV.search(cmd):
        return True
    env_val = os.environ.get("SMARTGIS_BUILD_LOCK", "")
    if re.match(r"^(0|off|false|no)$", env_val or "", re.IGNORECASE):
        return False
    if re.match(r"^(1|on|true|yes)$", env_val or "", re.IGNORECASE):
        return True
    cwd = _cwd_from(data)
    candidates: list[Path] = []
    if cwd is not None:
        candidates.append(cwd)
        for parent in cwd.parents:
            candidates.append(parent)
            if parent == parent.anchor:
                break
    candidates.append(_repo_root())
    for root in candidates:
        if (root / "out" / ".build.lock.on").is_file():
            return True
    return False


def _deny(agent_message: str, user_message: str, cmd: str) -> int:
    _audit("deny", cmd)
    print(
        json.dumps(
            {
                "continue": True,
                "permission": "deny",
                "agent_message": agent_message,
                "user_message": user_message,
            }
        )
    )
    return 2


def _allow(cmd: str, note: str = "") -> int:
    _audit("allow", cmd, note)
    print(json.dumps({"continue": True, "permission": "allow"}))
    return 0


def _forbid_enable(cmd: str) -> str | None:
    if _LOCK_ON_ENV.search(cmd):
        return _DENY_ENABLE_MSG
    if _MANUAL_LOCK_MUTEX.search(cmd):
        return _DENY_ENABLE_MSG
    if _CREATE_LOCK_ON.search(cmd):
        if _DELETE_LOCK_ON.search(cmd) and not re.search(
            r"New-Item[^\n;|&]{0,200}?\.build\.lock\.on|echo\.?\s*>\s*[^\n;|&]*\.build\.lock\.on",
            cmd,
            re.IGNORECASE,
        ):
            return None
        return _DENY_ENABLE_MSG
    return None


def main() -> int:
    data, raw = _load_payload()
    cmd = _command_from(data)
    if not cmd and raw:
        # Dump raw for debugging; do not brick the agent on schema drift.
        try:
            dump = _repo_root() / "out" / "scratch" / "build_lock_gate_stdin.bin"
            dump.parent.mkdir(parents=True, exist_ok=True)
            dump.write_bytes(raw[:4096])
        except Exception:
            pass
        return _allow("", "no-command-parse")

    forbid = _forbid_enable(cmd)
    if forbid:
        return _deny(forbid, "Blocked enabling the compile lock (user policy).", cmd)

    if re.search(r"build\.bat", cmd, re.IGNORECASE):
        return _allow(cmd)
    if re.search(r"SMARTGIS_BUILD_LOCK_HELD\s*=", cmd, re.IGNORECASE):
        return _allow(cmd)
    if not _lock_enabled(cmd, data):
        return _allow(cmd)

    if _BARE_NINJA.search(cmd) or _BARE_GN_GEN.search(cmd):
        msg = (
            "Compile lock is ON; bare gn/ninja on out/Debug|Release is blocked. "
            "Use build.bat, or turn lock off (del out\\.build.lock.on / "
            "set SMARTGIS_BUILD_LOCK=0)."
        )
        return _deny(
            msg,
            "Blocked bare gn/ninja while compile lock is on; use build.bat.",
            cmd,
        )

    return _allow(cmd)


if __name__ == "__main__":
    raise SystemExit(main())
