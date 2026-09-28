# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Edit-time stubs for smartgis.Host contribution surface."""

from typing import Any, Callable, Optional

class Host:
    def contribute_command(
        self,
        plugin_id: str,
        command_id: str,
        title: str,
        menu: str,
        fn: Callable[..., bool],
    ) -> bool: ...
    def contribute_dialog(
        self,
        plugin_id: str,
        dialog_id: str,
        title: str,
        fn: Callable[[], Any],
    ) -> bool: ...
    def contribute_dock(
        self,
        plugin_id: str,
        dock_id: str,
        title: str,
        area: str,
        fn: Callable[[], Any],
    ) -> bool: ...
    def contribute_processing(
        self,
        plugin_id: str,
        processing_id: str,
        title: str,
        fn: Callable[[str], bool],
    ) -> bool: ...
    def open_dialog(self, dialog_id: str) -> bool: ...
    def run_processing(
        self, processing_id: str, args_json: str = ...
    ) -> bool: ...
