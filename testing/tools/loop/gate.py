# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""Product runtime gate: GPU PE + Views --harness (replaces testing/e2e)."""

from __future__ import annotations

from .contract import load_suite
from .runner import run_suite

# Fast GPU device smoke, then the in-process Views GIS+chrome pipeline.
GATE_SUITE_IDS = ("gpu", "harness")


def run_product_gate(
    *,
    no_build: bool = True,
    rounds: int = 1,
    timeout_sec: int | None = None,
    config: str = "Debug",
) -> int:
    """Run GATE_SUITE_IDS in order. Returns the first non-zero suite rc."""
    for sid in GATE_SUITE_IDS:
        print(f"\n=== product gate: {sid} ===", flush=True)
        suite = load_suite(sid)
        rc = run_suite(
            suite,
            no_build=no_build,
            rounds=rounds,
            timeout_sec=timeout_sec,
            config=config,
        )
        if rc != 0:
            print(f"FAIL product gate at suite {sid} rc={rc}", flush=True)
            return rc
    print("PASS product gate (gpu + harness)", flush=True)
    return 0
