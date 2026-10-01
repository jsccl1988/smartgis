# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.
"""beforeShellExecution: block bare gn/ninja only when compile lock is enabled."""

from __future__ import annotations

import json
import os
import re
import sys
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
    r"SMARTGIS_BUILD_LOCK\s*=\s*(?:0|off|false|no)\b",
    re.IGNORECASE,
)
_LOCK_ON_ENV = re.compile(
    r"SMARTGIS_BUILD_LOCK\s*=\s*(?:1|on|true|yes)\b",
    re.IGNORECASE,
)


def _cwd_from(data: dict) -> Path | None:
    for key in ("working_directory", "cwd", "workdir"):
        raw = data.get(key)
        if isinstance(raw, str) and raw.strip():
            return Path(raw)
    return None


def _lock_enabled(cmd: str, data: dict) -> bool:
    """Compile lock defaults OFF; enable via env or out/.build.lock.on."""
    if _LOCK_OFF_ENV.search(cmd):
        return False
    if _LOCK_ON_ENV.search(cmd):
        return True
    env_val = os.environ.get("SMARTGIS_BUILD_LOCK", "")
    if re.match(r"^(?i)(0|off|false|no)$", env_val or ""):
        return False
    if re.match(r"^(?i)(1|on|true|yes)$", env_val or ""):
        return True
    cwd = _cwd_from(data)
    candidates: list[Path] = []
    if cwd is not None:
        candidates.append(cwd)
        for parent in cwd.parents:
            candidates.append(parent)
            if parent == parent.anchor:
                break
    for root in candidates:
        if (root / "build.bat").is_file() and (root / "out" / ".build.lock.on").is_file():
            return True
        if (root / "out" / ".build.lock.on").is_file():
            return True
    return False


def main() -> int:
    try:
        data = json.load(sys.stdin)
    except Exception:
        print(json.dumps({"permission": "allow"}))
        return 0

    cmd = data.get("command") or ""
    if re.search(r"build\.bat", cmd, re.IGNORECASE):
        print(json.dumps({"permission": "allow"}))
        return 0
    if re.search(r"SMARTGIS_BUILD_LOCK_HELD\s*=", cmd, re.IGNORECASE):
        print(json.dumps({"permission": "allow"}))
        return 0
    # Default: lock off → bare gn/ninja allowed.
    if not _lock_enabled(cmd, data):
        print(json.dumps({"permission": "allow"}))
        return 0

    if _BARE_NINJA.search(cmd) or _BARE_GN_GEN.search(cmd):
        msg = (
            "Compile lock is ON; bare gn/ninja on out/Debug|Release is blocked. "
            "Use build.bat, or turn lock off (del out\\.build.lock.on / "
            "set SMARTGIS_BUILD_LOCK=0). See .cursor/rules/build/build-lock.mdc."
        )
        print(
            json.dumps(
                {
                    "permission": "deny",
                    "agent_message": msg,
                    "user_message": "Blocked bare gn/ninja while compile lock is on; use build.bat.",
                }
            )
        )
        return 0

    print(json.dumps({"permission": "allow"}))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
