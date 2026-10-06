# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Stable import path: product PE helpers live in loop.drive.process."""

from .drive.process import *  # noqa: F403
from .drive.process import (  # noqa: F401
    is_product_switch_key,
    is_views_exe,
    kill_exe,
    merge_env,
    pe_launch_cwd,
    peel_product_switches,
    run_process,
    switch_flag,
)
