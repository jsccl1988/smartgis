# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Stable import path: plugin.json startup lives in loop.drive.plugin."""

from .drive.plugin import (  # noqa: F401
    apply_suite_plugin_startup,
    restore_product_plugin_startup,
    strip_retired_argv,
    suite_plugin_package,
)
