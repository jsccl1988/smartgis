# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""PE launch: argv strip, plugin.json startup, product env → switches."""

from __future__ import annotations

from pathlib import Path

from ..contract import Suite
from .plugin import apply_suite_plugin_startup, strip_retired_argv
from .process import is_views_exe, peel_product_switches


def views_cmd_and_env(
    suite: Suite, exe: Path, env: dict[str, str]
) -> tuple[list[str], dict[str, str]]:
    """Build Popen argv and env; mutates plugin.json when the image is Views.

    Retired CLI (--self-test / --harness / *showcase) is stripped only for the
    Views PE. GPU/render children still take --self-test as a real flag.
    """
    run_env = dict(env)
    # Debug CRT + 500ms sampler races (STATUS_HEAP_CORRUPTION). See
    # start_always_on_diagnostics in diagnostic_bootstrap.cc.
    run_env.setdefault("SMARTGIS_NO_ALWAYS_ON_DIAG", "1")
    if is_views_exe(suite.exe_name):
        cmd = [str(exe), *strip_retired_argv(suite.argv)]
        apply_suite_plugin_startup(suite, exe)
        run_env, extra = peel_product_switches(run_env)
        cmd.extend(extra)
        return cmd, run_env
    return [str(exe), *suite.argv], run_env
