# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Command execute / interactive tool activate."""

def execute(id: str, view_id: int = ..., payload: str = ...) -> bool: ...
def activate(tool_id: str, view_id: int = ...) -> bool: ...
