# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Label a DebugAgent connection with the Cursor conversation title."""

from __future__ import annotations

import json
import os
import sqlite3
from pathlib import Path


def composer_header_name(header_json: str) -> str:
    """Return the ``name`` field from a composerHeaders value, or empty."""
    try:
        data = json.loads(header_json)
    except json.JSONDecodeError:
        return ""
    if not isinstance(data, dict):
        return ""
    name = data.get("name")
    if not isinstance(name, str):
        return ""
    return name.strip()


def cursor_conversation_name() -> str:
    """Session label for this process.

    ``SMARTGIS_DEBUG_SESSION`` wins. Otherwise the title of the Cursor
    conversation identified by ``CURSOR_CONVERSATION_ID`` is read from the
    local composer header store. Empty means the server keeps ``agent-N``.
    """
    explicit = os.environ.get("SMARTGIS_DEBUG_SESSION", "").strip()
    if explicit:
        return explicit
    conversation_id = os.environ.get("CURSOR_CONVERSATION_ID", "").strip()
    appdata = os.environ.get("APPDATA", "").strip()
    if not conversation_id or not appdata:
        return ""
    db = Path(appdata) / "Cursor" / "User" / "globalStorage" / "state.vscdb"
    if not db.is_file():
        return ""
    try:
        uri = db.resolve().as_uri() + "?mode=ro&immutable=1"
        con = sqlite3.connect(uri, uri=True)
        try:
            row = con.execute(
                "SELECT value FROM composerHeaders WHERE composerId=?",
                (conversation_id,),
            ).fetchone()
        finally:
            con.close()
    except (OSError, sqlite3.Error):
        return ""
    if not row or not isinstance(row[0], str):
        return ""
    return composer_header_name(row[0])
