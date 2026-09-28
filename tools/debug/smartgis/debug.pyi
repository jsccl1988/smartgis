# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Process-wide tracing for Diagnostic Tools (embed + worker stubs)."""

from typing import ContextManager

def set_tracing(on: bool) -> None: ...
def tracing_enabled() -> bool: ...
def trace_event(name: str, cat: str) -> ContextManager[object]: ...
