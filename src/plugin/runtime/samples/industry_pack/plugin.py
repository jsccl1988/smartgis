# Copyright (c) 2026 The Mogu Authors.
# All rights reserved.

"""L3 industry-pack skeleton: orchestration only.

Calls stable L1 processing ids (`dem.*`, `native.*`) through
`host.run_processing`. Does not implement kernels or replace builtin
reference UI. Builtin `kind=builtin` product plugins remain the
reference / performance fallback until explicitly withdrawn per plugin.
"""

PLUGIN_ID = "smartgis.sample_industry_pack"


def _run_tin_then_buffer(host, args):
    # Industry flow: assemble args, then chain published processing ids.
    # Real packs fill paths from pickers / docks; skeleton uses empty JSON
    # so the call shape is clear without requiring sample data on disk.
    _ = args
    tin_ok = host.run_processing(
        "dem.tin_from_xyz",
        '{"input":"","output":""}',
    )
    if not tin_ok:
        # Kernel / seam failure is expected without map + files; orchestration
        # still demonstrates the L3 pattern (call L1, do not reimplement it).
        return False
    return host.run_processing(
        "native.buffer",
        '{"input":"","output":"","distance":1.0}',
    )


def _run_buffer_only(host, args):
    _ = args
    return host.run_processing(
        "native.buffer",
        '{"input":"","output":"","distance":1.0}',
    )


def start(host):
    def tin_then_buffer(args):
        return _run_tin_then_buffer(host, args)

    def buffer_only(args):
        return _run_buffer_only(host, args)

    host.contribute_command(
        PLUGIN_ID,
        "industry.tin_then_buffer",
        "Industry: TIN then buffer",
        "tools",
        tin_then_buffer,
    )
    host.contribute_command(
        PLUGIN_ID,
        "industry.buffer_only",
        "Industry: buffer (native)",
        "tools",
        buffer_only,
    )


def stop():
    pass
