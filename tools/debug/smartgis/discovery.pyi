# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

from pathlib import Path
from typing import Any, Mapping, Optional

from smartgis.client import AgentClient

def discovery_path() -> Path: ...
def load_discovery(path: Optional[Path] = ...) -> dict[str, Any]: ...
def agent_url_from_discovery(
    data: Optional[Mapping[str, Any]] = ...,
    *,
    path: Optional[Path] = ...,
) -> str: ...
def connect_from_discovery(
    path: Optional[Path] = ...,
    *,
    timeout: float = ...,
) -> AgentClient: ...
