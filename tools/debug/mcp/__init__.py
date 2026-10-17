# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Stdio MCP server that exposes DebugAgent RPC as tools for Cursor SDK."""

from __future__ import annotations

__all__ = ["main"]


def main() -> None:
    from debug.mcp.server import run_stdio

    run_stdio()
